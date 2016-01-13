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
import firmware


def load_bundle_from_release(directory):
    return DAPLinkFirmwareBundle(directory)


def load_bundle_from_project():
    


class ReleaseFirmwareBundle(firmware.FirmwareBundle):

    def __init__(self, directory):
        

    def get_firmware_list(self):
        raise NotImplementedError()

    @property
    def build_sha(self):
        raise NotImplementedError()

    @property
    def build_local_mods(self):
        raise NotImplementedError()


class ProjectFirmwareBundle(firmware.FirmwareBundle):

    def __init__(self, directory, tool):
        

    def get_all_firmware(self):
        """Return the firmware objects associated with this bundle"""
        raise NotImplementedError()

    @property
    def build_sha(self):
        """The GIT SHA this build was created at as a string or None"""
        raise NotImplementedError()

    @property
    def build_local_mods(self):
        """True if this was a clean build, False otherwise"""
        raise NotImplementedError()


class DAPLinkfirmware(firmware.Firmware):

    def __init__(self, name, bundle, directory):
        self._name = name
        self._bundle = bundle
        self._directory = directory
        #check for valid name
        #TODO - check for required files
        #FIRMWARE_NAME_TO_BOARD_ID

    @property
    def name(self):
        """Name of this project"""
        raise NotImplementedError()

    @property
    def hdk_id(self):
        """HDK ID for the type of board this firmware can run on"""
        raise NotImplementedError()

    @property
    def board_id(self):
        """Board ID for the type of board this firmware can run on or None"""
        raise NotImplementedError()

    @property
    def type(self):
        """Build type - either interface or bootloader"""
        raise NotImplementedError()

    @property
    def bin_path(self):
        """Path to the binary vesion of this firmware or None"""
        raise NotImplementedError()

    @property
    def hex_path(self):
        """Path to the hex vesion of this firmware or None"""
        raise NotImplementedError()

    @property
    def elf_path(self):
        """Path to the hex vesion of this firmware or None"""
        raise NotImplementedError()
