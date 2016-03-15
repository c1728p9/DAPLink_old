## Release
1. Create a tag with the correct release version and push it to github
2. Clean the repo you will be building from by running 'git clean -xdf' followed by 'git reset --hard'
3. Run the script 'build_release_uvision.bat' to create all builds.
4. All release deliverables will be created and stored in 'uvision_release'.  Save this wherever your builds are stored.

Note: A previous build can be reproduced by using the 'build_requirements.txt' of that build.
To do this add the additional argument 'build_requirements.txt' when calling 'build_release_uvision.bat' in step 2.
This will install and build with the exact version of the python packages used to create that build.