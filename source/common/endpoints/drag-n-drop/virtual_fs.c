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

#include "virtual_fs.h"
#include "string.h"
#include "version.h"
#include "config_settings.h"
#include "compiler.h"

#include "daplink_debug.h"

#define MIN(a,b) ((a) < (b) ? a : b)
#define kB(x)   (x*1024)

// Virtual file system driver
// Limitations:
//   - files must be contiguous
//   - data written cannot be read back
//   - data should only be read once

// If sector size changes update comment below
COMPILER_ASSERT(0x0200 == VFS_SECTOR_SIZE);
static const mbr_t mbr_tmpl = {
    /*uint8_t[11]*/.boot_sector = {
        0xEB,0x3C, 0x90,
        'M','S','D','0','S','4','.','1' // OEM Name in text (8 chars max)
    },
    /*uint16_t*/.bytes_per_sector           = 0x0200,       // 512 bytes per sector
    /*uint8_t */.sectors_per_cluster        = 0x08,         // 4k cluser
    /*uint16_t*/.reserved_logical_sectors   = 0x0001,       // mbr is 1 sector
    /*uint8_t */.num_fats                   = 0x02,         // 2 FATs
    /*uint16_t*/.max_root_dir_entries       = 0x0020,       // 16 dir entries (max)
    /*uint16_t*/.total_logical_sectors      = 0x1f50,       // sector size * # of sectors = drive size
    /*uint8_t */.media_descriptor           = 0xf8,         // fixed disc = F8, removable = F0
    /*uint16_t*/.logical_sectors_per_fat    = 0x0001,       // FAT is 1k - ToDO:need to edit this
    /*uint16_t*/.physical_sectors_per_track = 0x0001,       // flat
    /*uint16_t*/.heads                      = 0x0001,       // flat
    /*uint32_t*/.hidden_sectors             = 0x00000000,   // before mbt, 0
    /*uint32_t*/.big_sectors_on_drive       = 0x00000000,   // 4k sector. not using large clusters
    /*uint8_t */.physical_drive_number      = 0x00,
    /*uint8_t */.not_used                   = 0x00,         // Current head. Linux tries to set this to 0x1
    /*uint8_t */.boot_record_signature      = 0x29,         // signature is present
    /*uint32_t*/.volume_id                  = 0x27021974,   // serial number
    // needs to match the root dir label
    /*char[11]*/.volume_label               = {'D','A','P','L','I','N','K','-','D','N','D'},
    // unused by msft - just a label (FAT, FAT12, FAT16)
    /*char[8] */.file_system_type           = {'F','A','T','1','6',' ',' ',' '},

    /* Executable boot code that starts the operating system */
    /*uint8_t[448]*/.bootstrap = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    },
    // Set signature to 0xAA55 to make drive bootable
    /*uint16_t*/.signature = 0x0000,
};

static const FatDirectoryEntry_t root_dir_entry = {
    /*uint8_t[11] */ .filename = {""},
    /*uint8_t */ .attributes = 0x28,
    /*uint8_t */ .reserved = 0x00,
    /*uint8_t */ .creation_time_ms = 0x00,
    /*uint16_t*/ .creation_time = 0x0000,
    /*uint16_t*/ .creation_date = 0x0000,
    /*uint16_t*/ .accessed_date = 0x0000,
    /*uint16_t*/ .first_cluster_high_16 = 0x0000,
    /*uint16_t*/ .modification_time = 0x8E41,
    /*uint16_t*/ .modification_date = 0x32bb,
    /*uint16_t*/ .first_cluster_low_16 = 0x0000,
    /*uint32_t*/ .filesize = 0x00000000
};

static uint32_t read_dflt(void* user_data, uint32_t offset, uint8_t* data, uint32_t size);
static void write_dflt(void* user_data, uint32_t offset, const uint8_t* data, uint32_t size);

static uint32_t read_mbr(void * user_data, uint32_t offset, uint8_t* data, uint32_t size);
static uint32_t read_fat(void * user_data, uint32_t offset, uint8_t* data, uint32_t size);
static uint32_t read_dir1(void * user_data, uint32_t offset, uint8_t* data, uint32_t size);
static void write_dir1(void * user_data, uint32_t offset, const uint8_t* data, uint32_t size);

static void write_fat(file_allocation_table_t * fat, uint32_t fat_idx, uint16_t val)
{
    fat->f[fat_idx * 2 + 0] = (val >> 0) & 0xFF;
    fat->f[fat_idx * 2 + 1] = (val >> 8) & 0xFF;
}

