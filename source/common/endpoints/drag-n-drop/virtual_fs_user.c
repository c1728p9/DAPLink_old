/* CMSIS-DAP Interface Firmware
 * Copyright (c) 2009-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <ctype.h>

#include "main.h"
#include "RTL.h"
#include "rl_usb.h"
#include "virtual_fs.h"
#include "daplink_debug.h"
#include "validation.h"
#include "version.h"
#include "config_settings.h"
#include "daplink.h"
#include "target_config.h"
#include "target_flash.h"
#include "virtual_fs_user.h"

#define INVALID_SECTOR  0

typedef struct file_transfer_state {
    uint32_t start_sector;
    uint32_t amt_to_write;
    uint32_t amt_written;
    uint32_t next_sector_to_write;
    bool transfer_started;
    const FatDirectoryEntry_t * file_to_program_de;
    uint32_t transfer_finished;
    extension_t file_type;
} file_transfer_state_t;

// Must be bigger than 4x the flash size of the biggest supported
// device.  This is to accomidate for hex file programming.
static const uint32_t disc_size = MB(8);

static const char * const virtual_fs_auto_rstcfg = "AUTO_RSTCFG";
static const char * const virtual_fs_hard_rstcfg = "HARD_RSTCFG";

static const uint8_t mbed_redirect_file[512] =
    "<!doctype html>\r\n"
    "<!-- mbed Platform Website and Authentication Shortcut -->\r\n"
    "<html>\r\n"
    "<head>\r\n"
    "<meta charset=\"utf-8\">\r\n"
    "<title>mbed Website Shortcut</title>\r\n"
    "</head>\r\n"
    "<body>\r\n"
    "<script>\r\n"
    "window.location.replace(\"@R\");\r\n"
    "</script>\r\n"
    "</body>\r\n"
    "</html>\r\n";

static const uint8_t details_file[512] =
    "DAPLink Firmware - see https://mbed.com/daplink\r\n"
    "Version: " FW_BUILD "\r\n"
    "Build:   " __DATE__ " " __TIME__ "\r\n";

static const uint8_t hardware_rst_file[512] =
    "# Behavior configuration file\r\n"
    "# Reset can be hard or auto\r\n"
    "#     hard - user must disconnect power, press reset button or send a serial break command\r\n"
    "#     auto - upon programming completion the target MCU automatically resets\r\n"
    "#            and starts running\r\n"
    "#\r\n"
    "# The filename indicates how your board will reset the target\r\n"
    "# Delete this file to toggle the behavior\r\n"
    "# This setting can only be changed in maintenance mode\r\n";

static uint32_t usb_buffer[512/sizeof(uint32_t)];
static target_flash_status_t fail_reason = TARGET_OK;
static file_transfer_state_t file_transfer_state;
static vfs_fs_t vfs;

static void file_change_handler(void* user_data, const vfs_filename_t filename, vfs_file_change_t change,
                                const FatDirectoryEntry_t * old_entry, const FatDirectoryEntry_t * new_entry);
static void file_data_handler(uint32_t sector, const uint8_t *buf, uint32_t num_of_sectors);
static uint32_t get_file_size(read_funct_t read_func);
static uint32_t read_file_from_string(void* user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors);
static uint32_t read_mbed_redirect_file(void* user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors);
static void update_transfer_info(extension_t type, const FatDirectoryEntry_t * de, uint32_t sector, uint32_t size);
static void update_transfer_state(target_flash_status_t status);
static extension_t identify_file_name(const vfs_filename_t filename);
static extension_t identify_start_sequence(const uint8_t *buf);
static void start_disconnect_reconnect_timer(target_flash_status_t status);
static void tranfer_complete(target_flash_status_t status);
static void insert(uint8_t *buf, uint8_t* new_str, uint32_t strip_count);
static void update_html_file(uint8_t *buf, uint32_t bufsize);

// Rebuild the virtual filesystem.  This must only be called
// when mass storage is inactive.
void vfs_user_build_filesystem(void)
{
    static const file_transfer_state_t default_transfer_state = {0,0,0,0,INVALID_SECTOR,UNKNOWN};
    static vfs_file_config_t config;
    const char * name;
    const uint8_t * contents;
    bool auto_rst = config_get_auto_rst();

    // Update anything that could have changed file system state
    auto_rst = config_get_auto_rst();
    file_transfer_state = default_transfer_state;

    // Setup the filesystem based on target parameters
    vfs_init(&vfs, disc_size, daplink_drive_name);
    vfs_set_file_change_callback(&vfs, file_change_handler ,0);

    // MBED.HTM
    name = daplink_url_name;
    vfs_file_config_init(&config);
    config.read_func = read_mbed_redirect_file;
    // No user data needed for callback
    config.filesize = get_file_size(config.read_func);
    vfs_add_file(&vfs, name, &config);

    // DETAILS.TXT
    name = "DETAILS TXT";
    contents = details_file;
    vfs_file_config_init(&config);
    config.read_func = read_file_from_string;
    config.func_user_data = (void*)contents;
    config.filesize = strlen((const char*)contents);
    vfs_add_file(&vfs, name, &config);

    // HARD_RST.CFG / AUTO_RST.CFG
    name = auto_rst ? virtual_fs_auto_rstcfg : virtual_fs_hard_rstcfg;
    contents = hardware_rst_file;
    vfs_file_config_init(&config);
    config.read_func = read_file_from_string;
    config.func_user_data = (void*)contents;
    config.filesize = strlen((const char*)contents);
    config.attributes = 0x02; // Hidden
    vfs_add_file(&vfs, name, &config);

    // FAIL.TXT
    if (fail_reason != 0) {
        name = "FAIL    TXT";
        contents = (const uint8_t *)fail_txt_contents[fail_reason];
        vfs_file_config_init(&config);
        config.read_func = read_file_from_string;
        config.func_user_data = (void*)contents;
        config.filesize = strlen((const char*)contents);
        vfs_add_file(&vfs, name, &config);
    }

    // Set mass storage parameters
    USBD_MSC_MemorySize = vfs_get_total_size(&vfs);
    USBD_MSC_BlockSize  = VFS_SECTOR_SIZE;
    USBD_MSC_BlockGroup = 1;
    USBD_MSC_BlockCount = USBD_MSC_MemorySize / USBD_MSC_BlockSize;
    USBD_MSC_BlockBuf   = (uint8_t *)usb_buffer;
}

void usbd_msc_init(void)
{
    vfs_user_build_filesystem();
    USBD_MSC_MediaReady = __TRUE;
}

void usbd_msc_read_sect(uint32_t sector, uint8_t *buf, uint32_t num_of_sectors)
{
    // dont proceed if we're not ready
    if (!USBD_MSC_MediaReady) {
        return;
    }

    // indicate msc activity
    main_blink_msc_led(MAIN_LED_OFF);

    vfs_read(&vfs, sector, buf, num_of_sectors);
}

void usbd_msc_write_sect(uint32_t sector, uint8_t *buf, uint32_t num_of_sectors)
{
    if (!USBD_MSC_MediaReady) {
        return;
    }

    // Restart the disconnect counter on every packet
    // so the device does not detach in the middle of a
    // transfer.
    main_msc_delay_disconnect_event();

    if (file_transfer_state.transfer_finished) {
        debug_msg("%d: %s\r\n", __LINE__, "Transfer_finished - exit");
        return;
    }

    debug_msg("sector: %d\r\n", sector);

    // indicate msc activity
    main_blink_msc_led(MAIN_LED_OFF);

    vfs_write(&vfs, sector, buf, num_of_sectors);
    file_data_handler(sector, buf, num_of_sectors);
}


// Callback to handle changes to the root directory.  Should be used with vfs_set_file_change_callback
static void file_change_handler(void* user_data, const vfs_filename_t filename, vfs_file_change_t change,
                                const FatDirectoryEntry_t * entry, const FatDirectoryEntry_t * new_entry_data_ptr)
{
    if (VFS_FILE_CHANGED == change) {
        extension_t type;
        type = identify_file_name(filename);
        if (UNKNOWN != type) {
            uint32_t size;
            uint32_t sector;
            uint32_t cluster_idx;

            size = new_entry_data_ptr->filesize;
            cluster_idx = new_entry_data_ptr->first_cluster_low_16;
            // Cluster only valid if size > 0
            sector = size == 0 ? INVALID_SECTOR : vfs_cluster_to_sector(&vfs, cluster_idx);
            update_transfer_info(type, entry, sector, new_entry_data_ptr->first_cluster_low_16);
            update_transfer_state(TARGET_OK);
        }
    }

    if (VFS_FILE_CREATED == change) {
        if (!memcmp(filename, daplink_mode_file_name, sizeof(vfs_filename_t))) {
            if (daplink_is_interface()) {
                config_ram_set_hold_in_bl(true);
            } else {
                // Do nothing - bootloader will go to interface by default
            }
            tranfer_complete(TARGET_OK);
        }
    } else if (VFS_FILE_DELETED == change) {
        if (!memcmp(filename, virtual_fs_auto_rstcfg, sizeof(vfs_filename_t))) {
            config_set_auto_rst(false);
            tranfer_complete(TARGET_OK);
        } else if (!memcmp(filename, virtual_fs_hard_rstcfg, sizeof(vfs_filename_t))) {
            config_set_auto_rst(true);
            tranfer_complete(TARGET_OK);
        }
    }
}

// Handler for file data arriving over USB.  This function is responsible
// for detecting the start of a BIN/HEX file and performing programming
static void file_data_handler(uint32_t sector, const uint8_t *buf, uint32_t num_of_sectors)
{
    extension_t start_type_identified = UNKNOWN;
    target_flash_status_t status = TARGET_OK;

    // this is the key for starting a file write - we dont care what file types are sent
    //  just look for something unique (NVIC table, hex, srec, etc) until root dir is updated
    if (!file_transfer_state.transfer_started) {
        // look for file types we can program
        start_type_identified = identify_start_sequence(buf);
        if (start_type_identified != UNKNOWN) {
            debug_msg("%s", "FLASH INIT\r\n");

            file_transfer_state.amt_written = 0;
            file_transfer_state.transfer_started = 1;
            file_transfer_state.next_sector_to_write = sector;
            update_transfer_info(start_type_identified, 0, sector, 0);

            // Transfer must not be finished - can change from update_transfer_info
            if (file_transfer_state.transfer_finished) {
                return;
            }

            // prepare the target device
            status = target_flash_init(file_transfer_state.file_type);
            update_transfer_state(status);
        }
    }

    // Transfer must have been started for processing beyond this point
    if (!file_transfer_state.transfer_started) {
        return;
    }

    // Transfer must not be finished - can change from update_transfer_state
    if (file_transfer_state.transfer_finished) {
        return;
    }

    // Ignore sectors coming before this file
    if (sector < file_transfer_state.start_sector) {
        return;
    }

    // sectors must be in oder
    if (sector != file_transfer_state.next_sector_to_write) {
        debug_msg("%s", "SECTOR OUT OF ORDER\r\n");
        return;
    }

    debug_msg("%d: %s", __LINE__, "FLASH WRITE - ");
    status = target_flash_program_page((sector-file_transfer_state.start_sector)*VFS_SECTOR_SIZE, (uint8_t*)buf, VFS_SECTOR_SIZE*num_of_sectors);
    debug_msg("%d\r\n", status);
    debug_msg("Writing - start 0x%x, buf *%p=0x%x, size %i\r\n",(sector-file_transfer_state.start_sector)*VFS_SECTOR_SIZE, (uint8_t*)buf, ((uint8_t*)buf)[0], VFS_SECTOR_SIZE*num_of_sectors);
    // and do the housekeeping
    file_transfer_state.amt_written += VFS_SECTOR_SIZE;
    file_transfer_state.next_sector_to_write = sector + 1;

    // Update status
    update_transfer_state(status);
}

// Get the filesize from a filesize callback.
// The file data must be null terminated for this to work correctly.
static uint32_t get_file_size(read_funct_t read_func)
{
    static uint8_t sect_buf[512];

    // Determine size of the file by faking a read
    return read_func(0, 0, sect_buf, 1);
}

// File callback to be used with vfs_add_file to return file contents
// User data must be a null terminated string representing the file's contents
static uint32_t read_file_from_string(void* user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    const char * str = (const char *)user_data;
    uint32_t size = strlen(str);
    if (sector_offset != 0) {
        return 0;
    }
    memcpy(data, str, size);
    return size;
}

// File callback to be used with vfs_add_file to return file contents
// User data is unused.
static uint32_t read_mbed_redirect_file(void* user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    if (sector_offset != 0) {
        return 0;
    }
    update_html_file(data, VFS_SECTOR_SIZE);
    return strlen((const char *)data);
}

// Update info about the current file transfer and check for completion
// type     - Format of the file taht is being sent.  Currently BIN and HEX are supported.
// de       - Directory entry in the VFS of the file being written.  Set to null if unused.
// sector   - Starting sector of the tranfer.  Set to INVALID_SECTOR if unknown
// size     - Size of file being transfered.  Set to 0 if unknown
static void update_transfer_info(extension_t type, const FatDirectoryEntry_t * de, uint32_t sector, uint32_t size)
{
    target_flash_status_t status = TARGET_OK;

    // Initialize the type if it has not been set
    if (UNKNOWN == file_transfer_state.file_type) {
        file_transfer_state.file_type = type;
    }

    // Initialize the directory entry if it has not been set
    if (0 == file_transfer_state.file_to_program_de) {
        file_transfer_state.file_to_program_de = de;
    }

    // Initialize the starting sector if it has not been set
    if (INVALID_SECTOR == file_transfer_state.start_sector) {
        file_transfer_state.start_sector = sector;
    }

    // Check - Type must be the same
    if (type != file_transfer_state.file_type) {
        debug_msg("error: changed types during tranfer from %i to %i\n", type, file_transfer_state.file_type);
        status = TARGET_FAIL_ERROR_DURING_TRANSFER;
        goto end;
    }

    // Check - Directory entry must be the same (or null)
    if ((0 != de) && (de != file_transfer_state.file_to_program_de)) {
        debug_msg("error: entry != file_transfer_state.file_to_program_de: %p, %p\n", de, file_transfer_state.file_to_program_de);
        status = TARGET_FAIL_ERROR_DURING_TRANSFER;
        goto end;
    }

    // Check - File size must be the same or bigger (or 0)
    if ((0 != size) && (size < file_transfer_state.amt_to_write)) {
        debug_msg("error: file size changed from %i to %i\n", file_transfer_state.amt_to_write, size);
        status = TARGET_FAIL_ERROR_DURING_TRANSFER;
        goto end;
    }

    // Check - Starting sector must be the same
    if ((INVALID_SECTOR != sector) && (sector != file_transfer_state.start_sector)) {
        debug_msg("error: starting sector changed from %i to %i\n", file_transfer_state.start_sector, sector);
        status = TARGET_FAIL_ERROR_DURING_TRANSFER;
        goto end;
    }

    end:

    // Update values - Size is the only value that can change and it can only increase.
    file_transfer_state.amt_to_write = size;
    update_transfer_state(status);
}

// Check the current file transfer state and status and
// either finish the transfer or set the disconnect timer.
static void update_transfer_state(target_flash_status_t status)
{
    // An error occurred
    if ((TARGET_HEX_FILE_EOF != status) && (TARGET_OK != status)) {
        tranfer_complete(status);
        return;
    }

    // Hex file parsing has indicated the end of the file
    if (TARGET_HEX_FILE_EOF == status) {
        debug_msg("%s", "FLASH END\r\n");
        tranfer_complete(TARGET_OK);
        return;
    }

    // Check if the full size of the file has been written
    if (file_transfer_state.amt_written >= file_transfer_state.amt_to_write) {
        start_disconnect_reconnect_timer(TARGET_OK);
    } else {
        // If no more data comes in fail with the message
        // because an in progress tranfer timed out
        start_disconnect_reconnect_timer(TARGET_FAIL_TRANSFER_IN_PROGRESS);
    }
}

// Identify the file type from its extension
static extension_t identify_file_name(const vfs_filename_t filename)
{
    // 8.3 file names must be in upper case
    if (0 == strncmp("BIN", &filename[8], 3)) {
        return BIN;
    } else if (0 == strncmp("HEX", &filename[8], 3)) {
        return HEX;
    } else {
        return UNKNOWN;
    }
}

// Identify a file type from its contents
static extension_t identify_start_sequence(const uint8_t *buf)
{
    if (1 == validate_bin_nvic(buf)) {
        return BIN;
    }
    else if (1 == validate_hexfile(buf)) {
        return HEX;
    }
    return UNKNOWN;
}

// Start the usb disconnect timer.  If no more data arrives
// then fail with the given status.
static void start_disconnect_reconnect_timer(target_flash_status_t status)
{
    fail_reason = status;
    main_msc_disconnect_event();
}

// Complete the transfer with the given status.  After this
// call no more data received over USB will get processed
// until after reconnecting.
// Note - this function does not detach USB immediately.
//        It waits until usb is idle before disconnecting.
static void tranfer_complete(target_flash_status_t status)
{
    file_transfer_state.transfer_finished = 1;
    start_disconnect_reconnect_timer(status);
}

// Remove strip_count characters from the start of buf and then insert
// new_str at the new start of buf.
static void insert(uint8_t *buf, uint8_t* new_str, uint32_t strip_count)
{
    uint32_t buf_len = strlen((const char *)buf);
    uint32_t str_len = strlen((const char *)new_str);
    // push out string
    memmove(buf + str_len, buf + strip_count, buf_len - strip_count);
    // insert
    memcpy(buf, new_str, str_len);
}

// Fill buf with the contents of the mbed redirect file by
// expanding the special characters in mbed_redirect_file.
static void update_html_file(uint8_t *buf, uint32_t bufsize)
{
    uint32_t size_left;
    uint8_t *orig_buf = buf;
    uint8_t *insert_string;

    // Zero out buffer so strlen operations don't go out of bounds
    memset(buf, 0, bufsize);
    memcpy(buf, mbed_redirect_file, strlen((const char *)mbed_redirect_file));
    do {
        // Look for key or the end of the string
        while ((*buf != '@') && (*buf != 0)) buf++;

        // If key was found then replace it
        if ('@' == *buf) {
            switch(*(buf+1)) {
                case 'a':
                case 'A':   // platform ID string
                    insert_string = string_auth + 4;
                    break;

                case 'm':
                case 'M':   // MAC address
                    insert_string = (uint8_t *)mac_string;
                    break;

                case 'u':
                case 'U':   // UUID
                    insert_string = (uint8_t *)uuid_string;
                    break;

                case 'v':
                case 'V':   // Firmware version
                    insert_string = (uint8_t *)version_string;
                    break;

                case 'r':
                case 'R':   // URL replacement
                    insert_string = (uint8_t *)daplink_target_url;
                    break;

                default:
                    insert_string = (uint8_t *)"ERROR";
                    break;
            }
            insert(buf, insert_string, 2);
        }
    } while(*buf != '\0');
    size_left = buf - orig_buf;
    memset(buf, 0, bufsize - size_left);
}

