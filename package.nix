{
  lib,
  stdenv,
  cmake,
  ninja,
  dbus,
  kdePackages,
}:

stdenv.mkDerivation {
  pname = "plasma-iwd";
  version = "0.1.0";

  # Explicit file set so local build/, .prefix/ and result never leak into the source.
  src = lib.fileset.toSource {
    root = ./.;
    fileset = lib.fileset.unions [
      ./CMakeLists.txt
      ./package
      ./src
      ./tests
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
    kdePackages.extra-cmake-modules
    kdePackages.wrapQtAppsHook
  ];

  buildInputs = [
    kdePackages.qtbase
    kdePackages.qtdeclarative
    kdePackages.libplasma
    kdePackages.kirigami
  ];

  nativeCheckInputs = [ dbus ];

  # Only a QML plugin and a Plasma package; nothing to wrap.
  dontWrapQtApps = true;

  doCheck = true;

  meta = {
    description = "Plasma 6 system tray applet for iwd";
    license = lib.licenses.gpl2Plus;
    platforms = lib.platforms.linux;
  };
}
