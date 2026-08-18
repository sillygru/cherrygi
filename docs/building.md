# Building cherrygi 🍒

This guide covers the prerequisites, dependencies, and instructions for compiling **cherrygi** from source.

---

## 🛠️ Requirements & Dependencies

To build cherrygi, you will need:

- **CMake** >= 3.20
- **C++ Compiler** with C++20 support (GCC 12+, Clang 15+)
- **Qt 6** (Core, Gui, Network, Qml, Quick, QuickControls2, QuickEffects, Widgets) >= 6.5
- **KDE Frameworks 6 (KF6)**:
  - `KF6Kirigami`
  - `KF6CoreAddons`
  - `KF6I18n`
  - `KF6IconThemes`
  - `KF6ColorScheme`
  - `KF6Config`
- **Extra CMake Modules (ECM)**
- **zlib** (used for direct read of compressed Git objects)
- **Git** CLI (runtime dependency for mutating operations)

### Installing Dependencies by Distribution

#### Arch Linux / Manjaro
```bash
sudo pacman -S cmake extra-cmake-modules qt6-base qt6-declarative qt6-5compat kirigami kcoreaddons ki18n kiconthemes kcolorscheme kconfig zlib git
```

#### Fedora / RHEL
```bash
sudo dnf install cmake extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kirigami-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kiconthemes-devel kf6-kcolorscheme-devel kf6-kconfig-devel zlib-devel git
```

#### Ubuntu 24.04+ / Debian Unstable (Trixie/Sid)
```bash
sudo apt install cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev libkf6kirigami-dev libkf6coreaddons-dev libkf6i18n-dev libkf6iconthemes-dev libkf6colorscheme-dev libkf6config-dev zlib1g-dev git
```

#### openSUSE Tumbleweed
```bash
sudo zypper install cmake extra-cmake-modules qt6-base-devel qt6-declarative-devel kf6-kirigami-devel kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kiconthemes-devel kf6-kcolorscheme-devel kf6-kconfig-devel zlib-devel git
```

---

## 🚀 Building & Running

### 1. Configure the Build
```bash
cmake -B build
```

### 2. Compile
```bash
cmake --build build -j$(nproc)
```

### 3. Launch cherrygi
```bash
./build/cherrygi
```

### 4. Install (Optional)
```bash
sudo cmake --install build
```
This installs the binary, the desktop launcher (`org.kde.cherrygi.desktop`), and scalable icons to your system.
