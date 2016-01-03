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
#include "daplink.h"
#include "flash_intf.h"
#include "util.h"
#include "flash_hal.h"
#include "FlashPrg.h"
#include "compiler.h"
#include "crc.h"
#include "info.h"
#include "macro.h"

// Application start must be aligned to page write
COMPILER_ASSERT(DAPLINK_ROM_APP_START % DAPLINK_MIN_WRITE_SIZE == 0);
// Application size must be a multiple of write size
COMPILER_ASSERT(DAPLINK_ROM_APP_SIZE % DAPLINK_MIN_WRITE_SIZE == 0);
// Sector size must be a multiple of write size
COMPILER_ASSERT(DAPLINK_SECTOR_SIZE % DAPLINK_MIN_WRITE_SIZE == 0);
// Application start must be aligned to a sector erase
COMPILER_ASSERT(DAPLINK_ROM_APP_START % DAPLINK_SECTOR_SIZE == 0);
// Update start must be aligned to sector write
COMPILER_ASSERT(DAPLINK_ROM_UPDATE_START % DAPLINK_SECTOR_SIZE == 0);
// Update size must be a multiple of sector size
COMPILER_ASSERT(DAPLINK_ROM_UPDATE_SIZE % DAPLINK_SECTOR_SIZE == 0);

typedef enum {
    STATE_CLOSED,
    STATE_OPEN,
    STATE_ERROR
} state_t;

static error_t init(void);
static error_t uninit(void);
static error_t program_page(uint32_t addr, const uint8_t * buf, uint32_t size);
static error_t erase_sector(uint32_t sector);
static error_t erase_chip(void);
static uint32_t program_page_min_size(uint32_t addr);
static uint32_t erase_sector_size(uint32_t addr);

static bool page_program_allowed(uint32_t addr, uint32_t size);
static bool sector_erase_allowed(uint32_t addr);
static bool intercept_page_write(uint32_t addr, const uint8_t * buf, uint32_t size, error_t * status);
static bool intercept_sector_erase(uint32_t addr, error_t * status);

static const flash_intf_t flash_intf = {
    init,
    uninit,
    program_page,
    erase_sector,
    erase_chip,
    program_page_min_size,
    erase_sector_size,
};
    
const flash_intf_t * const flash_intf_iap_protected = &flash_intf;

static state_t state = STATE_CLOSED;
static bool update_complete;
static bool mass_erase_performed;
static bool current_sector_set;
static uint32_t current_sector;
static uint32_t current_sector_size;
static bool current_page_set;
static uint32_t current_page;
static uint32_t current_page_write_size;
static uint32_t crc;
static uint8_t sector_buf[DAPLINK_SECTOR_SIZE];

static error_t init()
{
    int iap_status;
    bool update_supported = DAPLINK_ROM_UPDATE_SIZE != 0;
    if (state != STATE_CLOSED) {
        util_assert(0);
        return ERROR_INTERNAL;
    }

    if (!update_supported) {
        return ERROR_IAP_UPDT_NOT_SUPPORTED;
    }
    iap_status = Init(0, 0, 0);
    if (iap_status != 0) {
        return ERROR_IAP_INIT;
    }

    update_complete = false;
    mass_erase_performed = false;
    current_sector_set = false;
    current_sector = 0;
    current_sector_size = 0;
    current_page_set = false;
    current_page = 0;
    current_page_write_size = 0;
    crc = 0;
    memset(sector_buf, 0, sizeof(sector_buf));

    state = STATE_OPEN;
    return ERROR_SUCCESS;
}

static error_t uninit(void)
{
    int iap_status;
    if (STATE_CLOSED == state) {
        util_assert(0);
        return ERROR_INTERNAL;
    }

    state = STATE_CLOSED;
    iap_status = UnInit(0);
    if (iap_status != 0) {
        return ERROR_IAP_UNINIT;
    }
    if (!update_complete) {
        return ERROR_IAP_UPDT_INCOMPLETE;
    }
    return ERROR_SUCCESS;
}

