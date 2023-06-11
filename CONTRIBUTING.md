# Contribution guide

## Introduction

This document is a guide for developers who want to contribute to ImHex in any way. It contains information about the codebase, the build process and the general workflow.

## Making Changes

### Adding new features

If you'd like to add new features, the best way to start is by joining our Discord and telling us about your idea. We can then discuss the best way to implement it and how it should be integrated into ImHex or if it should be done in a separate plugin.

There are standalone plugin templates that use ImHex as a submodule. You can find them here:
- https://github.com/WerWolv/ImHex-Cpp-Plugin-Template
- https://github.com/WerWolv/ImHex-Rust-Plugin-Template

### Adding a new language

If you'd like to support a new language in ImHex, the best way is by using the `dist/langtool.py` tool. It will create the necessary file for you and help you fill them out.
First, run the tool with `python3 dist/langtool.py create plugins/builtin/romfs/lang <iso_code>` where `<iso_code>` is the ISO 639-1 code of your language. This will create a new file in the language directory.
Afterwards follow the prompts of the program to populate the entire file. Once you're done, rerun cmake and rebuild ImHex. Your language should now be available in the settings.

### Updating an existing language

If you'd like to add missing keys to an existing language, you can also use the `dist/langtool.py` tool. Run it with `python3 dist/langtool.py translate plugins/builtin/romfs/lang <iso_code>` where `<iso_code>` is the ISO 639-1 code of the language.
This will one by one list all the missing translation keys that are present in the default translation file, and you can fill them in with the correct translation for your language.

## Codebase

ImHex is written in C++ and usually uses the latest compiler and standard library features available in gcc on all supported OSes. At the time of writing this is C++23.

### Structure

- `main`: Contains the main application code
    - Important to understand here is that the main ImHex application is basically just an empty shell. 
    - All it does is create a Window and a OpenGL context using GLFW, load all available plugins, properly configure ImGui and render it to the screen.
    - Everything else is done inside of plugins. ImHex comes with a few plugins by default, most notably the `builtin` plugin which contains the majority of the application code.
    - In most cases, this code doesn't need to be modified. Most features should be self-contained inside a plugin.
- `lib`
    - `libimhex`: Contains all helper utilities as well as various APIs for plugins to interact with ImHex.
        - The library's main purpose is for Dependency Inversion. The ImHex main application as well as libimhex do not know about the existence of plugins at build time. Plugins and the main application instead link against libimhex and use it as a common API to interact with each other.
        - Since libimhex is a doesn't know about the existence of plugins, it cannot depend on any of them. This includes localizations and things that get registered by plugins after launch.
        - Even if the builtin plugin is technically always available, it is still a plugin and should be treated that way.
        - All important APIs can be found in the `hex/api` include directory and are documented in the respective header file.
    - `external`: All libraries that need custom patches or aren't typically available in package managers go into here.
        - If you'd like to add new features to the Pattern language, please make a PR to https://github.com/WerWolv/PatternLanguage instead. ImHex usually depends on the latest commit of the master branch of this repo. 
- `plugins`
    - `builtin`: The builtin plugin. Contains the majority of the application code.
        - It's the heart of ImHex's functionality. It contains most of the default views, providers, etc. so if you want to add new functionality to ImHex, this is the place to start.
- `tests`: Contains all unit tests for ImHex. These are run automatically by the CI and should be kept up to date, especially when adding new helper functions to libimhex.

### RomFS

ImHex uses a custom library called [libromfs](https://github.com/WerWolv/libromfs). It's a simple static library which uses CMake's code generation feature to embed files into the binary at compile time so they can be accessed at runtime.
All plugins have a `romfs` folder which contains all files that should be embedded into the binary. Resources that need to be embedded into the main application (this is usually not necessary), go into the `resources/romfs` folder.
When adding, changing files or removing files, make sure to re-run CMake to update the generated code. Otherwise, the changes might not be reflected in the binary.

## Development Environment

I personally use CLion for development since it makes configuring and building the project very easy on all platforms.

### Windows
- Install MSYS2 from https://www.msys2.org/ and use the `dist/get_deps_msys2.sh` script to install all dependencies.
### Linux
- Install all dependencies using one of the `dist/get_deps_*.sh` scripts depending on your distribution or install them manually with your package manager.
### macOS
- Install all dependencies using brew and the `dist/Brewfile` script.

