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

import os
import re
import time
import subprocess
import sys
import six
import mbedapi
import mbed_lstools
from intelhex import IntelHex
from pyOCD.board import MbedBoard

TEST_REPO = 'https://developer.mbed.org/users/c1728p9/code/daplink-validation/'

BOARD_ID_TO_BUILD_TARGET = {
    0x0231: 'FRDM-K22F',
    0x1050: 'NXP-LPC800-MAX',
    0x0240: 'FRDM-K64F',
    0x9900: 'Microbit',
    0x1100: 'Nordic-nRF51-DK',
}


# This prevents the following error message from getting
# displayed on windows if the mbed dismounts unexpectedly
# during a transfer:
#   There is no disk in the drive. Please insert a disk into
#   drive \Device\<Harddiskx>\<rdrive>
def disable_popup():
    if sys.platform.startswith("win"):
        # pylint: disable=invalid-name
        import ctypes
        SEM_FAILCRITICALERRORS = 1
        GetErrorMode = ctypes.windll.kernel32.GetErrorMode
        GetErrorMode.restype = ctypes.c_uint
        GetErrorMode.argtypes = []
        SetErrorMode = ctypes.windll.kernel32.SetErrorMode
        SetErrorMode.restype = ctypes.c_uint
        SetErrorMode.argtypes = [ctypes.c_uint]

        err_mode = GetErrorMode()
        err_mode |= SEM_FAILCRITICALERRORS
        SetErrorMode(err_mode)

disable_popup()


class Board(object):

    def update_interface(self):
        raise NotImplementedError()

    def update_bootloader(self):
        raise NotImplementedError()

    def prepare_for_testing(self):
        raise NotImplementedError()

    @property
    def hw_msd_support(self):
        """Set to True if the hardware supports mass storage"""
        raise NotImplementedError()

    @property
    def hw_cdc_support(self):
        """Set to True if the hardware supports a serial port"""
        raise NotImplementedError()

    @property
    def hw_hid_support(self):
        """Set to True if the hardware supports HID"""
        raise NotImplementedError()

    @property
    def mount_point(self):
        """Mount point of the drive or None if no SW support"""
        raise NotImplementedError()

    @property
    def serial_port(self):
        """Serial port string usable with pySerial  or None if no SW support"""
        raise NotImplementedError()

    @property
    def board_id(self):
        """The ID of this board type"""
        raise NotImplementedError()

    @property
    def unique_id(self):
        """ID which uniquely identifies this board"""
        raise NotImplementedError()


