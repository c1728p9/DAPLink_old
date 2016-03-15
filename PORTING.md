## Porting
Install necessary tools. Skip any step where a compatible tool already exists. All tools **MUST** be added to the system path

* Install [Python 2.7.9 or above](https://www.python.org/downloads/)
* Install [Git](https://git-scm.com/downloads)
* Install [Keil MDK-ARM](https://www.keil.com/download/product/)
* Install virtualenv in your global Python installation

1. Get the sources and create a virtual environment
```
$ git clone https://github.com/mbedmicro/DAPLink
$ pip install virtualenv
$ virtualenv venv
```

2. Update tools and generate project files. **This should be done everytime you pull new changes**
``` cmd Windows
$ "venv/Scripts/activate"
$ pip install -r requirements.txt
$ progen generate -t uvision
$ "venv/Scripts/deactivate"
```

``` terminal Mac
$ source bin/activate
$ pip install -r requirements.txt
$ progen generate -t uvision
$ deactivate
```
3. Pull requests should be made once a changeet is [rebased onto Master](https://www.atlassian.com/git/tutorials/merging-vs-rebasing/workflow-walkthrough)

## Adding New Boards

## Testing

## Release
Docs on [how to create a release](RELEASE.md)

