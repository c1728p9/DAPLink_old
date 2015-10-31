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
 
#ifndef VIRTUAL_FS_H
#define VIRTUAL_FS_H

#include <stdint.h>

#ifdef __cplusplus
  extern "C" {
#endif

#define VFS_SECTOR_SIZE      512
#define VFS_CLUSTER_SIZE    (4 * 1024)

typedef char vfs_filename_t[11];

typedef enum {
    VFS_FILE_CREATED = 0,   /*!< A new file was created */
    VFS_FILE_DELETED,       /*!< An existing file was deleted */
    VFS_FILE_CHANGED,       /*!< Some attribute of the file changed.
                                  Note: when a file is deleted or 
                                  created a file changed
                                  notification will also occur*/
} vfs_file_change_t;

typedef struct {
    uint8_t boot_sector[11];
    /* DOS 2.0 BPB - Bios Parameter Block, 11 bytes */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_logical_sectors;
    uint8_t  num_fats;
    uint16_t max_root_dir_entries;
    uint16_t total_logical_sectors;
    uint8_t  media_descriptor;
    uint16_t logical_sectors_per_fat;
    /* DOS 3.31 BPB - Bios Parameter Block, 12 bytes */
    uint16_t physical_sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t big_sectors_on_drive;
    /* Extended BIOS Parameter Block, 26 bytes */
    uint8_t  physical_drive_number;
    uint8_t  not_used;
    uint8_t  boot_record_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     file_system_type[8];
    /* bootstrap data in bytes 62-509 */
    uint8_t  bootstrap[448];
    /* These entries in place of bootstrap code are the *nix partitions */
    //uint8_t  partition_one[16];
    //uint8_t  partition_two[16];
    //uint8_t  partition_three[16];
    //uint8_t  partition_four[16];
    /* Mandatory value at bytes 510-511, must be 0xaa55 */
    uint16_t signature;
} __attribute__((packed)) mbr_t;


typedef struct file_allocation_table {
    uint8_t f[512];
} file_allocation_table_t;

typedef union FatDirectoryEntry {
    uint8_t data[32];
    struct {
        vfs_filename_t filename;
        uint8_t attributes;
        uint8_t reserved;
        uint8_t creation_time_ms;
        uint16_t creation_time;
        uint16_t creation_date;
        uint16_t accessed_date;
        uint16_t first_cluster_high_16;
        uint16_t modification_time;
        uint16_t modification_date;
        uint16_t first_cluster_low_16;
        uint32_t filesize;
    } __attribute__((packed)) ;
} __attribute__((packed)) FatDirectoryEntry_t;

// to save RAM all files must be in the first root dir entry (512 bytes)
//  but 2 actually exist on disc (32 entries) to accomodate hidden OS files,
//  folders and metadata 
typedef struct root_dir {
    FatDirectoryEntry_t f[16];
} root_dir_t;

// Callback for when data is written to a file on the virtual filesystem
typedef void (*write_funct_t)(void* user_data, uint32_t sector_offset, const uint8_t* data, uint32_t num_sectors);
// Callback for when data is ready from the virtual filesystem
typedef uint32_t (*read_funct_t)(void* user_data, uint32_t sector_offset, uint8_t* data, uint32_t num_sectors);
// Callback for when a file's attributes are changed on the virtual filesystem.  Note that the 'entry' parameter
// can be saved and compared to other directory entry pointers to see if they are referencing the same object.  The
// same cannot be done with new_entry_data_ptr since it points to a temporary buffer.
typedef void (*file_change_func_t)(void* user_data, const vfs_filename_t filename, vfs_file_change_t change,
                                   const FatDirectoryEntry_t * entry, const FatDirectoryEntry_t * new_entry_data_ptr);

typedef struct virtual_media {
    read_funct_t read_func;
    write_funct_t write_func;
    void * func_data;
    uint32_t length;
} virtual_media_t;

typedef struct {
    mbr_t mbr;
    file_allocation_table_t fat;
    virtual_media_t vm[16];
    uint32_t media_size;
    uint8_t file_count;
    root_dir_t dir1;
    file_change_func_t change_func;
    void * change_func_data;
    uint32_t vm_idx;
    uint32_t fat_idx;
    uint32_t dir_idx;
    uint32_t data_start;
} vfs_fs_t;

typedef struct {
    uint8_t attributes;
    uint8_t creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t accessed_date;
    uint16_t modification_time;
    uint16_t modification_date;
    read_funct_t read_func;
    write_funct_t write_func;
    void * func_user_data;
    uint32_t filesize;
} vfs_file_config_t;

// Initialize the filesystem with the given size and name
void vfs_init(vfs_fs_t *vfs, uint32_t disk_size, const vfs_filename_t drive_name);
// Get the total size of the virtual filesystem
uint32_t vfs_get_total_size(vfs_fs_t *vfs);
// Initialize the file configuration structure.
void vfs_file_config_init(vfs_file_config_t * config);
// Add a file to the virtual FS.  This must be called before vfs_read or vfs_write are called.
// Adding a new file after vfs_read or vfs_write have been called results in undefined behavior.
void vfs_add_file(vfs_fs_t *vfs, const vfs_filename_t filename, vfs_file_config_t * file_config);
// Convert the cluster index to a sector index
uint32_t vfs_cluster_to_sector(vfs_fs_t *vfs, uint32_t cluster_idx);
// Set the callback when a file is created, deleted or has atributes changed.
void vfs_set_file_change_callback(vfs_fs_t *vfs, file_change_func_t cb, void* cb_data);
// Read one or more sectors from the virtual filesystem
void vfs_read(vfs_fs_t * vfs, uint32_t sector, uint8_t *buf, uint32_t num_of_sectors);
// Write one or more sectors to the virtual filesystem
void vfs_write(vfs_fs_t * vfs, uint32_t sector, uint8_t *buf, uint32_t num_of_sectors);

#ifdef __cplusplus
}
#endif

#endif
