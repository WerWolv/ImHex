<p align="center">
  <a title="'Build' workflow Status" href="https://github.com/WerWolv/ImHex/actions?query=workflow%3ABuild"><img alt="'Build' workflow Status" src="https://img.shields.io/github/workflow/status/WerWolv/ImHex/Build?longCache=true&style=for-the-badge&label=Build&logoColor=fff&logo=GitHub%20Actions"></a><!--
  -->
</p>

# ImHex

A Hex Editor for Reverse Engineers, Programmers and people that value their eye sight when working at 3 AM.

## Supporting

If you like my work, consider supporting me on GitHub Sponsors, Patreon or PayPal. Thanks a lot!

<p align="center">
<a href="https://github.com/sponsors/WerWolv"><img src="https://werwolv.net/assets/github_banner.png" alt="GitHub donate button" /> </a>
<a href="https://www.patreon.com/werwolv"><img src="https://c5.patreon.com/external/logo/become_a_patron_button.png" alt="Patreon donate button" /> </a>
<a href="https://werwolv.net/donate"><img src="https://werwolv.net/assets/paypal_banner.png" alt="PayPal donate button" /> </a>
</p>

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
  - arrays, pointers, structs, unions, enums, bitfields, using declarations, little and big endian support
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
  - Highest and avarage entropy
  - Encrypted / Compressed file detection
- Helpful tools
  - Itanium and MSVC demangler
  - ASCII table
  - Regex replacer
  - Mathematical expression evaluator (Calculator)
  - Hexadecimal Color picker
- Built-in cheat sheet for pattern language and Math evaluator
- Doesn't burn out your retinas when used in late-night sessions

## Screenshots

![](https://i.imgur.com/xH7xJ4g.png)
![](https://i.imgur.com/fhVJYEa.png)

## Additional Files

For format patterns, includable libraries and magic files, check out the [ImHex-Patterns](https://github.com/WerWolv/ImHex-Patterns) repository. Feel free to PR your own files there as well!

## Compiling

You need a C++20 compatible compiler such as GCC 10.2.0 to compile ImHex. Moreover, the following dependencies are needed for compiling ImHex:

- GLFW3
- libmagic, libgnurx, libtre, libintl, libiconv
- libcrypto
- capstone
- libLLVMDemangle
- nlohmann json
- Python3

Find all-in-one dependency installation scripts for Arch Linux, Fedora, Debian/Ubuntu and/or MSYS2 in [dist](dist).

After all the dependencies are installed, run the following commands to build ImHex:

```sh
mkdir build
cd build
cmake ..
make -j
```

---

To create a standalone zipfile on Windows, get the Python standard library (e.g. from https://github.com/python/cpython/tree/master/Lib) and place the files and folders in `lib/python3.8` next to your built executable. Don't forget to also copy the `libpython3.8.dll` and `libwinpthread-1.dll` from your mingw setup next to the executable.

On both Windows and Linux:

- Copy the files from `python_libs` in the `lib` folder next to your built executable.
- Place your magic databases in the `magic` folder next to your built executable
- Place your patterns in the `pattern` folder next to your built executable
- Place your include pattern files in the `include` folder next to your built executable

## Credits

- Thanks a lot to ocornut for their amazing [Dear ImGui](https://github.com/ocornut/imgui) which is used for building the entire interface
  - Thanks to orconut as well for their hex editor view used as base for this project.
  - Thanks to BalazsJako for their incredible [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) used for the pattern language syntax highlighting
  - Thanks to AirGuanZ for their amazing [imgui-filebrowser](https://github.com/AirGuanZ/imgui-filebrowser) used for loading and saving files
- Thanks to nlohmann for their [json](https://github.com/nlohmann/json) library used for project files
- Thanks to aquynh for [capstone](https://github.com/aquynh/capstone) which is the base of the disassembly window
