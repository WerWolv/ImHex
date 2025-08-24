{
  description = "ImHex";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin" ];
      forAllSystems = f: nixpkgs.lib.genAttrs supportedSystems (system: f system);
    in {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
          {
            default = pkgs.mkShell {
              buildInputs = [
                pkgs.cmake
                pkgs.clang
                pkgs.lld

                pkgs.nghttp3
                pkgs.pkg-config
                pkgs.glfw
                pkgs.fontconfig
                pkgs.file
                pkgs.mbedtls
                pkgs.freetype
                pkgs.dbus
                pkgs.gtk3
                pkgs.curl
                pkgs.fmt
                pkgs.yara
                pkgs.nlohmann_json
                pkgs.ninja
                pkgs.zlib
                pkgs.bzip2
                pkgs.xz
                pkgs.zstd
                pkgs.lz4
                pkgs.libssh2
                pkgs.md4c
              ];

              shellHook = ''
                export CC=${pkgs.clang}/bin/clang
                export CXX=${pkgs.clang}/bin/clang++
              '';
            };
          });
    };
}
