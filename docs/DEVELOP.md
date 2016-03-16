## Setup
Install the necessary tools listed below. Skip any step where a compatible tool already exists. All tools **MUST** be added to the system path.

* Install [Python 2.7.9 or above](https://www.python.org/downloads/)
* Install [Git](https://git-scm.com/downloads)
* Install [Keil MDK-ARM](https://www.keil.com/download/product/)
* Install virtualenv in your global Python installation eg: `pip install virtualenv`

**Step 1.** Get the sources and create a virtual environment

```
$ git clone https://github.com/mbedmicro/DAPLink
$ pip install virtualenv
$ virtualenv venv
```

**Step 2.** Update tools and generate project files. **This should be done everytime you pull new changes**

```Windows
$ "venv/Scripts/activate"
$ pip install -r requirements.txt
$ progen generate -t uvision
$ "venv/Scripts/deactivate"
```

**Step 3.** Pull requests should be made once a changeet is [rebased onto Master](https://www.atlassian.com/git/tutorials/merging-vs-rebasing/workflow-walkthrough)


## Porting
There are three defined way in which DAPLink can be extended.  These are adding target suppport, adding board support and adding HIC support.  Details on porting each of these can be found below.

* [Adding a new target](PORT_TARGET.md)
* [Adding a new board](PORT_BOARD.md)
* [Adding a new HIC](PORT_HIC.md)


## Test
* Build the project to be tested.
* Enable automation mode on the board if is has not been enabled already
* python run_test.py --user <username> --password <password>
* Test results will be printed to console 


## Enable Automation Features
1. Update the interface software to a version at or above 0241
2. Create an empty text file called "auto_on.cfg" on the drive while the reset button is held
3. Release the reset button after the drive remounts and confirm "Automation Allowed" is set to 1 in details.txt
4. Update the bootloader software to a version at or above 0241
5. Confirm that "start_bl.act" causes the device to enter bootloader mode and "start_if.act" causes the device to enter interface mode.


## Release
1. Create a tag with the correct release version and push it to github
2. Clean the repo you will be building from by running 'git clean -xdf' followed by 'git reset --hard'
3. Run the script 'build_release_uvision.bat' to create all builds.
4. All release deliverables will be created and stored in 'uvision_release'.  Save this wherever your builds are stored.

Note: A previous build can be reproduced by using the 'build_requirements.txt' of that build.
To do this add the additional argument 'build_requirements.txt' when calling 'build_release_uvision.bat' in step 2.
This will install and build with the exact version of the python packages used to create that build.
