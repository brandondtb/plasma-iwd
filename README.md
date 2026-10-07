# plasma-iwd

A Plasma 6 system tray applet for [iwd](https://iwd.wiki.kernel.org/). It shows Wi-Fi status and lets you scan, connect, disconnect, manage saved networks, join hidden networks, and toggle Wi-Fi power. It talks to iwd directly over D-Bus, so NetworkManager is not involved.

Your user needs D-Bus access to `net.connman.iwd`. iwd's stock policy grants it to the `wheel` group. 802.1X/enterprise networks have to be provisioned with iwd config files; once provisioned, they connect from the applet.

## Install (NixOS)

Build it with your system's `pkgs`. The plugin is loaded into plasmashell, so it must link the same Qt; a copy built from a different nixpkgs can crash plasmashell.

```nix
# configuration.nix
environment.systemPackages = [ (pkgs.callPackage /home/brandon/src/plasma-iwd/package.nix { }) ];
```

If your system config is a flake, add this repo as an input with `inputs.nixpkgs.follows = "nixpkgs"` and use its `packages.<system>.default`.

After rebuilding, restart plasmashell (`systemctl --user restart plasma-plasmashell`). Then enable **plasma-iwd** under System Tray Settings → Entries.

## Development

```sh
nix develop
cmake --preset dev && cmake --build --preset dev
ctest --preset dev                       # runs against an in-process fake iwd on a private session bus
cmake --install build                    # installs into .prefix/, which the dev shell puts on the QML/XDG paths
plasmawindowed io.radiation.plasma-iwd   # run the applet against the real iwd
lint-qml                                 # qmllint with the dev shell's import paths
cmake --build build --target clang-format
qmlformat -i package/contents/ui/*.qml
```

## Layout

- `src/` contains the C++ QML module `io.radiation.plasmaiwd`, which exposes the `Iwd` singleton:
  - `IwdClient` mirrors iwd's ObjectManager tree.
  - `IwdManager` exposes the state and the actions.
  - `NetworkModel` and `KnownNetworkModel` are the list models.
  - `Agent` answers iwd's passphrase requests and handles signal-level callbacks.
- `package/` holds the Plasma applet package (QML UI and metadata).
- `tests/` holds the QtTest suite and `FakeIwd`, an in-process iwd stand-in.
