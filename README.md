<a href="https://imhex.werwolv.net"><h1 align="center" >:mag: ImHex</h1></a>

<p align="center">A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.</p>

<p align="center">
  <a title="'Build' workflow Status" href="https://github.com/WerWolv/ImHex/actions?query=workflow%3ABuild"><img alt="'Build' workflow Status" src="https://img.shields.io/github/workflow/status/WerWolv/ImHex/Build?longCache=true&style=for-the-badge&label=Build&logoColor=fff&logo=GitHub%20Actions"></a>
  <a title="Discord Server" href="https://discord.gg/X63jZ36xBY"><img alt="Discord Server" src="https://img.shields.io/discord/789833418631675954?label=Discord&logo=Discord&style=for-the-badge"></a>
  <a title="Total Downloads" href="https://github.com/WerWolv/ImHex/releases/latest"><img alt="Total Downloads" src="https://img.shields.io/github/downloads/WerWolv/ImHex/total?longCache=true&style=for-the-badge&label=Downloads&logoColor=fff&logo=GitHub"></a>
</p>

## Supporting

If you like my work, please consider supporting me on GitHub Sponsors, Patreon or PayPal. Thanks a lot!

<p align="center">
<a href="https://github.com/sponsors/WerWolv"><img src="https://werwolv.net/assets/github_banner.png" alt="GitHub donate button" /> </a>
<a href="https://www.patreon.com/werwolv"><img src="https://c5.patreon.com/external/logo/become_a_patron_button.png" alt="Patreon donate button" /> </a>
<a href="https://werwolv.net/donate"><img src="https://werwolv.net/assets/paypal_banner.png" alt="PayPal donate button" /> </a>
</p>

## Screenshots