void vfs_init(vfs_fs_t *vfs, uint32_t disk_size, const vfs_filename_t drive_name)
{
    uint32_t i;
    mbr_t * mbr;
    uint32_t offset = 0;
    uint32_t num_clusters = 0;

    // Clear everything
    memset(vfs, 0, sizeof(vfs_fs_t));

    // Initialize MBR
    mbr = &vfs->mbr;
    memcpy(mbr, &mbr_tmpl, sizeof(mbr_t));
    mbr->total_logical_sectors = ((disk_size + kB(64)) / mbr->bytes_per_sector);
    // FAT table will likely be larger than needed, but this is allowed by the
    // fat specification
    num_clusters = mbr->total_logical_sectors / mbr->sectors_per_cluster;
    mbr->logical_sectors_per_fat = (num_clusters * 2 + VFS_SECTOR_SIZE - 1) / VFS_SECTOR_SIZE;

    // Initailize virtual media
    // NOTE - everything in the virtual media must be a multiple of the sector size
    for (i = 0; i < sizeof(vfs->vm) / sizeof(vfs->vm[0]); i++) {
        vfs->vm[i].read_func = read_dflt;
        vfs->vm[i].write_func = write_dflt;
        vfs->vm[i].length  = mbr->bytes_per_sector*(mbr->sectors_per_cluster);
    }
    vfs->vm_idx = 0;
    vfs->vm[vfs->vm_idx].read_func = read_mbr;
    vfs->vm[vfs->vm_idx].func_data = (void*)vfs;
    vfs->vm[vfs->vm_idx].length = VFS_SECTOR_SIZE; /* sizeof(mbr_t) */
    offset += vfs->vm[vfs->vm_idx].length;
    vfs->vm_idx++;
    vfs->vm[vfs->vm_idx].read_func = read_fat;
    vfs->vm[vfs->vm_idx].func_data = (void*)vfs;
    vfs->vm[vfs->vm_idx].length = VFS_SECTOR_SIZE * mbr->logical_sectors_per_fat;
    offset += vfs->vm[vfs->vm_idx].length;
    vfs->vm_idx++;
    vfs->vm[vfs->vm_idx].read_func = read_fat;
    vfs->vm[vfs->vm_idx].func_data = (void*)vfs;
    vfs->vm[vfs->vm_idx].length = VFS_SECTOR_SIZE * mbr->logical_sectors_per_fat;
    offset += vfs->vm[vfs->vm_idx].length;
    vfs->vm_idx++;
    vfs->vm[vfs->vm_idx].read_func = read_dir1;
    vfs->vm[vfs->vm_idx].write_func = write_dir1;
    vfs->vm[vfs->vm_idx].func_data = (void*)vfs;
    vfs->vm[vfs->vm_idx].length = VFS_SECTOR_SIZE;
    offset += vfs->vm[vfs->vm_idx].length;
    vfs->vm_idx++;
    vfs->vm[vfs->vm_idx].read_func = read_dflt; /* file_contents_dir2 */
    vfs->vm[vfs->vm_idx].func_data = (void*)vfs;
    vfs->vm[vfs->vm_idx].length = VFS_SECTOR_SIZE;
    offset += vfs->vm[vfs->vm_idx].length;
    vfs->vm_idx++;

    // Set the byte offset where data begins
    vfs->data_start = offset;

    // Initialize FAT
    memset(&vfs->fat, 0, sizeof(vfs->fat));
    vfs->fat_idx = 0;
    write_fat(&vfs->fat, vfs->fat_idx, 0xFFF8); // Media type
    vfs->fat_idx++;
    write_fat(&vfs->fat, vfs->fat_idx, 0xFFFF); // FAT12 - always 0xFFF, FAT16 - dirty/clean (clean = 0xFFFF)
    vfs->fat_idx++;

    // Initialize root dir
    vfs->dir_idx = 0;
    vfs->dir1.f[vfs->dir_idx] = root_dir_entry;
    memcpy(vfs->dir1.f[vfs->dir_idx].filename, drive_name, sizeof(vfs->dir1.f[0].filename));
    vfs->dir_idx++;
}

uint32_t vfs_get_total_size(vfs_fs_t *vfs)
{
    return vfs->mbr.bytes_per_sector * vfs->mbr.total_logical_sectors;
}

uint32_t vfs_cluster_to_sector(vfs_fs_t *vfs, uint32_t cluster_idx)
{
    uint32_t sectors_before_data = vfs->data_start / vfs->mbr.bytes_per_sector;
    return sectors_before_data + (cluster_idx - 2) * vfs->mbr.sectors_per_cluster;
}

