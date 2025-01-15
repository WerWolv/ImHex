<a href="https://imhex.werwolv.net">
  <h1 align="center">
    <picture>
      <img height="300px" style="margin: 0; padding: 0" src="./resources/dist/common/logo/ImHexLogoSVGBG.svg">
    </picture>
  </h1>
</a>

<p align="center">
    A Hex Editor for Reverse Engineers, Programmers and people who value their retinas when working at 3 AM.
    <br>
    <a href="https://itinerarium.github.io/phoneme-synthesis/?w=/'ˈɪmhɛks/"><strong>/ˈɪmhɛks/</strong></a>
</p>
<p align="center">
  <a title="'Build' workflow Status" href="https://github.com/WerWolv/ImHex/actions?query=workflow%3ABuild"><img alt="'Build' workflow Status" src="https://img.shields.io/github/actions/workflow/status/WerWolv/ImHex/build.yml?longCache=true&style=for-the-badge&label=Build&logoColor=fff&logo=GitHub%20Actions&branch=master"></a>
  <a title="Discord Server" href="https://discord.gg/X63jZ36xBY"><img alt="Discord Server" src="https://img.shields.io/discord/789833418631675954?label=Discord&logo=Discord&logoColor=fff&style=for-the-badge"></a>
  <a title="Total Downloads" href="https://github.com/WerWolv/ImHex/releases/latest"><img alt="Total Downloads" src="https://img.shields.io/github/downloads/WerWolv/ImHex/total?longCache=true&style=for-the-badge&label=Downloads&logoColor=fff&logo=GitHub"></a>
  <a title="Code Quality" href="https://www.codefactor.io/repository/github/werwolv/imhex"><img alt="Code Quality" src="https://img.shields.io/codefactor/grade/github/WerWolv/ImHex?longCache=true&style=for-the-badge&label=Code%20Quality&logoColor=fff&logo=CodeFactor&branch=master"></a>
  <a title="Translation" href="https://weblate.werwolv.net/projects/imhex/"><img alt="Translation" src="https://img.shields.io/weblate/progress/imhex?logo=weblate&logoColor=%23FFFFFF&server=https%3A%2F%2Fweblate.werwolv.net&style=for-the-badge"></a>
  <a title="Plugins" href="https://github.com/WerWolv/ImHex/blob/master/PLUGINS.md"><img alt="Plugins" src="https://img.shields.io/badge/Plugins-Supported-brightgreen?logo=stackedit&logoColor=%23FFFFFF&style=for-the-badge"></a>
</p>

<p align="center">
  <a title="Download the latest version of ImHex" href="https://imhex.download"><img alt="Download the latest version of ImHex!" src="resources/dist/common/get_release_banner.png"></a>
  <a title="Download the latest nightly pre-release version of ImHex" href="https://imhex.download/#nightly"><img alt="Download the latest nightly pre-release version of ImHex" src="resources/dist/common/get_nightly_banner.png"></a>
  <a title="Use the Web version of ImHex right in your browser!" href="https://web.imhex.werwolv.net"><img alt="Use the Web version of ImHex right in your browser!" src="resources/dist/common/try_online_banner.png"></a>
  <a title="Read the documentation of ImHex!" href="https://docs.werwolv.net"><img alt="Read the documentation of ImHex!" src="resources/dist/common/read_docs_banner.png"></a>
</p>

## Supporting

If you like my work, please consider supporting me on GitHub Sponsors, Patreon or PayPal. Thanks a lot!

<p align="center">
<a href="https://github.com/sponsors/WerWolv"><img src="https://werwolv.net/assets/github_banner.png" alt="GitHub donate button" /></a>
<a href="https://www.patreon.com/werwolv"><img src="https://c5.patreon.com/external/logo/become_a_patron_button.png" alt="Patreon donate button" /></a>
<a href="https://werwolv.net/donate"><img src="https://werwolv.net/assets/paypal_banner.png" alt="PayPal donate button" /></a>
</p>

