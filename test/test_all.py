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
"""
DAPLink validation and testing tool

optional arguments:
  -h, --help            show this help message and exit
  --user USER           MBED username (required for compile-api)
  --password PASSWORD   MBED password (required for compile-api)
  --project {k20dx_k64f_if,kl26z_nrf51822_if,lpc11u35_lpc1114_if,kl26z_microbit
_if,lpc11u35_lpc812_if,k20dx_k22f_if}
                        Project to test
  --nobuild             Skip build step. Binaries must have been built
                        already.
  --noloadif            Skip load step for interface.
  --noloadbl            Skip load step for bootloader.
  --notestendpt         Dont test the interface USB endpoints.
  --testfirst           If multiple boards of the same type are found only
                        test the first one.
  --verbose {Minimal,Normal,Verbose,All}
                        Verbose output

The purpose of this script is test the daplink projects
and verify as much of it is bug free as reasonable.
1. Build all or some of the projects
2. Load new interface code (bootloader in the future too)
3. Test filesystem and file contents
4. Test each USB endpoint (HID, MSD, CDC)


Example usages
------------------------

Test everything on all projects in the repository:
test_all.py --user <username> --password <password>

Test everything on a single project in the repository:
test_all.py --project <project> --testfirst --user <username>
    --password <password>

Quickly verify that the mass storage and the filesystem
have not regressed after creating a build using:
test_all.py --nobuild --project <project name>

Verify that the USB endpoints are working correctly on
an existing board with firmware already loaded:
test_all.py --nobuild --noloadif --user <username> --password <password>
"""
from __future__ import absolute_import
from __future__ import print_function

import os
import re
import time
import argparse
from hid_test import test_hid
from serial_test import test_serial
from msd_test import test_mass_storage
from daplink_board import get_all_attached_daplink_boards
from project_generator.generate import Generator
from test_info import TestInfo
from daplink_firmware import load_bundle_from_project, load_bundle_from_release
from firmware import Firmware
from target import load_target_bundle
from test_daplink import daplink_test

TEST_REPO = 'https://developer.mbed.org/users/c1728p9/code/daplink-validation/'

# TODO - move somewhere else
VERB_MINIMAL = 'Minimal'    # Just top level errors
VERB_NORMAL = 'Normal'      # Top level errors and warnings
VERB_VERBOSE = 'Verbose'    # All errors and warnings
VERB_ALL = 'All'            # All errors
VERB_LEVELS = [VERB_MINIMAL, VERB_NORMAL, VERB_VERBOSE, VERB_ALL]


def test_endpoints(workspace, parent_test):
    """Run tests to validate DAPLINK fimrware"""
    test_info = parent_test.create_subtest('test_endpoints')
    #board.build_target_firmware(test_info)
    test_hid(workspace, parent_test)
    test_serial(workspace, test_info)
    test_mass_storage(workspace, test_info)


