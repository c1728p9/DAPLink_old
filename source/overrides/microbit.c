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

// URL_NAME and DRIVE_NAME must be 11 characters excluding
// the null terminated character
__attribute__((aligned (4)))
const char daplink_url_name[11] =   "MICROBITHTM";
__attribute__((aligned (4)))
const char daplink_drive_name[11] = "MICROBIT   ";
__attribute__((aligned (4)))
const char * const daplink_target_url = "https://www.microbit.co.uk/device?mbedcode=@B@V";

// TODO - investigate why "__attribute__((weak, aligned (4)))" is needed to prevent a
// hardfault from occurring.
