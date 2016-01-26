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
  --targetdir TARGETDIR
                        Directory with pre-built target test images.
  --user USER           MBED username (required for compile-api)
  --password PASSWORD   MBED password (required for compile-api)
  --firmwaredir FIRMWAREDIR
                        Directory with firmware images to test
  --firmware {k20dx_k64f_if,lpc11u35_efm32gg_stk_if,lpc11u35_lpc1114_if,
kl26z_microbit_if,lpc11u35_lpc812_if,sam3u2c_nrf51822_if,
kl26z_nrf51822_if,k20dx_k22f_if
}
                        Firmware to test
  --noloadif            Skip load step for interface.
  --noloadbl            Skip load step for bootloader.
  --notestendpt         Dont test the interface USB endpoints.
  --notestdl            Dont run DAPLink specific tests.
  --testfirst           If multiple boards of the same type are found only
                        test the first one.
  --verbose {Minimal,Normal,Verbose,All}
                        Verbose output
  --dryrun              Print info on configurations but dont actually run
                        tests.


Example usages
------------------------

Test all built projects in the repository:
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
#TODO - udpate comment
from __future__ import absolute_import
from __future__ import print_function

import os
import argparse
from enum import Enum
from hid_test import test_hid
from serial_test import test_serial
from msd_test import test_mass_storage
from daplink_board import get_all_attached_daplink_boards
from project_generator.generate import Generator
from test_info import TestInfo
from daplink_firmware import load_bundle_from_project, load_bundle_from_release
from firmware import Firmware
from target import load_target_bundle, build_target_bundle
from test_daplink import daplink_test

DEFAULT_TEST_DIR = '../test_results'

# TODO - move somewhere else
VERB_MINIMAL = 'Minimal'    # Just top level errors
VERB_NORMAL = 'Normal'      # Top level errors and warnings
VERB_VERBOSE = 'Verbose'    # All errors and warnings
VERB_ALL = 'All'            # All errors
VERB_LEVELS = [VERB_MINIMAL, VERB_NORMAL, VERB_VERBOSE, VERB_ALL]


def test_endpoints(workspace, parent_test):
    """Run tests to validate DAPLINK fimrware"""
    test_info = parent_test.create_subtest('test_endpoints')
    test_hid(workspace, parent_test)
    test_serial(workspace, test_info)
    test_mass_storage(workspace, test_info)


class TestConfiguration(object):

    def __init__(self, name):
        self.name = name
        self.board = None
        self.target = None
        self.if_firmware = None
        self.bl_firmware = None

    def __str__(self):
        name_board = '<None>'
        name_target = '<None>'
        name_if_firmware = '<None>'
        name_bl_firmware = '<None>'
        if self.board is not None:
            name_board = self.board.name
        if self.target is not None:
            name_target = self.target.name
        if self.if_firmware is not None:
            name_if_firmware = self.if_firmware.name
        if self.bl_firmware is not None:
            name_bl_firmware = self.bl_firmware.name
        return "APP=%s BL=%s Board=%s Target=%s" % (name_if_firmware,
                                                    name_bl_firmware,
                                                    name_board, name_target)