static error_t program_page(uint32_t addr, const uint8_t * buf, uint32_t size)
{
    uint32_t iap_status;
    error_t status;
    uint32_t min_prog_size;
    uint32_t sector_size;
    uint32_t updt_end = DAPLINK_ROM_UPDATE_START + DAPLINK_ROM_UPDATE_SIZE;
    if (state != STATE_OPEN) {
        util_assert(0);
        return ERROR_INTERNAL;
    }
    min_prog_size = program_page_min_size(addr);
    sector_size = erase_sector_size(addr);
    // Address must be on a write size boundary
    if (addr % min_prog_size != 0) {
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }
    // Programming size must be a non-zero multiple of the minimum write size
    if ((size < min_prog_size) || (size % min_prog_size != 0)) {
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }
    // Write must not cross a sector boundary
    if ((addr % sector_size) + size > sector_size) {
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }
    // Write must be in an erased sector (current_sector is always erased if it is set)
    if (!mass_erase_performed) {
        if (!current_sector_set) {
            util_assert(0);
            state = STATE_ERROR;
            return ERROR_INTERNAL;
        }
        if ((addr < current_sector) || (addr >= current_sector + current_sector_size)) {
            util_assert(0);
            state = STATE_ERROR;
            return ERROR_INTERNAL;
        }
    }
    // Address must be sequential - no gaps
    if (current_page_set && (addr != current_page + current_page_write_size)) {
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }

    if (!page_program_allowed(addr, size)) {
        state = STATE_ERROR;
        return ERROR_IAP_WRITE;
    }
    current_page_set = true;
    current_page = addr;
    current_page_write_size = size;
    if (intercept_page_write(addr, buf, size, &status)) {
        if (ERROR_SUCCESS != status) {
            state = STATE_ERROR;
        }
        return status;
    }
    iap_status = flash_program_page(addr, size, (uint8_t*)buf);
    if (iap_status != 0) {
        state = STATE_ERROR;
        return ERROR_IAP_WRITE;
    }
    if (addr + size >= updt_end) {
        update_complete = true;
    }
    return ERROR_SUCCESS;
}

static error_t erase_sector(uint32_t addr)
{
    uint32_t iap_status;
    error_t status;
    uint32_t sector_size;
    if (state != STATE_OPEN) {
        util_assert(0);
        return ERROR_INTERNAL;
    }
    // Address must be on a sector boundary
    sector_size = erase_sector_size(addr);
    if (addr % sector_size != 0) {
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }
    // Address must be sequential - no gaps
    if (current_sector_set && (addr != current_sector + current_sector_size)) {
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }

    if (!sector_erase_allowed(addr)) {
        state = STATE_ERROR;
        return ERROR_IAP_ERASE_SECTOR;
    }
    current_sector_set = true;
    current_sector = addr;
    current_sector_size = sector_size;
    if (intercept_sector_erase(addr, &status)) {
        if (ERROR_SUCCESS != status) {
            state = STATE_ERROR;
        }
        return status;
    }
    iap_status = flash_erase_sector(addr);
    if (iap_status != 0) {
        state = STATE_ERROR;
        return ERROR_IAP_ERASE_SECTOR;
    }
    return ERROR_SUCCESS;
}

static error_t erase_chip(void)
{
    uint32_t updt_start = DAPLINK_ROM_UPDATE_START;
    uint32_t updt_end = DAPLINK_ROM_UPDATE_START + DAPLINK_ROM_UPDATE_SIZE;
    if (state != STATE_OPEN) {
        util_assert(0);
        return ERROR_INTERNAL;
    }
    if (mass_erase_performed) {
        // Mass erase only allowed once
        util_assert(0);
        state = STATE_ERROR;
        return ERROR_INTERNAL;
    }

    for (uint32_t addr = updt_start; addr < updt_end; addr += DAPLINK_SECTOR_SIZE) {
        error_t status;
        status = erase_sector(addr);
        if(status != ERROR_SUCCESS) {
            state = STATE_ERROR;
            return ERROR_IAP_ERASE_ALL;
        }
    }
    mass_erase_performed = true;
    return ERROR_SUCCESS;
}

static uint32_t program_page_min_size(uint32_t addr)
{
    return DAPLINK_MIN_WRITE_SIZE;
}

static uint32_t erase_sector_size(uint32_t addr)
{
    return DAPLINK_SECTOR_SIZE;
}

static bool page_program_allowed(uint32_t addr, uint32_t size)
{
    // Check if any data would overlap with the application region
    if ((addr < DAPLINK_ROM_APP_START + DAPLINK_ROM_APP_SIZE) && (addr + size > DAPLINK_ROM_APP_START)) {
        return false;
    }
    return true;
}