## Determine interface or bootloader - also check if name is valid
## Handle building project when requested
## Handle testing project when requested
#class ProjectTester(object):
#
#    _if_exp = re.compile("^([a-z0-9]+)_([a-z0-9_]+)_if$")
#    _bl_exp = re.compile("^([a-z0-9]+)_bl$")
#    _tool = 'uvision'
#    _name_to_board_id = {
#        'k20dx_k22f_if': 0x0231,
#        'k20dx_k64f_if': 0x0240,
#        'kl26z_microbit_if': 0x9900,
#        'kl26z_nrf51822_if': 0x9900,
#        'lpc11u35_lpc812_if': 0x1050,
#        'lpc11u35_lpc1114_if': 0x1114,
#        'lpc11u35_efm32gg_stk_if': 0x2015,
#        'sam3u2c_nrf51822_if': 0x1100,
#    }
#
#    def __init__(self, yaml_prj, path='.'):
#        self.prj = yaml_prj
#        self._path = path
#        self.name = yaml_prj.name
#        if_match = self._if_exp.match(self.name)
#        if if_match is not None:
#            self._bl_name = if_match.group(1) + '_bl'
#            self._is_bl = False
#            self._board_id = self._name_to_board_id[self.name]
#        bl_match = self._bl_exp.match(self.name)
#        if bl_match is not None:
#            self._is_bl = True
#        if if_match is None and bl_match is None:
#            raise Exception("Invalid project name %s" % self.name)
#        self._built = False
#        self._boards = None
#        self._bl = None
#        self._test_info = TestInfo(self.get_name())
#        build_path = os.path.normpath(path + os.sep + 'projectfiles' + os.sep +
#                                      self._tool + os.sep + self.name +
#                                      os.sep + 'build')
#        self._hex_file = os.path.normpath(build_path + os.sep +
#                                          self.name + '_crc.hex')
#        self._bin_file = os.path.normpath(build_path + os.sep +
#                                          self.name + '_crc.bin')
#
#        # By default test all configurations and boards
#        self._only_test_first = False
#        self._load_if = True
#        self._load_bl = True
#        self._test_daplink = True
#        self._test_ep = True
#
#    def get_name(self):
#        return self.name
#
#    def get_built(self):
#        """
#        Return true if the project has been built in the current session
#        """
#        return self._built
#
#    def build(self, clean=True, parent_test=None):
#        test_info = parent_test or self._test_info
#        if self._built:
#            # Project already built so return
#            return
#
#        # Build bootloader if there is one
#        bl = None
#        if not self.is_bl():
#            bl = self.get_bl()
#        if bl is not None:
#            bl.build(clean, test_info)
#
#        # Build self
#        test_info.info("Building %s" % self.name)
#        start = time.time()
#        ret = self.prj.generate(self._tool, False)
#        test_info.info('Export return value %s' % ret)
#        if ret != 0:
#            raise Exception('Export return value %s' % ret)
#        ret = self.prj.build(self._tool)
#        test_info.info('Build return value %s' % ret)
#        if ret != 0:
#            raise Exception('Build return value %s' % ret)
#        files = self.prj.get_generated_project_files(self._tool)
#        export_path = files['path']
#        base_file = os.path.normpath(export_path + os.sep + 'build' +
#                                     os.sep + self.name + "_crc")
#        built_hex_file = base_file + '.hex'
#        built_bin_file = base_file + '.bin'
#        assert (os.path.abspath(self._hex_file) ==
#                os.path.abspath(built_hex_file))
#        assert (os.path.abspath(self._bin_file) ==
#                os.path.abspath(built_bin_file))
#        self._hex_file = built_hex_file
#        self._bin_file = built_bin_file
#        stop = time.time()
#        test_info.info('Build took %s seconds' % (stop - start))
#        self._built = True


class TestWorkspace(object):

    def __init__(self, name):
        self.name = name
    #TODO - explicitly check fields?