## Screenshots
![Hex editor, patterns and data information](https://github.com/user-attachments/assets/902a7c4c-410d-490f-999e-14c856fec027)
![Bookmarks, data information, find view and data processor](https://github.com/user-attachments/assets/58eefa1f-31c9-4bb8-a1c1-8cdd8ddbd29f)

<details>
<summary><strong>More Screenshots</strong></summary>

![Data Processor decrypting some data and displaying it as an image](https://github.com/WerWolv/ImHex/assets/10835354/d0623081-3094-4840-a8a8-647b38724db8)
![STL Parser written in the Pattern Language visualizing a 3D model](https://github.com/WerWolv/ImHex/assets/10835354/62cbcd18-1c3f-4dd6-a877-2bf2bf4bb2a5)
![Data Information view displaying various stats about the file](https://github.com/WerWolv/ImHex/assets/10835354/d4706c01-c258-45c9-80b8-fe7a10d5a1de)

</details>

## Features

<details>
  <summary><strong>Featureful hex view</strong></summary>

  - Byte patching
  - Patch management
  - Infinite Undo/Redo
  - "Copy bytes as..."
    - Bytes
    - Hex string
    - C, C++, C#, Rust, Python, Java & JavaScript array
    - ASCII-Art hex view
    - HTML self-contained div
  - Simple string and hex search
  - Goto from start, end and current cursor position
  - Colorful highlighting
    - Configurable foreground highlighting rules
    - Background highlighting using patterns, find results and bookmarks
  - Displaying data as a list of many different types
    - Hexadecimal integers (8, 16, 32, 64 bit)
    - Signed and unsigned decimal integers (8, 16, 32, 64 bit)
    - Floats (16, 32, 64 bit)
    - RGBA8 Colors
    - HexII
    - Binary
  - Decoding data as ASCII and custom encodings
    - Built-in support for UTF-8, UTF-16, ShiftJIS, most Windows encodings and many more
  - Paged data view
</details>
<details>
  <summary><strong>Custom C++-like pattern language for parsing highlighting a file's content</strong></summary>
  
  - Automatic loading based on MIME types and magic values
  - Arrays, pointers, structs, unions, enums, bitfields, namespaces, little and big endian support, conditionals and much more!
  - Useful error messages, syntax highlighting and error marking
  - Support for visualizing many different types of data
    - Images
    - Audio
    - 3D Models
    - Coordinates
    - Time stamps
</details>
<details>
  <summary><strong>Theming support</strong></summary>

  - Doesn't burn out your retinas when used in late-night sessions
    - Dark mode by default, but a light mode is available as well
  - Customizable colors and styles for all UI elements through shareable theme files
  - Support for custom fonts
</details>
<details>
  <summary><strong>Importing and Exporting data</strong></summary>
  
  - Base64 files
  - IPS and IPS32 patches
  - Markdown reports
</details>
<details>
  <summary><strong>Data Inspector</strong></summary>

  - Interpreting data as many different types with endianness, decimal, hexadecimal and octal support and bit inversion
    - Unsigned and signed integers (8, 16, 24, 32, 48, 64 bit)
    - Floats (16, 32, 64 bit)
    - Signed and Unsigned LEB128
    - ASCII, Wide and UTF-8 characters and strings
    - time32_t, time64_t, DOS date and time
    - GUIDs
    - RGBA8 and RGB65 Colors
  - Copying and modifying bytes through the inspector
  - Adding new data types through the pattern language
  - Support for hiding rows that aren't used
</details>
<details>
  <summary><strong>Node-based data pre-processor</strong></summary>

  - Modify, decrypt and decode data before it's being displayed in the hex editor
  - Modify data without touching the underlying source
  - Support for adding custom nodes
</details>
<details>
  <summary><strong>Loading data from many different data sources</strong></summary>

  - Local Files
    - Support for huge files with fast and efficient loading
  - Raw Disks
    - Loading data from raw disks and partitions
  - GDB Server
    - Access the RAM of a running process or embedded devices through GDB
  - Intel Hex and Motorola SREC data
  - Process Memory
    - Inspect the entire address space of a running process
</details>
<details>
  <summary><strong>Data searching</strong></summary>
  
  - Support for searching the entire file or only a selection
  - String extraction
    - Option to specify minimum length and character set (lower case, upper case, digits, symbols)
    - Option to specify encoding (ASCII, UTF-8, UTF-16 big and little endian)
  - Sequence search
    - Search for a sequence of bytes or characters
    - Option to ignore character case
  - Regex search
    - Search for strings using regular expressions
  - Binary Pattern
    - Search for sequences of bytes with optional wildcards
  - Numeric Value search
    - Search for signed/unsigned integers and floats
    - Search for ranges of values
    - Option to specify size and endianness
    - Option to ignore unaligned values
</details>
<details>
  <summary><strong>Data hashing support</strong></summary>

  - Many different algorithms available
    - CRC8, CRC16 and CRC32 with custom initial values and polynomials
      - Many default polynomials available
    - MD5
    - SHA-1, SHA-224, SHA-256, SHA-384, SHA-512
    - Adler32
    - AP
    - BKDR
    - Bernstein, Bernstein1
    - DEK, DJB, ELF, FNV1, FNV1a, JS, PJW, RS, SDBM
    - OneAtTime, Rotating, ShiftAndXor, SuperFast
    - Murmur2_32, MurmurHash3_x86_32, MurmurHash3_x86_128, MurmurHash3_x64_128
    - SipHash64, SipHash128
    - XXHash32, XXHash64
    - Tiger, Tiger2
    - Blake2B, Blake2S
  - Hashing of specific regions of the loaded data
  - Hashing of arbitrary strings
</details>
<details>
  <summary><strong>Diffing support</strong></summary>

  - Compare data of different data sources
  - Difference highlighting
  - Table view of differences
</details>
<details>
  <summary><strong>Integrated disassembler</strong></summary>
  
  - Support for all architectures supported by Capstone
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
    - RISC-V
    - WebAssembly
    - MOS65XX
    - Berkeley Packet Filter
</details>
<details>
  <summary><strong>Bookmarks</strong></summary>

  - Support for bookmarks with custom names and colors
  - Highlighting of bookmarked region in the hex editor
  - Jump to bookmarks
  - Open content of bookmark in a new tab
  - Add comments to bookmarks
</details>
<details>
  <summary><strong>Featureful data analyzer and visualizer</strong></summary>

  - File magic-based file parser and MIME type database
  - Byte type distribution graph
  - Entropy graph
  - Highest and average entropy
  - Encrypted / Compressed file detection
  - Digram and Layered distribution graphs
</details>
<details>
  <summary><strong>YARA Rule support</strong></summary>

  - Scan a file for vulnerabilities with official yara rules
  - Highlight matches in the hex editor
  - Jump to matches
  - Apply multiple rules at once
</details>
<details>
  <summary><strong>Helpful tools</strong></summary>

  - Itanium, MSVC, Rust and D-Lang demangler based on LLVM
  - ASCII table
  - Regex replacer
  - Mathematical expression evaluator (Calculator)
  - Graphing calculator
  - Hexadecimal Color picker with support for many different formats
  - Base converter
  - Byte swapper
  - UNIX Permissions calculator
  - Wikipedia term definition finder
  - File utilities
    - File splitter
    - File combiner
    - File shredder
  - IEEE754 Float visualizer
  - Division by invariant multiplication calculator
  - TCP Client/Server
  - Euclidean algorithm calculator
</details>
<details>
  <summary><strong>Built-in Content updater</strong></summary>

  - Download all files found in the database directly from within ImHex
    - Pattern files for decoding various file formats
    - Libraries for the pattern language
    - Magic files for file type detection
    - Custom data processor nodes
    - Custom encodings
    - Custom themes
    - Yara rules
</details>
<details>
  <summary><strong>Modern Interface</strong></summary>

  - Support for multiple workspaces
  - Support for custom layouts
  - Detachable windows
</details>
<details>
  <summary><strong>Easy to get started</strong></summary>

  - Support for many different languages
  - Simplified mode for beginners
  - Extensive documentation
  - Many example files available on [the Database](https://github.com/WerWolv/ImHex-Patterns)
  - Achievements guiding you through the features of ImHex
  - Interactive tutorials
</details>

## Pattern Language

The Pattern Language is the completely custom programming language developed for ImHex.
It allows you to define structures and data types in a C-like syntax and then use them to parse and highlight a file's content.

- Source Code: [Link](https://github.com/WerWolv/PatternLanguage/)
- Documentation: [Link](https://docs.werwolv.net/pattern-language/)

## Database

For format patterns, libraries, magic and constant files, check out the [ImHex-Patterns](https://github.com/WerWolv/ImHex-Patterns) repository. 

**Feel free to PR your own files there as well!**

## Requirements

To use ImHex, the following minimal system requirements need to be met.

> [!IMPORTANT]
> ImHex requires a GPU with OpenGL 3.0 support in general.
> There are releases available (with the `-NoGPU` suffix) that are software rendered and don't require a GPU, however these can be a lot slower than the GPU accelerated versions.
> 
> If possible at all, make ImHex use the dedicated GPU on your system instead of the integrated one.
> ImHex will usually run fine with integrated GPUs as well but certain Intel HD GPU drivers on Windows are known to cause graphical artifacts.

- **OS**: 
  - **Windows**: Windows 7 or higher (Windows 10/11 recommended)
  - **macOS**: macOS 13 (Ventura) or higher, 
    - Lower versions should still work too, but you'll need to compile ImHex yourself. The release binaries will NOT work.
    - The macOS build is not signed and will require you to manually allow them in the Security & Privacy settings.
  - **Linux**: "Modern" Linux. The following distributions have official releases available. Other distros are supported through the AppImage and Flatpak releases.
    - Ubuntu and Debian
    - Fedora
    - RHEL/AlmaLinux
    - Arch Linux
    - Basically any other distro will work as well when compiling ImHex from sources.
- **CPU**: Officially supported are x86_64 and ARM64, though any Little Endian 64 bit CPU should work.
- **GPU**: OpenGL 3.0 or higher 
  - Integrated Intel HD iGPUs are supported, however certain drivers are known to cause various graphical artifacts, especially on Windows. Use at your own risk.
  - In case you don't have a GPU available, there are software rendered releases available for Windows and macOS
- **RAM**: ~150MiB, more is required for more complex analysis
- **Storage**: 150MiB

## Installing

Information on how to install ImHex can be found in the [Install](/INSTALL.md) guide

## Compiling

To compile ImHex on any platform, GCC (or Clang) is required with a version that supports C++23 or higher. 
Windows and Linux releases are being built using latest available GCC.
MacOS releases are being built using latest available LLVM Clang.

Important to note is, the MSVC and AppleClang compilers are both **NOT** supported since they're both generally severely outdated and lack features GCC and LLVM Clang have.

> [!NOTE]
> Many dependencies are bundled into the repository using submodules so make sure to clone it using the `--recurse-submodules` option.
> All dependencies that aren't bundled, can be installed using the dependency installer scripts found in the `/dist` folder.

For more information, check out the [Compiling](/dist/compiling) guide.

## Contributing
See [Contributing](/CONTRIBUTING.md)

## Plugin development

To develop plugins for ImHex, use the following template project to get started. You then have access to the entirety of libimhex as well as the ImHex API and the Content Registry to interact with ImHex or to add new content.
- [ImHex Plugin Template](https://github.com/WerWolv/ImHex-Plugin-Template)


## Credits

### Contributors

- [iTrooz](https://github.com/iTrooz) for getting ImHex onto the Web as well as hundreds of contributions in every part of the project
- [jumanji144](https://github.com/jumanji144) for huge contributions to the Pattern Language and ImHex's infrastructure
- [Mary](https://github.com/marysaka) for her immense help porting ImHex to MacOS and help during development
- [Roblabla](https://github.com/Roblabla) for adding MSI Installer support to ImHex
- [Mailaender](https://github.com/Mailaender) for getting ImHex onto Flathub
- Everybody else who has reported issues on Discord or GitHub that I had great conversations with :)

### Dependencies

- Thanks a lot to ocornut for their amazing [Dear ImGui](https://github.com/ocornut/imgui) which is used for building the entire interface
  - Thanks to epezent for [ImPlot](https://github.com/epezent/implot) used to plot data in various places
  - Thanks to Nelarius for [ImNodes](https://github.com/Nelarius/imnodes) used as base for the data processor 
  - Thanks to BalazsJako for [ImGuiColorTextEdit](https://github.com/BalazsJako/ImGuiColorTextEdit) used for the pattern language syntax highlighting
- Thanks to nlohmann for their [json](https://github.com/nlohmann/json) library used for configuration files
- Thanks to vitaut for their [libfmt](https://github.com/fmtlib/fmt) library which makes formatting and logging so much better
- Thanks to btzy for [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) and their great support, used for handling file dialogs on all platforms
- Thanks to danyspin97 for [xdgpp](https://sr.ht/~danyspin97/xdgpp) used to handle folder paths on Linux
- Thanks to aquynh for [capstone](https://github.com/aquynh/capstone) which is the base of the disassembly window
- Thanks to rxi for [microtar](https://github.com/rxi/microtar) used for extracting downloaded store assets 
- Thanks to VirusTotal for [Yara](https://github.com/VirusTotal/yara) used by the Yara plugin
- Thanks to Martinsos for [edlib](https://github.com/Martinsos/edlib) used for sequence searching in the diffing view
- Thanks to ron4fun for [HashLibPlus](https://github.com/ron4fun/HashLibPlus) which implements every hashing algorithm under the sun
- Thanks to mackron for [miniaudio](https://github.com/mackron/miniaudio) used to play audio files
- Thanks to all other groups and organizations whose libraries are used in ImHex

### License

The biggest part of ImHex is under the GPLv2-only license. 
Notable exceptions to this are the following parts which are under the LGPLv2.1 license:
- **/lib/libimhex**: The library that allows Plugins to interact with ImHex.
- **/plugins/ui**: The UI plugin library that contains some common UI elements that can be used by other plugins

The reason for this is to allow for proprietary plugins to be developed for ImHex.
