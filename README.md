# DAPLink
DAPLink is firmware that runs on Hardware Interface Circuits (HICs) and provides methods to program and debug a microcontroller via JTAG or SWD. The firware exposes a USB composite device to the host OS with CDC, HID and MSC endpoints. These endpoints are critical when developing software for microcontrollers. Description of endpoints:
* MSC - drag-n-drop programming of binary and hex files
* CDC - Virtual com port for USB to UART for log, trace and terminal emulation
* HID - CMSIS-DAP compliant debug channel.

For more detailed information see the user guide.

## Compatibility
There are many Hardware Interface Circuits (HICs) that DAPLink firmware runs on. These can be found as standalone boards or as part of development kits. Known supported circuits are based on and compatible with:
* Atmel edbg (SAM3U) - coming soon
* Maxim Epsilon (MAX32550) - coming soon
* [NXP OpenSDA](http://www.nxp.com/products/software-and-tools/run-time-software/kinetis-software-and-tools/ides-for-kinetis-mcus/opensda-serial-and-debug-adapter:OPENSDA)
* [NXP Link based on LPC11U35 or LPC4322](https://www.lpcware.com/LPCXpressoBoards)

## Releases
There are many HIC and target combinations created from this repository. Quarterly releases will contain new features and bugfixes. Standalone bugfixes are released once reported, verified and fixed. Both quarterly and bugfix releases will result in the build number being incremented, however, not all HIC and targets will be released if not affected. Release notes and all artifacts can be found under releases. **Products shipping or compatible with this firmware should have instructions on how to upgrade and the most up to date release build on the product page.**

## Develop
Install necessary tools. Skip any step where a compatible tool already exists. All tools **MUST** be added to the system path

* Install [Python 2.7.9 or above](https://www.python.org/downloads/)
* Install [Git](https://git-scm.com/downloads)
* Install [Keil MDK-ARM](https://www.keil.com/download/product/)
* Install virtualenv in your global Python installation

1. Get the sources and create a virtual environment
```
> git clone https://github.com/mbedmicro/DAPLink
> pip install virtualenv
> virtualenv venv
```

2. Update tools and generate project files. **This should be done everytime you pull new changes**
```
> "venv/Scripts/activate"
> pip install -r requirements.txt
> progen generate -t uvision
> "venv/Scripts/deactivate"
```

For adding new targets start from template and use these docs...

## Release
1. Create a tag with the correct release version and push it to github
2. Clean the repo you will be building from by running 'git clean -xdf' followed by 'git reset --hard'
3. Run the script 'build_release_uvision.bat' to create all builds.
4. All release deliverables will be created and stored in 'uvision_release'.  Save this wherever your builds are stored.

Note: A previous build can be reproduced by using the 'build_requirements.txt' of that build.
To do this add the additional argument 'build_requirements.txt' when calling 'build_release_uvision.bat' in step 2.
This will install and build with the exact version of the python packages used to create that build.

## Contribute
Check out the issue tracker.

##ToDo
- Create a test
- Document how to use
- Document how to contribute