class TestManager(object):

    def __init__(self):
        # By default test all configurations and boards
        self._target_list = []
        self._board_list = []
        self._firmware_list = []
        self._only_test_first = False
        self._load_if = True
        self._load_bl = True
        self._test_daplink = True
        self._test_ep = True

        # Internal state
        self._testing_started = False
        self._test_configuration_list = None
        self._all_tests_pass = None

    @property
    def all_tests_pass(self):
        assert self._all_tests_pass is not None, 'Must call run_tests first'
        return self._all_tests_pass

    def set_test_first_board_only(self, first):
        """Only test one board of each type"""
        assert isinstance(first, bool)
        assert not self._testing_started
        self._only_test_first = first

    def set_load_if(self, load):
        """Load new interface firmware before testing"""
        assert isinstance(load, bool)
        assert not self._testing_started
        self._load_if = load

    def set_load_bl(self, load):
        """Load new bootloader firmware before testing"""
        assert isinstance(load, bool)
        assert not self._testing_started
        self._load_bl = load

    def set_test_daplink(self, run_test):
        """Run DAPLink specific tests"""
        assert isinstance(run_test, bool)
        assert not self._testing_started
        self._test_daplink = True

    def set_test_ep(self, run_test):
        """Test each endpoint - MSD, CDC, HID"""
        assert isinstance(run_test, bool)
        assert not self._testing_started
        self._test_ep = run_test

    def add_firmware(self, firmware_list):
        """Add firmware to be tested"""
        assert not self._testing_started
        self._firmware_list.extend(firmware_list)

    def add_boards(self, board_list):
        """Add boards to be used for testing"""
        assert not self._testing_started
        self._board_list.extend(board_list)

    def add_targets(self, target_list):
        """Add targets to be used for testing"""
        assert not self._testing_started
        self._target_list.extend(target_list)

    def run_tests(self):
        # Tests can only be run once per TestManager instance
        assert not self._testing_started
        self._testing_started = True

        self._test_configuration_list = self._create_test_configurations()
        for test_configuration in self._test_configuration_list:
            board = test_configuration.board
            test_info = TestInfo(test_configuration.name)
            test_configuration.test_info = test_info # TODO - how should this be handled?

            if self._load_if:
                if_path = test_configuration.if_firmware.hex_path
                board.load_interface(if_path, test_info)

            valid_bl = test_configuration.bl_firmware is not None
            if self._load_bl and valid_bl:
                bl_path = test_configuration.bl_firmware.hex_path
                board.load_bootloader(bl_path, test_info)

            board.set_check_fs_on_remount(True)

            if self._test_daplink:
                daplink_test(test_configuration, test_info)

            if self._test_ep:
                test_endpoints(test_configuration, test_info)

        #TODO - set self._all_tests_pass

    def print_results(self, info_level):
        assert self._testing_started
        # Print info for boards tested
        for test_configuration in self._test_configuration_list:
            print('')
            test_info = test_configuration.test_info
            if info_level == VERB_MINIMAL:
                test_info.print_msg(TestInfo.FAILURE, 0)
            elif info_level == VERB_NORMAL:
                test_info.print_msg(TestInfo.WARNING, None)
            elif info_level == VERB_VERBOSE:
                test_info.print_msg(TestInfo.WARNING, None)
            elif info_level == VERB_ALL:
                test_info.print_msg(TestInfo.INFO, None)
            else:
                # This should never happen
                assert False

    def get_untested_configurations(self):
        return [] #TODO

    def _create_test_configurations(self):

        # Create table mapping each board id to a list of boards with that ID
        board_id_to_board_list = {}
        for board in self._board_list:
            board_id = board.get_board_id()
            if board_id not in board_id_to_board_list:
                board_id_to_board_list[board_id] = []
            board_list = board_id_to_board_list[board_id]
            if self._only_test_first and len(board_list) > 1:
                # Ignore this board since we already have one
                continue
            board_list.append(board)

        # Create a table mapping each board id to a target
        board_id_to_target = {}
        for target in self._target_list:
            assert target.board_id not in board_id_to_target, 'Multiple ' \
                'targets found for board id "%s"' % target.board_id
            board_id_to_target[target.board_id] = target

        # Create a list for bootloader firmware and interface firmware
        bootloader_firmware_list = []
        interface_firmware_list = []
        for firmware in self._firmware_list:
            if firmware.type == Firmware.TYPE.BOOTLOADER:
                bootloader_firmware_list.append(firmware)
            elif firmware.type == Firmware.TYPE.INTERFACE:
                interface_firmware_list.append(firmware)
            else:
                assert False, 'Unsupported firmware type "%s"' % firmware.type

        # Create a table mapping each hdk to a bootloader
        hdk_id_to_bootloader = {}
        for firmware in bootloader_firmware_list:
            hdk_id = firmware.hdk_id
            assert hdk_id not in hdk_id_to_bootloader, 'Duplicate ' \
                'bootloaders for HDK "%s" not allowed'
            hdk_id_to_bootloader[hdk_id] = firmware

        # Create a test configuration for each interface and supported board
        # combination
        test_conf_list = []
        for firmware in interface_firmware_list:
            board_id = firmware.board_id
            hdk_id = firmware.hdk_id
            bl_firmware = None
            target = None

            # Check if there is a board to test this firmware
            # and if not skip it
            if board_id not in board_id_to_board_list:
                #TODO - record unsupported board
                continue

            # Get target
            target = None
            if board_id in board_id_to_target:
                target = board_id_to_target[board_id]

            # Check for a bootloader
            if hdk_id in hdk_id_to_bootloader:
                bl_firmware = hdk_id_to_bootloader[hdk_id]

            # Create a test configuration for each board
            board_list = board_id_to_board_list[board_id]
            for board in board_list:
                assert firmware.hdk_id == board.hdk_id
                if bl_firmware is not None:
                    assert firmware.hdk_id == bl_firmware.hdk_id
                if target is not None:
                    assert firmware.board_id == target.board_id

                test_conf = TestWorkspace(firmware.name + ' ' + board.name)
                test_conf.if_firmware = firmware
                test_conf.bl_firmware = bl_firmware
                #TODO - record missing bootloader
                test_conf.board = board
                test_conf.target = target
                test_conf_list.append(test_conf)
                #TODO - record missing targets?
            #TODO - check wich boards haven't been tested?

        return test_conf_list

def get_firmware_names():

    # Save current directory
    cur_dir = os.getcwd()
    parent_dir = os.path.dirname(cur_dir)
    os.chdir(parent_dir)
    try:
        all_names = set()
        projects = list(Generator('projects.yaml').generate())
        for project in projects:
            assert project.name not in all_names
            all_names.add(project.name)
    finally:
        # Restore the current directory
        os.chdir(cur_dir)
    return list(all_names)