![Hex editor, patterns and data information](https://user-images.githubusercontent.com/10835354/139717326-8044769d-527b-4d88-8adf-2d4ecafdca1f.png)
![Bookmarks, disassembler and data processor](https://user-images.githubusercontent.com/10835354/139717323-1f8c9d52-f7eb-4f43-9f11-097ac728ed6c.png)

## Features

- Featureful hex view
  - Byte patching
  - Patch management
  - Copy bytes as feature
    - Bytes
    - Hex string
    - C, C++, C#, Rust, Python, Java & JavaScript array
    - ASCII-Art hex view
    - HTML self contained div
  - String and hex search
  - Colorful highlighting
  - Goto from start, end and current cursor position
- Custom C++-like pattern language for parsing highlighting a file's content
  - Automatic loading based on MIME type
  - arrays, pointers, structs, unions, enums, bitfields, namespaces, little and big endian support, conditionals and much more!
  - Useful error messages, syntax highlighting and error marking
- Data importing
  - Base64 files
  - IPS and IPS32 patches
- Data exporting
  - IPS and IPS32 patches
- Data inspector allowing interpretation of data as many different types (little and big endian)
- Huge file support with fast and efficient loading
- String search
  - Copying of strings
  - Copying of demangled strings
- File hashing support
  - CRC16 and CRC32 with custom initial values and polynomials
  - MD4, MD5
  - SHA-1, SHA-224, SHA-256, SHA-384, SHA-512
- Disassembler supporting many different architectures
  - ARM32 (ARM, Thumb, Cortex-M, AArch32)
  - ARM64
  - MIPS (MIPS32, MIPS64, MIPS32R6, Micro)
  - x86 (16-bit, 32-bit, 64-bit)
  - PowerPC (32-bit, 64-bit)
  - SPARC
  - IBM SystemZ
  - xCORE
  - M68K
  - TMS320C64X
  - M680X
  - Ethereum
- Bookmarks
  - Region highlighting
  - Comments
- Data Analyzer
  - File magic-based file parser and MIME type database
  - Byte distribution graph
  - Entropy graph
  - Highest and average entropy
  - Encrypted / Compressed file detection
- Built-in Content Store
  - Download all files found in the database directly from within ImHex
- Yara Rules support
  - Quickly scan a file for vulnearabilities with official yara rules
- Helpful tools
  - Itanium and MSVC demangler
  - ASCII table
  - Regex replacer
  - Mathematical expression evaluator (Calculator)
  - Hexadecimal Color picker
  - Base converter
  - UNIX Permissions calculator
  - Anonfiles File upload tool
  - Wikipedia term definition finder
  - File utilities
    - File splitter
    - File combiner
    - File shredderer
- Built-in cheat sheet for pattern language and Math evaluator
- Doesn't burn out your retinas when used in late-night sessions

## Pattern Language

The custom C-like Pattern Language developed and used by ImHex is easy to read, understand and learn. A guide with all features of the language can be found [on the docs page](http://imhex.werwolv.net/docs).

## Plugin development

To develop plugins for ImHex, use one of the following two templates projects to get startet. You then have access to the entirety of libimhex as well as the ImHex API and the Content Registry to interact with ImHex or to add new content.
- [C++ Plugin Template](https://github.com/WerWolv/ImHex-Cpp-Plugin-Template)
- [Rust Plugin Template](https://github.com/WerWolv/ImHex-Rust-Plugin-Template)

## Additional Files

For format patterns, includable libraries and magic files, check out the [ImHex-Patterns](https://github.com/WerWolv/ImHex-Patterns) repository. Feel free to PR your own files there as well!

## Nightly builds

Nightlies are available via GitHub Actions [here](https://github.com/WerWolv/ImHex/actions?query=workflow%3ABuild).

- Windows • __x86_64__
  - [MSI](https://nightly.link/WerWolv/ImHex/workflows/build/master/Windows%20Installer.zip)
  - [EXE](https://nightly.link/WerWolv/ImHex/workflows/build/master/Windows%20Portable%20ZIP.zip)
- MacOS • __x86_64__
  - [DMG](https://nightly.link/WerWolv/ImHex/workflows/build/master/macOS%20DMG.zip)
- Linux • __x86_64__
  - [ELF](https://nightly.link/WerWolv/ImHex/workflows/build/master/Linux%20ELF.zip)
  - [DEB](https://nightly.link/WerWolv/ImHex/workflows/build/master/Linux%20DEB.zip)
  - [Flatpak](https://nightly.link/WerWolv/ImHex/workflows/build/master/Linux%20Flatpak.zip)
  - [AppImage](https://nightly.link/WerWolv/ImHex/workflows/build/master/Linux%20AppImage.zip)

## Compiling

You need a C++20 compatible compiler such as GCC 10.2.0 to compile ImHex.

Many dependencies are bundled into the repository using submodules so make sure to clone it using the `--recurse-submodules` option.
All dependencies that aren't bundled, can be installed using the dependency installer scripts found in the `/dist` folder.

For working examples 

### Windows

On Windows, ImHex is built through msys2 / mingw. To install all dependencies, open a mys2 window and run the PKGCONFIG script in the [dist/msys2](dist/msys2) folder.
After all the dependencies are installed, run the following commands to build ImHex:

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

---

To create a standalone zipfile on Windows, get the Python standard library (e.g. from https://github.com/python/cpython/tree/master/Lib) and place the files and folders in `lib/python3.8` next to your built executable. Don't forget to also copy the `libpython3.8.dll` and `libwinpthread-1.dll` from your mingw setup next to the executable.

- Copy the files inside the `/resources/lib/python` folder into the `lib` folder next to your built executable.
- Place your magic databases in the `magic` folder next to your built executable
- Place your patterns in the `pattern` folder next to your built executable
- Place your include pattern files in the `include` folder next to your built executable

### macOS

To build ImHex on macOS, run the following commands:

```sh
brew bundle --no-lock --file dist/Brewfile
mkdir build
cd build
CC=$(brew --prefix llvm)/bin/clang CXX=$(brew --prefix llvm)/bin/clang++ PKG_CONFIG_PATH="$(brew --prefix openssl)/lib/pkgconfig":"$(brew --prefix)/lib/pkgconfig" cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

Install the ImHex executable as well as libimhex.dylib to wherever ImHex should be installed.

All other files belong in `~/Library/Application Support/imhex`:
```
Patterns: ~/Library/Application Support/imhex/patterns
Pattern Includes: ~/Library/Application Support/imhex/includes
Magic files: ~/Library/Application Support/imhex/magic
Python: ~/Library/Application Support/imhex/lib/pythonX.X
Plugins: ~/Library/Application Support/imhex/plugins
Configuration: ~/Library/Application Support/imhex/config
Resources: ~/Library/Application Support/imhex/resources
```

### Linux

Dependency installation scripts are available for many common Linux distributions in the [/dist](dist) folder.
After all the dependencies are installed, run the following commands to build ImHex:

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
```

---

Put the ImHex executable into the `/usr/bin` folder.
Put libimhex.so into the `/usr/lib` folder.
Configuration files go to `/etc/xdg/imhex` or `~/.config/imhex`.
All other files belong in `/usr/share/imhex` or `~/.local/share/imhex`:

```
Patterns: /usr/share/imhex/patterns
Pattern Includes: /usr/share/imhex/includes
Magic files: /usr/share/imhex/magic
Python: /usr/share/imhex/lib/pythonX.X
Plugins: /usr/share/imhex/plugins
Configuration: /etc/xdg/imhex/config
Resources: /usr/share/imhex/resources
```

All paths follow the XDG Base Directories standard, and can thus be modified
with the environment variables `XDG_CONFIG_HOME`, `XDG_CONFIG_DIRS`,
`XDG_DATA_HOME` and `XDG_DATA_DIRS`.

## Credits

### Contributors

- [Mary](https://github.com/Thog) for her immense help porting ImHex to MacOS and help during development
- [Roblabla](https://github.com/Roblabla) for adding MSI Installer support to ImHex
- [jam1garner](https://github.com/jam1garner) and [raytwo](https://github.com/raytwo) for their help with adding Rust support to plugins
- All other people that have been reporting issues on Discord or GitHub that I had great conversations with :)

### Libraries

- Thanks a lot to ocornut for their amazing [Dear ImGui](https://github.com/ocornut/imgui) which is used for building the entire interface
  - Thanks to ocornut as well for their hex editor view used as base for this project.
  - Thanks to BalazsJako for their incredible [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) used for the pattern language syntax highlighting
- Thanks to nlohmann for their [json](https://github.com/nlohmann/json) library used for project files
- Thanks to aquynh for [capstone](https://github.com/aquynh/capstone) which is the base of the disassembly window
- Thanks to vitaut for their [libfmt](https://github.com/fmtlib/fmt) library which makes formatting and logging so much better
- Thanks to rxi for [microtar](https://github.com/rxi/microtar) used for extracting downloaded store assets 
