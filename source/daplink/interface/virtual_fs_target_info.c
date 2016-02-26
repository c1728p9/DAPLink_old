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
#include <string.h>

#include "virtual_fs.h"
#include "target_config.h"
#include "util.h"
#include "swd_host.h"

static uint32_t get_file_size(vfs_read_cb_t read_func, uint8_t * buffer);
static uint32_t read_file_rom_bin(uint32_t sector_offset, uint8_t* data, uint32_t num_sectors);
static uint32_t read_file_ram_bin(uint32_t sector_offset, uint8_t* data, uint32_t num_sectors);
static uint32_t read_file_regs_txt(uint32_t sector_offset, uint8_t* data, uint32_t num_sectors);

// Rebuild the virtual filesystem.  This must only be called
// when mass storage is inactive.
void vfs_target_info_build_filesystem(uint8_t scratch_buffer[VFS_SECTOR_SIZE])
{
    uint32_t file_size;

    //ram.bin
    file_size = target_device.ram_end - target_device.ram_start;
    vfs_create_file("RAM     BIN", read_file_ram_bin, 0, file_size);

    //rom.bin
    file_size = target_device.flash_end - target_device.flash_start;
    vfs_create_file("ROM     BIN", read_file_rom_bin, 0, file_size);

    //regs.txt
    file_size = get_file_size(read_file_regs_txt, scratch_buffer);
    vfs_create_file("REGS    TXT", read_file_regs_txt, 0, file_size);
}

bool vfs_target_info_enable(bool on)
{
    if (on) {
        // Halt the target
        // If target can't be halted then don't change modes
        if (!swd_init_debug()) {
            return false;
        }
        if (!swd_halt()) {
            return false;
        }
    } else {
        // retume the target
        if (!swd_resume()) {
            return false;
        }
    }
    return true;
}

// Get the filesize from a filesize callback.
// The file data must be null terminated for this to work correctly.
static uint32_t get_file_size(vfs_read_cb_t read_func, uint8_t scratch_buffer[VFS_SECTOR_SIZE])
{
    memset(scratch_buffer, 0, VFS_SECTOR_SIZE);
    // Determine size of the file by faking a read
    return read_func(0, scratch_buffer, 1);
}

static uint32_t read_file_rom_bin(uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    uint32_t address = target_device.flash_start + sector_offset * VFS_SECTOR_SIZE;
    uint32_t size = num_sectors * VFS_SECTOR_SIZE;
    uint32_t max_size = target_device.flash_end - address;

    // The entire read is out of range
    if (address >=target_device.flash_end) {
        return 0;
    }

    // Determine size to read
    size = MIN(size, max_size);

    if (!swd_read_memory(address, data, size)) {
        memset(data, 0, size);
    }
    return size;
}

static uint32_t read_file_ram_bin(uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    uint32_t address = target_device.ram_start + sector_offset * VFS_SECTOR_SIZE;
    uint32_t size = num_sectors * VFS_SECTOR_SIZE;
    uint32_t max_size = target_device.ram_end - address;

    // The entire read is out of range
    if (address >=target_device.ram_end) {
        return 0;
    }

    // Determine size to read
    size = MIN(size, max_size);

    if (!swd_read_memory(address, data, size)) {
        memset(data, 0, size);
    }
    return size;
}

static uint32_t read_file_regs_txt(uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    uint32_t i;
    uint32_t pos = 0;
    uint32_t val;
    char * buf = (char *)data;
    if (sector_offset != 0) {
        return 0;
    }
    for (i = 0; i < 16; i++) {
        pos += util_write_string(buf + pos, "r[");
        pos += util_write_uint32(buf + pos, i);
        pos += util_write_string(buf + pos, "]: 0x");
        if (swd_read_core_register(i, &val)) {
            pos += util_write_hex32(buf + pos, val);
        } else {
            pos += util_write_string(buf + pos, "xxxxxxxx");
        }
        pos += util_write_string(buf + pos, "\r\n");
    }
    return pos;
    
}