class TestManager(object):

    class _STATE(Enum):
        INIT = 0
        CONFIGURED = 1
        COMPLETE = 2

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
        self._state = self._STATE.INIT
        self._test_configuration_list = None
        self._all_tests_pass = None
        self._firmware_filter = None

        #
        self._untested_firmware = None

    @property
    def all_tests_pass(self):
        assert self._all_tests_pass is not None, 'Must call run_tests first'
        return self._all_tests_pass

    def set_test_first_board_only(self, first):
        """Only test one board of each type"""
        assert isinstance(first, bool)
        assert self._state is self._STATE.INIT
        self._only_test_first = first

    def set_load_if(self, load):
        """Load new interface firmware before testing"""
        assert isinstance(load, bool)
        assert self._state is self._STATE.INIT
        self._load_if = load

    def set_load_bl(self, load):
        """Load new bootloader firmware before testing"""
        assert isinstance(load, bool)
        assert self._state is self._STATE.INIT
        self._load_bl = load

    def set_test_daplink(self, run_test):
        """Run DAPLink specific tests"""
        assert isinstance(run_test, bool)
        assert self._state is self._STATE.INIT
        self._test_daplink = True

    def set_test_ep(self, run_test):
        """Test each endpoint - MSD, CDC, HID"""
        assert isinstance(run_test, bool)
        assert self._state is self._STATE.INIT
        self._test_ep = run_test

    def add_firmware(self, firmware_list):
        """Add firmware to be tested"""
        assert self._state is self._STATE.INIT
        self._firmware_list.extend(firmware_list)

    def add_boards(self, board_list):
        """Add boards to be used for testing"""
        assert self._state is self._STATE.INIT
        self._board_list.extend(board_list)

    def add_targets(self, target_list):
        """Add targets to be used for testing"""
        assert self._state is self._STATE.INIT
        self._target_list.extend(target_list)

    def set_firmware_filter(self, name_list):
        """Test only the project names passed given"""
        assert self._state is self._STATE.INIT
        assert self._firmware_filter is None
        self._firmware_filter = set(name_list)

    def run_tests(self):
        """Run all configurations"""
        # Tests can only be run once per TestManager instance
        assert self._state is self._STATE.CONFIGURED
        self._state = self._STATE.COMPLETE

        all_tests_pass = True
        for test_configuration in self._test_configuration_list:
            board = test_configuration.board
            test_info = TestInfo(test_configuration.name)
            test_configuration.test_info = test_info

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

            if test_info.get_failed():
                all_tests_pass = False

        self._all_tests_pass = all_tests_pass

    def print_results(self, info_level):
        assert self._state is self._STATE.COMPLETE
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

    def write_test_results(self, directory, info_level=TestInfo.INFO):
        assert self._state is self._STATE.COMPLETE

        assert not os.path.exists(directory)
        os.mkdir(directory)
        #TODO - log info about board

        for test_configuration in self._test_configuration_list:
            test_info = test_configuration.test_info
            file_path = directory + os.sep + test_info.name + '.txt'
            with open(file_path, 'w') as file_handle:
                test_info.print_msg(info_level, None, log_file=file_handle)

    def get_test_configurations(self):
        assert self._state in (self._STATE.CONFIGURED,
                               self._STATE.COMPLETE)
        return self._test_configuration_list

    def get_untested_firmware(self):
        assert self._state in (self._STATE.CONFIGURED,
                               self._STATE.COMPLETE)
        return self._untested_firmware

    def build_test_configurations(self, parent_test):
        assert self._state is self._STATE.INIT
        self._state = self._STATE.CONFIGURED
        test_info = parent_test.create_subtest('Build test configuration')

        # Create table mapping each board id to a list of boards with that ID
        board_id_to_board_list = {}
        for board in self._board_list:
            board_id = board.get_board_id()
            if board_id not in board_id_to_board_list:
                board_id_to_board_list[board_id] = []
            board_list = board_id_to_board_list[board_id]
            if self._only_test_first and len(board_list) > 1:
                # Ignore this board since we already have one
                test_info.info('Ignoring extra boards of type 0x%x' %
                               board_id)
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
        filtered_interface_firmware_list = []
        for firmware in self._firmware_list:
            if firmware.type == Firmware.TYPE.BOOTLOADER:
                bootloader_firmware_list.append(firmware)
            elif firmware.type == Firmware.TYPE.INTERFACE:
                name = firmware.name
                if ((self._firmware_filter is None) or
                        (name in self._firmware_filter)):
                    filtered_interface_firmware_list.append(firmware)
            else:
                assert False, 'Unsupported firmware type "%s"' % firmware.type

        # Explicitly specified boards must be present
        fw_name_set = set(fw.name for fw in filtered_interface_firmware_list)
        if self._firmware_filter is not None:
            assert self._firmware_filter == fw_name_set

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
        self._untested_firmware = []
        for firmware in filtered_interface_firmware_list:
            board_id = firmware.board_id
            hdk_id = firmware.hdk_id
            bl_firmware = None
            target = None

            # Check if there is a board to test this firmware
            # and if not skip it
            if board_id not in board_id_to_board_list:
                self._untested_firmware.append(firmware)
                test_info.info('No board to test firmware %s' % firmware.name)
                continue

            # Get target
            target = None
            target_required = not self._test_ep and not self._test_daplink
            if board_id in board_id_to_target:
                target = board_id_to_target[board_id]
            elif target_required:
                self._untested_firmware.append(firmware)
                test_info.info('No target to test firmware %s' %
                               firmware.name)
                continue

            # Check for a bootloader
            bl_required = not self._load_bl and not self._test_daplink
            #TODO - add exemption for bootloader
            if hdk_id in hdk_id_to_bootloader:
                bl_firmware = hdk_id_to_bootloader[hdk_id]
            elif bl_required:
                self._untested_firmware.append(firmware)
                test_info.info('No bootloader to test firmware %s' %
                               firmware.name)
                continue

            # Create a test configuration for each board
            board_list = board_id_to_board_list[board_id]
            for board in board_list:
                if firmware.hdk_id != board.hdk_id:
                    test_info.warning('FW HDK ID %s != Board HDK ID %s' %
                                      (firmware.hdk_id, board.hdk_id))
                if bl_firmware is not None:
                    if firmware.hdk_id != bl_firmware.hdk_id:
                        test_info.warning('FW HDK ID %s != BL HDK ID %s' %
                                          (firmware.hdk_id,
                                           bl_firmware.hdk_id))
                if target is not None:
                    assert firmware.board_id == target.board_id

                test_conf = TestConfiguration(firmware.name + ' ' + board.name)
                test_conf.if_firmware = firmware
                test_conf.bl_firmware = bl_firmware
                test_conf.board = board
                test_conf.target = target
                test_conf_list.append(test_conf)
        self._test_configuration_list = test_conf_list


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
    parser.add_argument('--firmware', help='Firmware to test', action='append',
                        choices=firmware_choices, default=[], required=False)
    parser.add_argument('--logdir', help='Directory to log test results to',
                        default=DEFAULT_TEST_DIR)
    parser.add_argument('--noloadif', help='Skip load step for interface.',
                        default=False, action='store_true')
    parser.add_argument('--noloadbl', help='Skip load step for bootloader.',
                        default=False, action='store_true')
    parser.add_argument('--notestendpt', help='Dont test the interface '
                        'USB endpoints.', default=False, action='store_true')
    parser.add_argument('--notestdl', help='Dont run DAPLink specific tests.',
                        default=False, action='store_true')
    parser.add_argument('--testfirst', help='If multiple boards of the same '
                        'type are found only test the first one.',
                        default=False, action='store_true')
    parser.add_argument('--verbose', help='Verbose output',
                        choices=VERB_LEVELS, default=VERB_NORMAL)
    parser.add_argument('--dryrun', default=False, action='store_true',
                        help='Print info on configurations but dont '
                        'actually run tests.')
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

    firmware_explicitly_specified = len(args.firmware) != 0
    test_info = TestInfo('DAPLink')
    if args.targetdir is not None:
        target_dir = args.targetdir
    else:
        target_dir = '../tmp'
        build_target_bundle(target_dir, args.user, args.password, test_info)

    if os.path.exists(args.logdir):
        print('Error - test results directory "%s" already exists' %
              args.logdir)
        exit(-1)

    # TODO - Switch all boards out of bootloader mode

    # Get all relevant info
    if args.firmwaredir is None:
        firmware_bundle = load_bundle_from_project()
    else:
        firmware_bundle = load_bundle_from_release(args.firmwaredir)
    target_bundle = load_target_bundle(target_dir)
    all_firmware = firmware_bundle.get_firmware_list()
    all_boards = get_all_attached_daplink_boards()
    all_targets = target_bundle.get_target_list()

    # Make sure firmware is present
    if firmware_explicitly_specified:
        all_firmware_names = set(fw.name for fw in all_firmware)
        firmware_missing = False
        for firmware_name in args.firmware:
            if firmware_name not in all_firmware_names:
                firmware_missing = True
                test_info.failure('Cannot find firmware %s' % firmware_name)
        if firmware_missing:
            test_info.failure('Firmware missing - aborting test')
            exit(-1)

    # Create manager and add resources
    tm = TestManager()
    tm.add_firmware(all_firmware)
    tm.add_boards(all_boards)
    tm.add_targets(all_targets)
    if firmware_explicitly_specified:
        tm.set_firmware_filter(args.firmware)

    # Configure test manager
    tm.set_test_first_board_only(args.testfirst)
    tm.set_load_if(not args.noloadif)
    tm.set_load_bl(not args.noloadbl)
    tm.set_test_ep(not args.notestendpt)
    tm.set_test_daplink(not args.notestdl)

    # Build test configurations
    tm.build_test_configurations(test_info)

    test_config_list = tm.get_test_configurations()
    if len(test_config_list) == 0:
        test_info.failure("Nothing that can be tested")
        exit(-1)
    else:
        test_info.info('Test configurations to be run:')
        index = 0
        for test_config in test_config_list:
            test_info.info('    %i: %s' % (index, test_config))
            index += 1
    test_info.info('')

    untested_list = tm.get_untested_firmware()
    if len(untested_list) == 0:
        test_info.info("All firmware can be tested")
    else:
        test_info.info('Fimrware that will not be tested:')
        for untested_firmware in untested_list:
            test_info.info('    %s' % untested_firmware.name)
    test_info.info('')

    if firmware_explicitly_specified and len(untested_list) != 0:
        test_info.failure("Exiting because not all firmware could be tested")
        exit(-1)

    # If this is a dryrun don't run tests, just print info
    if args.dryrun:
        exit(0)

    # Run tests
    tm.run_tests()

    # Print test results
    tm.print_results(args.verbose)
    tm.write_test_results(args.logdir)
    #TODO - LOG VERSION OF TOOLS USED!!!
    #TODO - allow for running from any directory

    # Warn about untested boards
    print('')
    for firmware in tm.get_untested_firmware():
        print('Warning - configuration %s is untested' % firmware.name)

    if tm.all_tests_pass:
        print("All boards passed")
        exit(0)
    else:
        print("Test Failed")
        exit(-1)


if __name__ == "__main__":
    main()
