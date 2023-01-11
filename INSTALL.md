# Installing ImHex

## Official Releases

The easiest way to install ImHex is to download the latest release from the [GitHub Releases page](https://github.com/WerWolv/ImHex/releases/latest).

There's also a NoGPU version available for users who don't have a GPU or want to run ImHex in a VM without GPU passthrough.

## Nightly Builds

The GitHub Actions CI builds a new release package on every commit made to repository. These builds are available on the [GitHub Actions page](https://github.com/WerWolv/ImHex/actions?query=workflow%3A%22Build%22).
These builds are not guaranteed to be stable and may contain bugs, however they also contain new features that are not yet available in the official releases.

## Building from source

Build instructions for Windows, Linux and macOS can be found under `/dist/compiling`:
- Windows: [Link](dist/compiling/windows.md)
- macOS: [Link](dist/compiling/macos.md)
- Linux: [Link](dist/compiling/linux.md)

## Package managers

ImHex is also available on various package managers. The officially supported ones are listed here:

### Windows

- **Chocolatey**
  - [imhex](https://community.chocolatey.org/packages/imhex) (Thanks to @Jarcho)
    - `choco install imhex`
- **winget**
  - [WerWolv.ImHex](https://github.com/microsoft/winget-pkgs/tree/master/manifests/w/WerWolv/ImHex)
    - `winget install WerWolv.ImHex`

### Linux
- **Arch Linux AUR**
    - [imhex-bin](https://aur.archlinux.org/packages/imhex-bin/) (Thanks to @iTrooz)
      - `yay -S imhex-bin`
    - [imhex](https://aur.archlinux.org/packages/imhex/) (Thanks to @KokaKiwi)
      - `yay -S imhex`
- **Fedora**
  - [imhex](https://src.fedoraproject.org/rpms/imhex/) (Thanks to @jonathanspw)
    - `dnf install imhex`
- **Flatpak**
  - [net.werwolv.Imhex](https://flathub.org/apps/details/net.werwolv.ImHex) (Thanks to @Mailaender)
    - `flatpak install flathub net.werwolv.ImHex`

### Available on other package managers

Packages that aren't explicitly mentioned above are not officially supported but they will most likely still work.
Contact the maintainer of the package if you have any issues.

[![Packaging status](https://repology.org/badge/vertical-allrepos/imhex.svg)](https://repology.org/project/imhex/versions)
