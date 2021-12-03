# Building an AppImage
There are two ways of building an AppImage for ImHex, using the provided
tools here.

If you want to create an AppImage and do not have a build to work from
already, you can use docker to build ImHex and package an AppImage.

Alternatively you can create an AppImage using an existing build.

## Using docker
First run `build.sh` to create a docker image. Then run `extract.sh` to get the
AppImage out. This needs to be in two steps, as a docker build cannot copy
files out. Nor can docker build use volume mounts.

The environment variable TAG can be set to build for a specific git tag.
Without the master branch is build.

## Using an existing build
Run `package.sh` with the build dir as an argument. E.g.:
```
./package.sh ../../build
```
