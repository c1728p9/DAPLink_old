# CMSIS-DAP Interface Firmware
# Copyright (c) 2009-2013 ARM Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
from __future__ import absolute_import
import re

_IF_RE = re.compile("^([a-z0-9]+)_([a-z0-9_]+)_if$")
_BL_RE = re.compile("^([a-z0-9]+)_bl$")

# Add new HDKs here
_HDK_STRING_TO_ID = {
    'k20dx': 0x646c0000,
    'kl26z': 0x646c0001,
    'lpc11u35': 0x646c0002,
    'sam3u2c': 0x646c0003,
}

# Add new firmware with a hard coded board ID here
FIRMWARE_NAME_TO_BOARD_ID = {
    'k20dx_k22f_if': 0x0231,
    'k20dx_k64f_if': 0x0240,
    'kl26z_microbit_if': 0x9900,
    'kl26z_nrf51822_if': 0x9900,
    'lpc11u35_lpc812_if': 0x1050,
    'lpc11u35_lpc1114_if': 0x1114,
    'lpc11u35_efm32gg_stk_if': 0x2015,
    'sam3u2c_nrf51822_if': 0x1100,
}

#TODO - grab these from mbedls
_BOARD_ID_TO_BUILD_TARGET = {
    0x0231: 'FRDM-K22F',
    0x1050: 'NXP-LPC800-MAX',
    0x0240: 'FRDM-K64F',
    0x9900: 'Microbit',
    0x1100: 'Nordic-nRF51-DK',
}

#TODO - sanity check?

def daplink_board_info(board_id):
    info = BoardInfo(board_id)
    info.

def daplink_firmware_info(name):
    info = FirmwareInfo("k20dx_bl")

    is_bl = False
    is_if = False
    string_hdk = None
    #string_target = None
    match = _BL_RE.match(name)
    if match is not None:
        string_hdk = match.group(1)
        is_bl = True
    match = _IF_RE.match(name)
    if match is not None:
        string_hdk = match.group(1)
        #string_target = match.group(2)
        is_if = True

    if string_hdk is None:
        return None
    assert is_bl ^ is_if

    info.hdk_id = _HDK_STRING_TO_ID[string_hdk]
    info.board_id = None
    if name in _FIRMWARE_NAME_TO_BOARD_ID:
        info.board_id = _FIRMWARE_NAME_TO_BOARD_ID[name]
    info.is_bl = is_bl
    info.is_if = is_if

    if is_if:
        info.cdc = True
        info.hid = True
        info.msd = True
    else:
        info.cdc = False
        info.hid = False
        info.msd = False  # No target programming
    info.update = True


    
    
    string_hdk = 
    string_target = 

    info.hdk_id = 1234
    info.interface = True
    info.board_id = 0x1234
    info.cdc = True
    info.hid = True
    info.msd = True
    info.update = True
    FIRMWARE_LIST.append(info)

    _tool = 'uvision'




class BoardInfo(object):
    pass

class FirmwareInfo(object):
    def __init__(self, interface=None, bootlooader=None, hdk_id=None, board_id=None):
        pass

FIRMWARE_LIST = []

info = FirmwareInfo("k20dx_bl")
info.hdk_id = 1234
info.interface = True
info.board_id = 0x1234
info.cdc = True
info.hid = True
info.msd = True
info.update = True
FIRMWARE_LIST.append(info)

info = FirmwareInfo("k20dx_k22f_if")
info.hdk_id = 1234
info.interface = True
info.board_id = 0x1234
info.cdc = True
info.hid = True
info.msd = True
info.update = True
FIRMWARE_LIST.append(info)

info = FirmwareInfo("k20dx_k64f_if")
info.hdk_id = 1234
info.interface = True
info.board_id = 0x1234
info.cdc = True
info.hid = True
info.msd = True
info.update = True
FIRMWARE_LIST.append(info)


FIRMWARE_K20DX_K22F_IF

FIRMWARE_NAME_TO_INFO = {
    "k20dx_bl"
    "k20dx_k22f_if": FirmwareInfo(interface=True, hdk_id=12345, board_id=0x0231),
}

FIRMWARE_NAME_TO_BOARD_ID = {
    "k20dx_k22f_if": 0x0231
    
}

FIRMWARE_NAME_TO_HDK_ID = {
}


BOARD_ID_TO_BUILD_TARGET = {
    0x0231: 'FRDM-K22F',
    0x1050: 'NXP-LPC800-MAX',
    0x0240: 'FRDM-K64F',
    0x9900: 'Microbit',
    0x1100: 'Nordic-nRF51-DK',
}