static bool sector_erase_allowed(uint32_t addr)
{
    uint32_t app_start = DAPLINK_ROM_APP_START;
    uint32_t app_end = DAPLINK_ROM_APP_START + DAPLINK_ROM_APP_SIZE;
    // Check if the sector is part of the application
    if ((addr >= app_start) && (addr < app_end)) {
        return false;
    }
    return true;
}

static bool intercept_page_write(uint32_t addr, const uint8_t * buf, uint32_t size, error_t * status)
{
    uint32_t crc_size;
    uint32_t updt_start = DAPLINK_ROM_UPDATE_START;
    uint32_t updt_end = DAPLINK_ROM_UPDATE_START + DAPLINK_ROM_UPDATE_SIZE;
    *status = ERROR_INTERNAL;
    if (state != STATE_OPEN) {
        util_assert(0);
        *status = ERROR_INTERNAL;
        return true;
    }

    if ((addr < updt_start) || (addr >= updt_end)) {
        *status = ERROR_IAP_OUT_OF_BOUNDS;
        return true;
    }

    if (!daplink_is_interface()) {
        return false;
    }

    /* Everything below here is interface specific */

    crc_size = MIN(size, updt_end - addr - 4);
    crc = crc32_continue(crc, buf, crc_size);

    // Intercept the data if it is in the first sector
    if ((addr >= updt_start) && (addr < updt_start + DAPLINK_SECTOR_SIZE)) {
        uint32_t buf_offset = addr - updt_start;
        memcpy(sector_buf + buf_offset, buf, size);
        *status = ERROR_SUCCESS;
        return true;
    }

    // Finalize update if this is the last sector
    if (updt_end == addr + size) {
        uint32_t iap_status;

        uint32_t size_left = updt_end - addr;
        uint32_t crc_in_image = (buf[size_left - 4] << 0) |
                                (buf[size_left - 3] << 8) |
                                (buf[size_left - 2] << 16) |
                                (buf[size_left - 1] << 24);
        if (crc != crc_in_image) {
            *status = ERROR_BL_UPDT_BAD_CRC;
            return true;
        }
        // Program the current buffer
        iap_status = flash_program_page(addr, size, (uint8_t*)buf);
        if(iap_status != 0) {
            *status = ERROR_WRITE;
            return true;
        }
        // Erase the first sector
        iap_status = flash_erase_sector(DAPLINK_ROM_UPDATE_START);
        if(iap_status != 0) {
            *status = ERROR_WRITE;
            return true;
        }
        // Fill it in with the bootloader's vector table
        iap_status = flash_program_page(DAPLINK_ROM_UPDATE_START, DAPLINK_SECTOR_SIZE, (uint8_t*)sector_buf);
        if(iap_status != 0) {
            *status = ERROR_WRITE;
            return true;
        }
        // The bootloader has been updated so recompute the crc
        info_crc_compute();

        update_complete = true;
        *status = ERROR_SUCCESS;
        return true;
    }
    return false;
}

bool intercept_sector_erase(uint32_t addr, error_t * status)
{
    uint32_t updt_start = DAPLINK_ROM_UPDATE_START;
    uint32_t updt_end = DAPLINK_ROM_UPDATE_START + DAPLINK_ROM_UPDATE_SIZE;
    *status = ERROR_INTERNAL;
    if (state != STATE_OPEN) {
        util_assert(0);
        return ERROR_INTERNAL;
    }

    if ((addr < updt_start) || (addr >= updt_end)) {
        *status = ERROR_IAP_OUT_OF_BOUNDS;
        return true;
    }
    if (!daplink_is_interface()) {
        return false;
    }

    /* Everything below here is interface specific */

    if (DAPLINK_ROM_UPDATE_START == addr) {
        uint32_t iap_status;
        uint32_t addr = DAPLINK_ROM_UPDATE_START;

        // Erase the first sector
        iap_status = flash_erase_sector(addr);
        if(iap_status != 0) {
            *status = ERROR_ERASE_ALL;
            return true;
        }
        // Program the interface's vector table
        iap_status = flash_program_page(addr, DAPLINK_MIN_WRITE_SIZE, (uint8_t*)DAPLINK_ROM_IF_START);
        if(iap_status != 0) {
            *status = ERROR_ERASE_ALL;
            return true;
        }
        *status = ERROR_SUCCESS;
        return true;
    }
    return false;
}
