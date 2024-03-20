For a TLDR of commands see [How to build](#How-to-build)

# Introduction

The original CI we used (vanilla Github Actions) was great for specifying what steps to execute to build packages. It could even do some custom steps with reusable actions. But it had problem: no local reproducibility. This meant that:
- We couldn't test code properly locally, we were dependent on GitHub to do it
- If something was wrong and we had to debug the build script, it was *long and painful* because we had to wait for Github runners to finish builds, and couldn't quickly iterate

To solve this, we are now trying to move the CI build script to docker containers (so using Dockerfiles)

# How to build

Commands are available in the [CI](../../.github/workflows/build.yml) and you should prefer copying them from there.
But here is a general command that should work for every build we have:
```
docker buildx build . -f <DOCKERFILE_PATH> --progress plain --build-arg 'JOBS=4' --build-arg 'BUILD_TYPE=Debug' --build-context imhex=$(pwd) --output local
```

where `<DOCKERFILE_PATH>` should be replaced by the wanted Dockerfile base d on the build you want to do:

| Wanted build | Dockerfile path             | Target |
|--------------|-----------------------------|--------|
| MacOS M1     | dist/macOS/arm64.Dockerfile | -      |
| AppImage     | dist/appimage/Dockerfile    | -      |
| Web version  | dist/web/Dockerfile         | raw    |

We'll explain this command in the next section

# Useful knowledge about Docker builds

Docker-based builds work with a Dockerfile. You run the Dockerfile, it builds the package.

We are using a base environment (often given to us by dockerhub) (e.g. ubuntu:22.04) which is really just a root filesystem, and we then run shell commands in that env, just like a shell script

Docker-based builds have two kind of caches used:
- layer cache, which mean that if a layer (instruction) hasn't been changed, and previous layers haven't changed, it will not be run again
    - a `COPY` layer will be invalidated if one of the file copied has changed
- mount cache, which are per-instructions mounts that will be cached and restored in the next run. Mounts on different folders will not collide

Docker cache tends to grow very quickly when constantly making changes in the Dockerfile and rebuilding (a.k.a debugging what's going on), you can clear it with something like `docker system prune -a`

In the command saw earlier:
- `.` is the base folder that the Dockerfile will be allowed to see
- `-f <path>` is to specify the Dockerfile path
- `--progress plain` is to allow you to see the output of instructions
- `--build-arg <key>=<value>` is to allow to specify arguments to the build (like -DKEY=VALUE in CMake)
- `--build-context key=<folder>` is to specify folders other than the base folder that the Dockerfile is allowed to see
- `--output <path>` is the path to write the output package to. If not specified, Docker will create an image as the output (probably not what you want)
- `--target <target>` specifies which docker target to build