def main():

    firmware_list = get_firmware_names()
    firmware_choices = [firmware for firmware in firmware_list if
                        firmware.endswith('_if')]

    #TODO - rename args
    description = 'DAPLink validation and testing tool'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('--targetdir',
                        help='Directory with pre-built target test images.',
                        default=None)
    parser.add_argument('--user', type=str, default=None,
                        help='MBED username (required for compile-api)')
    parser.add_argument('--password', type=str, default=None,
                        help='MBED password (required for compile-api)')
    parser.add_argument('--firmwaredir',
                        help='Directory with firmware images to test',
                        default=None)
    parser.add_argument('--firmware', help='Firmware to test', action='append',#TODO - change to firmware
                        choices=firmware_choices, default=[], required=False)
#    parser.add_argument('--nobuild', help='Skip build step.  Binaries must have been built already.', default=False,
#                        action='store_true')
    parser.add_argument('--noloadif', help='Skip load step for interface.',
                        default=False, action='store_true')
    parser.add_argument('--noloadbl', help='Skip load step for bootloader.',
                        default=False, action='store_true')
    parser.add_argument('--notestendpt', help='Dont test the interface USB endpoints.',
                        default=False, action='store_true')
    parser.add_argument('--testfirst', help='If multiple boards of the same type are found only test the first one.',
                        default=False, action='store_true')
    parser.add_argument('--verbose', help='Verbose output',
                        choices=VERB_LEVELS, default=VERB_NORMAL)
    # TODO - test results
    args = parser.parse_args()

    use_prebuilt = args.targetdir is not None
    use_compile_api = args.user is not None and args.password is not None

    # Validate args
    if not args.notestendpt:
        if not use_prebuilt and not use_compile_api:
            print("Endpoint test requires target test images.")
            print("  Directory with pre-build target test images")
            print("  must be specified with '--targetdir'")
            print("OR")
            print("  Mbed login credentials '--user' and '--password' must")
            print("  be specified so test images can be built with")
            print("  the compile API.")
            exit(-1)

    boards_explicitly_specified = len(args.firmware) != 0#TODO
    if args.targetdir is not None:
        target_dir = args.targetdir
    else:
        target_dir = '../tmp'

    #TODO Switch all boards out of bootloader mode

    #TODO - Build firmware if requested?

    #TODO - Build targets if requested

    # Build targets if requested

    # Get all relevant info
    firmware_bundle = load_bundle_from_project()#TODO or load from directory
    target_bundle = load_target_bundle(target_dir)
    all_firmware = firmware_bundle.get_firmware_list()
    all_boards = get_all_attached_daplink_boards()
    all_targets = target_bundle.get_target_list()
    #TODO - filter out firmware

    # Create manager and add resources
    tm = TestManager()
    tm.add_firmware(all_firmware)
    tm.add_boards(all_boards)
    tm.add_targets(all_targets)
#    tm.can_test_all_boards()    # TODO
#    tm.can_test_all_firmware()

    #TODO - rename file to run_test.py

    # Configure test manager
    tm.set_test_first_board_only(args.testfirst)
    tm.set_load_if(not args.noloadif)
    tm.set_load_bl(not args.noloadbl)
    tm.set_test_ep(not args.notestendpt)
    tm.set_test_daplink(True) #TODO - arg for this

#    tm.get_untested_firmware()
#    tm.get_untested_boards()

    # Run tests
    tm.run_tests()

    # Print test results
    tm.print_results(args.verbose)
    #tm.write_test_results() # TODO - write test results to a file

        #TODO
#    # Put together the list of projects to build
#    if boards_explicitly_specified:
#        projects_to_test = [name_to_prj[name] for name in args.project]
#    else:
#        projects_to_test = if_project_list

        #TODO
#    # Attach firmware build credentials
#    if not args.notestendpt:
#        for board in all_boards:
#            if args.targetdir is None:
#                board.set_build_login(args.user, args.password)
#            else:
#                board.set_build_prebuilt_dir(args.targetdir)

        #TODO
#    # Build all projects
#    if not args.nobuild:
#        for project in projects_to_test:
#            project.build()


            #TODO
#    if boards_explicitly_specified:
#        # Error should have been triggered before this
#        # point if there were boards that couldn't be tested
#        assert len(untested_projects) == 0


    # Warn about untested boards
    print('')
    for project in tm.get_untested_configurations():
        print('Warning - configuration %s is untested' % project.name)#UNTESTED CONFIGS

    if tm.all_tests_pass:
        print("All boards passed")
        exit(0)
    else:
        print("Test Failed")
        exit(-1)


if __name__ == "__main__":
    main()
