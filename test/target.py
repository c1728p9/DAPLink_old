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
import info


def load_target_bundle(directory):
    return TargetBundle(directory)


class TargetBundle(object):

    def __init__(self, directory):
        dir_contents = os.listdir(directory)
        name_to_target = {}
        for name in dir_contents:
            base_name, extension = os.path.splitext(name)
            path = directory + os.sep + name
            if os.path.isdir(path):
                # Directories are unused
                pass
            elif os.path.isfile(path):
                if extension not in ('.bin', '.hex'):
                    continue
                if base_name not in name_to_target:
                    name_to_target[base_name] = Target(base_name)
                if extension == '.bin':
                    name_to_target[base_name].set_bin_path(path)
                elif extension == '.hex':
                    name_to_target[base_name].set_hex_path(path)
                else:
                    # Unsupported file type
                    pass
            else:
                assert False
        all_targets = name_to_target.values()
        self._target_list = [target for target in all_targets if target.valid]

    def get_target_list(self):
        """Return the target objects associated with this bundle"""
        return self._target_list


class Target(object):

    def __init__(self, name, hex_path=None, bin_path=None):
        self._name = name
        self._valid = False
        self._hex_path = None
        self._bin_path = None
        self._board_id = None
        if name not in info.TARGET_NAME_TO_BOARD_ID:
            return # Error
        self._board_id = info.TARGET_NAME_TO_BOARD_ID[name]
        if hex_path is not None:
            self.set_hex_path(hex_path)
        if bin_path is not None:
            self.set_bin_path(bin_path)
        self._valid = True

    def set_hex_path(self, path):
        base_name = os.path.basename(path)
        assert self._hex_path is None
        assert base_name == self._name + '.hex'
        path = os.path.abspath(path)
        assert os.path.isfile(path)
        self._hex_path = path

    def set_bin_path(self, path):
        base_name = os.path.basename(path)
        assert self._bin_path is None
        assert base_name == self._name + '.bin'
        path = os.path.abspath(path)
        assert os.path.isfile(path)
        self._bin_path = path

    @property
    def valid(self):
        hex_valid = self._hex_path is not None
        bin_valid = self._bin_path is not None
        return hex_valid and bin_valid and self._valid

    @property
    def name(self):
        return self._name

    @property
    def board_id(self):
        return self._board_id

    @property
    def hex_path(self):
        return self._hex_path

    @property
    def bin_path(self):
        return self._hex_path
