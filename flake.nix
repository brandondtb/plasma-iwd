{
  description = "Plasma 6 system tray applet for iwd";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      packages = forAllSystems (pkgs: {
        default = pkgs.callPackage ./package.nix { };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell rec {
          # QML modules the applet imports, so qmllint/qmlls resolve them outside a Nix build.
          qmlDeps = pkgs.lib.makeSearchPath "lib/qt-6/qml" (
            with pkgs.kdePackages;
            [
              qtdeclarative
              libplasma
              kirigami.unwrapped
              ksvg
              kitemmodels
            ]
          );
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.default ];
          packages = with pkgs; [
            clang-tools
            dbus
            (writeShellScriptBin "lint-qml" ''
              args=()
              IFS=: read -ra dirs <<< "$QML_IMPORT_PATH"
              for d in "''${dirs[@]}"; do args+=(-I "$d"); done
              exec qmllint "''${args[@]}" "$@" $(find package -name '*.qml')
            '')
            kdePackages.plasma-sdk
          ];
          shellHook = ''
            export PREFIX="$PWD/.prefix"
            export QML_IMPORT_PATH="$PREFIX/lib/qt-6/qml:${qmlDeps}''${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}"
            export QML2_IMPORT_PATH="$PREFIX/lib/qt-6/qml''${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
            export XDG_DATA_DIRS="$PREFIX/share''${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
          '';
        };
      });

      formatter = forAllSystems (pkgs: pkgs.nixfmt-rfc-style);
    };
}