void vfs_add_file(vfs_fs_t *vfs, const vfs_filename_t filename, vfs_file_config_t * file_config)
{
    uint8_t cluster_idx = vfs->fat_idx;
    FatDirectoryEntry_t * de;

    //TODO - handle files larger than 1 sector

    // Fill in fat table
    write_fat(&vfs->fat, vfs->fat_idx, 0xFFFF);
    vfs->fat_idx++;

    // Update directory entry
    de = &vfs->dir1.f[vfs->dir_idx];
    vfs->dir_idx++;
    memset(de, 0, sizeof(*de));
    memcpy(de->filename, filename, 11);
    de->attributes = file_config->attributes;
    de->creation_time_ms = file_config->creation_time_ms;
    de->creation_time = file_config->creation_time;
    de->creation_date = file_config->creation_date;
    de->accessed_date = file_config->accessed_date;
    de->modification_time = file_config->modification_time;
    de->modification_date = file_config->modification_date;
    de->filesize = file_config->filesize;
    de->first_cluster_high_16 = (cluster_idx >> 16) & 0xFFFF;
    de->first_cluster_low_16 = (cluster_idx >> 0) & 0xFFFF;

    // Update virtual media
    vfs->vm[vfs->vm_idx].read_func = file_config->read_func;
    vfs->vm[vfs->vm_idx].write_func = file_config->write_func;
    vfs->vm[vfs->vm_idx].func_data = file_config->func_user_data;
    vfs->vm[vfs->vm_idx].length = vfs->mbr.bytes_per_sector*(vfs->mbr.sectors_per_cluster);
    vfs->vm_idx++;

    vfs->file_count += 1;
}

void vfs_file_config_init(vfs_file_config_t * config)
{
    memset(config, 0, sizeof(vfs_file_config_t));
    /* config->filename[11]; */
    config->attributes = 0x01;
    config->creation_time_ms = 0x00;
    config->creation_time = 0x0000;
    config->creation_date = 0x0021;
    config->accessed_date = 0xbb32;
    config->modification_time = 0x83dc;
    config->modification_date = 0x34bb;
    config->read_func = read_dflt;
    config->write_func = write_dflt;
    config->func_user_data = 0;
}

void vfs_set_file_change_callback(vfs_fs_t *vfs, file_change_func_t cb, void* cb_data)
{
    vfs->change_func = cb;
    vfs->change_func_data = cb_data;
}

void vfs_read(vfs_fs_t * vfs, uint32_t requested_sector, uint8_t *buf, uint32_t num_sectors)
{
    uint8_t i = 0;
    uint32_t current_sector;

    // Zero out the buffer
    memset(buf, 0, num_sectors * VFS_SECTOR_SIZE);

    current_sector = 0;
    for (i = 0; i < sizeof(vfs->vm) / sizeof(vfs->vm[0]); i++) {
        uint32_t vm_sectors = vfs->vm[i].length / VFS_SECTOR_SIZE;
        uint32_t vm_start = current_sector;
        uint32_t vm_end = current_sector + vm_sectors;

        // Data can be used in this sector
        if ((requested_sector >= vm_start) && (requested_sector < vm_end)) {
            uint32_t sector_offset;
            uint32_t sectors_to_write = vm_end - requested_sector;
            sectors_to_write = MIN(sectors_to_write, num_sectors);
            sector_offset = requested_sector - current_sector;
            vfs->vm[i].read_func(vfs->vm[i].func_data, sector_offset, buf, sectors_to_write);
            // Update requested sector
            requested_sector += sectors_to_write;
            num_sectors -= sectors_to_write;
        }

        // If there is no more data to be read then break
        if (num_sectors == 0) {
            break;
        }

        // Move to the next virtual media entry
        current_sector += vm_sectors;
    }
}

void vfs_write(vfs_fs_t * vfs, uint32_t requested_sector, uint8_t *buf, uint32_t num_sectors)
{
    uint8_t i = 0;
    uint32_t current_sector;

    current_sector = 0;
    for (i = 0; i < vfs->vm_idx; i++) {
        uint32_t vm_sectors = vfs->vm[i].length / VFS_SECTOR_SIZE;
        uint32_t vm_start = current_sector;
        uint32_t vm_end = current_sector + vm_sectors;

        // Data can be used in this sector
        if ((requested_sector >= vm_start) && (requested_sector < vm_end)) {
            uint32_t sector_offset;
            uint32_t sectors_to_read = vm_end - requested_sector;
            sectors_to_read = MIN(sectors_to_read, num_sectors);
            sector_offset = requested_sector - current_sector;
            vfs->vm[i].write_func(vfs->vm[i].func_data, sector_offset, buf, sectors_to_read);
            // Update requested sector
            requested_sector += sectors_to_read;
            num_sectors -= sectors_to_read;
        }

        // If there is no more data to be read then break
        if (num_sectors == 0) {
            break;
        }

        // Move to the next virtual media entry
        current_sector += vm_sectors;
    }
}

