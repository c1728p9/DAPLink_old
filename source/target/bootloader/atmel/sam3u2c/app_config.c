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

#include "target_config.h"

// atsam3u2c target information
const target_cfg_t target_device = {
    .board_id   = "0000",
    .secret     = "xxxxxxxx",
    .sector_size    = 0x1000,
    // Assume memory is regions are same size. Flash algo should ignore requests
    //  when variable sized sectors exist
    // .sector_cnt = ((.flash_end - .flash_start) / .sector_size);
    .sector_cnt     = ((KB(128)-KB(32))/0x1000),
    .flash_start    = 0x00080000 + KB(32),
    .flash_end      = 0x00080000 + KB(128),
    .ram_start      = 0x2007C000,
    .ram_end        = 0x20084000
};
