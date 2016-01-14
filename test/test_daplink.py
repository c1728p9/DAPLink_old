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

from msd_test import MassStorageTester


def daplink_test(workspace, parent_test):
    board = workspace.board
    interface = workspace.if_firmware
    test_info = parent_test.create_subtest('daplink_test')

#    if_hex = board.get_interface_hex()
#    bl_hex = board.get_bootloader_hex()

    board.set_mode(board.MODE_BL, test_info)

    # Test loading a binary file with shutils
    test = MassStorageTester(board, test_info, "Shutil hex file load")
    test.set_shutils_copy(interface.hex_path)
    test.set_expected_data(None)
    test.run()

#    # Test loading a binary file with flushes
#    test = MassStorageTester(board, test_info, "Load hex with flushes")
#    test.set_programming_data(bl_hex, 'image.bin')
#    test.set_expected_data(None)
#    test.set_flush_size(0x1000)
#    test.run()


    # Partial bootloader update
    # Partial interface update?