static uint32_t read_dflt(void* user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    uint32_t read_size = VFS_SECTOR_SIZE * num_sectors;
    memset(data, 0, read_size);
    return read_size;
}

static void write_dflt(void* user_data, uint32_t sector_offset, const uint8_t* data, uint32_t num_sectors)
{
    // Do nothing
}

static uint32_t read_mbr(void * user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    vfs_fs_t * vfs = (vfs_fs_t *)user_data;
    uint32_t read_size = sizeof(mbr_t);
    COMPILER_ASSERT(sizeof(mbr_t) <= VFS_SECTOR_SIZE);
    if (sector_offset != 0) {
        // Don't worry about reading other sectors
        return 0;
    }
    memcpy(data, &vfs->mbr, read_size);
    return read_size;
}

/* No need to handle writes to the mbr */

static uint32_t read_fat(void * user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    vfs_fs_t * vfs = (vfs_fs_t *)user_data;
    uint32_t read_size = sizeof(file_allocation_table_t);
    COMPILER_ASSERT(sizeof(file_allocation_table_t) <= VFS_SECTOR_SIZE);
    if (sector_offset != 0) {
        // Don't worry about reading other sectors
        return 0;
    }
    memcpy(data, &vfs->fat, read_size);
    return read_size;
}

/* No need to handle writes to the fat */

static uint32_t read_dir1(void * user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors)
{
    vfs_fs_t * vfs = (vfs_fs_t *)user_data;
    uint32_t read_size = sizeof(root_dir_t);
    COMPILER_ASSERT(sizeof(root_dir_t) <= VFS_SECTOR_SIZE);
    if (sector_offset != 0) {
        // No data in other sectors
        return 0;
    }
    memcpy(data, &vfs->dir1, read_size);
    return read_size;
}

static bool filename_valid(vfs_filename_t filename)
{
    const char valid_char[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ _";
    uint32_t i, j;

    for (i = 0; i < sizeof(vfs_filename_t); i++) {
        bool valid = false;
        for (j = 0; j < sizeof(valid_char) - 1; j++) {
            if (filename[i] == valid_char[j]) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            return false;
        }
    }
    return true;
}

static void write_dir1(void * user_data, uint32_t sector_offset, const uint8_t* data, uint32_t num_sectors)
{
    root_dir_t * old_dir;
    root_dir_t * new_dir;
    uint32_t i;
    vfs_fs_t * vfs = (vfs_fs_t *)user_data;
    if (sector_offset != 0) {
        return;
    }

    old_dir = &vfs->dir1;
    new_dir = (root_dir_t *)data;

    // Start at index 1 to get past drive name
    for (i = 1; i < sizeof(old_dir->f) / sizeof(old_dir->f[0]); i++) {
        debug_msg("name:%*s\t attributes:%8d\t size:%8d\r\n", 11, new_dir->f[i].filename, new_dir->f[i].attributes, new_dir->f[i].filesize);
        bool same_name;
        if (0 == memcmp(&old_dir->f[i], &new_dir->f[i], sizeof(new_dir->f[i]))) {
            continue;
        }
        same_name = 0 == memcmp(old_dir->f[i].filename, new_dir->f[i].filename, sizeof(new_dir->f[i].filename));

        // Changed
        vfs->change_func(vfs->change_func_data, new_dir->f[i].filename, VFS_FILE_CHANGED, &old_dir->f[i], &new_dir->f[i]);

        // Deleted
        if (0xe5 == (uint8_t)new_dir->f[i].filename[0]) {
            vfs->change_func(vfs->change_func_data, old_dir->f[i].filename, VFS_FILE_DELETED, &old_dir->f[i], &new_dir->f[i]);
            continue;
        }

        // Created
        if (!same_name && filename_valid(new_dir->f[i].filename)) {
            vfs->change_func(vfs->change_func_data, new_dir->f[i].filename, VFS_FILE_CREATED, &old_dir->f[i], &new_dir->f[i]);
            continue;
        }
    }

    memcpy(old_dir, new_dir, VFS_SECTOR_SIZE);
}
