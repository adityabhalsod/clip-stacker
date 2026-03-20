<p align="center">
  <img src="assets/icons/clip-stacker.svg" alt="clip-stacker logo" width="120" />
</p>

<h1 align="center">clip-stacker</h1>

<p align="center">
  <strong>A fast, lightweight clipboard manager for Linux desktops</strong>
</p>

<p align="center">
  <a href="#-quick-install"><img alt="Quick Install" src="https://img.shields.io/badge/-Quick%20Install-blue?style=flat-square" /></a>
  <a href="#-build-from-source"><img alt="Build" src="https://img.shields.io/badge/-Build%20From%20Source-green?style=flat-square" /></a>
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" /></a>
</p>

---

**clip-stacker** runs quietly in the background, captures everything you copy, and lets you pull up your full clipboard history with a single keystroke. Select any past item and it gets pasted instantly — just like the Windows 11 clipboard manager, but on Linux.

Built with **C++20** and **Qt 6** for speed and low resource usage. History is stored locally in **SQLite**. Packaged as a `.deb` for easy installation on Ubuntu, Debian, Linux Mint, Pop!\_OS, and similar distributions.

---

## Table of Contents

- [Features](#-features)
- [How It Works (60-Second Overview)](#-how-it-works-60-second-overview)
- [Quick Install](#-quick-install)
- [Getting Started](#-getting-started)
- [Keyboard Shortcuts](#-keyboard-shortcuts)
- [Configuration](#%EF%B8%8F-configuration)
- [Build From Source](#-build-from-source)
- [Package as .deb](#-package-as-deb)
- [Project Architecture](#-project-architecture)
- [GitHub Actions CI/CD](#-github-actions-cicd)
- [Contributing](#-contributing)
- [Troubleshooting](#-troubleshooting)
- [Privacy & Security](#-privacy--security)
- [License](#-license)

---

## ✨ Features

| Feature | Description |
|---|---|
| **Clipboard History** | Saves plain text, rich text (HTML/RTF), images, and file URI lists |
| **Instant Popup** | Press `Ctrl+Super+V` to open a searchable history panel near your cursor |
| **Single-Click Paste** | Click any item → it copies to clipboard **and** pastes into the active app |
| **Pin & Favorite** | Pin important clips so they're never pruned; star favorites for quick access |
| **Live Search** | Type to filter history entries in real time |
| **System Tray** | Runs in the background; access history, settings, and quit from the tray icon |
| **Auto-Start** | Optional autostart integration for GNOME, KDE, XFCE, and other freedesktop sessions |
| **Deduplication** | Identical clips are merged instead of creating duplicates |
| **Source Tracking** | Remembers which application produced each clip (X11) |
| **Multi-Device Sync** | Share non-sensitive clips across machines via a shared folder |
| **CLI Fallback** | `clip-stacker --toggle-popup` for desktops that block global hotkeys |
| **Lightweight** | C++20, ~5 MB binary, minimal runtime memory |

---

## 🎬 How It Works (60-Second Overview)

```
┌─────────────────────────────────────────────────────────┐
│  You copy something (Ctrl+C, right-click → Copy, etc.)  │
└───────────────────────────┬─────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────┐
│  clip-stacker captures the clipboard content silently    │
│  and stores it in a local SQLite database                │
└───────────────────────────┬─────────────────────────────┘
                            ▼
┌─────────────────────────────────────────────────────────┐
│  Press Ctrl+Super+V → popup appears near your cursor     │
│  Click any item → it pastes into the previously active   │
│  application (just like Windows 11's Win+V)              │
└─────────────────────────────────────────────────────────┘
```

---

## 📦 Quick Install

### Option A — Download a package

Go to the [**Releases**](../../releases) page and download the package for your distro and architecture:

| Package | Distros | Architectures |
|---------|---------|---------------|
| `.deb` | Ubuntu 22.04+, Debian, Linux Mint, Pop!\_OS | amd64 |
| `.rpm` | Fedora 37+, openSUSE, RHEL, AlmaLinux, Rocky | x86\_64, aarch64 |
| `.tar.gz` | Arch Linux, and any other distro | x86\_64, aarch64 |

**Ubuntu / Debian:**
```bash
sudo apt install ./clip-stacker_*_ubuntu_amd64.deb
```

**Fedora / RHEL:**
```bash
sudo dnf install ./clip-stacker_*_fedora_x86_64.rpm
```

**Arch Linux (and other distros — tarball):**
```bash
sudo tar -xzf clip-stacker_*_arch_x86_64.tar.gz -C /
sudo update-desktop-database /usr/share/applications
sudo gtk-update-icon-cache -f /usr/share/icons/hicolor
```

### Option B — Build from source

See [Build From Source](#-build-from-source) below.

---

## 🚀 Getting Started

### Step 1 — Launch the app

After installation, start clip-stacker from your application menu or terminal:

```bash
clip-stacker
```

A small icon appears in your **system tray** (notification area).

### Step 2 — Copy things as usual

Copy text, images, files, or rich content normally (`Ctrl+C`, right-click → Copy, etc.). clip-stacker captures everything automatically in the background.

### Step 3 — Open clipboard history

Press **`Ctrl+Super+V`** (Ctrl + Windows key + V). A popup appears near your cursor showing all your recent clips.

### Step 4 — Paste a previous item

**Click any item** in the popup. clip-stacker will:
1. Copy that item to your clipboard
2. Close the popup
3. Restore focus to the app you were using
4. Automatically paste it (`Ctrl+V`) into that app

**That's it!** You now have unlimited clipboard history.

### Step 5 — Explore more features

- **Search**: Type in the search box at the top of the popup to filter history
- **Pin items**: Right-click → Pin to prevent an item from being pruned
- **Tray menu**: Right-click the tray icon for quick access to History, Settings, and Quit
- **Auto-start**: Enable auto-start so clip-stacker launches when you log in

---

## ⌨ Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Super+V` | Open/close clipboard history popup |
| `Enter` | Paste the highlighted item |
| `↑` / `↓` | Navigate through history items |
| `Escape` | Close the popup |
| Type in search | Filter history entries in real time |

---

## ⚙️ Configuration

Right-click the **tray icon** → **Settings** to configure:

| Setting | Description | Default |
|---|---|---|
| History limit | Maximum number of entries to keep | 500 |

Settings are stored via Qt's `QSettings` mechanism (`~/.config/clip-stacker/`).

### Enable auto-start

Run the included helper script to add clip-stacker to your desktop session's autostart:

```bash
/usr/lib/clip-stacker/install-autostart.sh
```

Or manually copy the desktop entry:

```bash
cp /usr/share/applications/clip-stacker.desktop ~/.config/autostart/
```

---

## 🔨 Build From Source

### Prerequisites

**Ubuntu / Debian** (22.04+):

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  qt6-base-dev \
  qt6-declarative-dev \
  qt6-tools-dev \
  qt6-svg-dev \
  qt6-wayland \
  libx11-dev \
  libxtst-dev \
  libxfixes-dev
```

**Fedora** (37+):

```bash
sudo dnf install -y \
  gcc-c++ cmake ninja-build \
  qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel \
  qt6-qtsvg-devel qt6-qtwayland-devel \
  libX11-devel libXtst-devel libXfixes-devel
```

**Arch Linux**:

```bash
sudo pacman -S --needed \
  base-devel cmake ninja \
  qt6-base qt6-declarative qt6-tools qt6-svg qt6-wayland \
  libx11 libxtst libxfixes
```

### Compile

```bash
# Clone the repository
git clone https://github.com/adityabhalsod/clip-stacker.git
cd clip-stacker

# Configure (Debug build)
cmake -S . -B build

# Configure (Release build — recommended)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build using all available CPU cores
cmake --build build -j$(nproc)
```

### Run

```bash
./build/clip-stacker
```

---

## 📦 Package as `.deb`

### Using the helper script

```bash
./packaging/linux/build-deb.sh
```

### Using CMake/CPack directly

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cpack --config build/CPackConfig.cmake -G DEB
```

Output: `clip-stacker_0.1.0_amd64.deb`

Install the generated package:

```bash
sudo apt install ./clip-stacker_0.1.0_amd64.deb
```

---

## 🏗 Project Architecture

### High-Level Design

```
┌────────────────────────────────────────────────────────────────────┐
│                        Application Layer                           │
│   ApplicationController  — bootstraps services and wiring          │
├───────────────────┬──────────────────┬─────────────────────────────┤
│    UI Layer       │   Core Layer     │   Platform Layer             │
│                   │                  │                              │
│  PopupController  │  HistoryManager  │  HotkeyManager (X11 grab)   │
│  TrayController   │  ClipboardDB     │  InputSimulator (XTest)      │
│  SettingsDialog   │  ClipboardListen │  SyncEngine (shared folder)  │
│                   │  HistoryListModel│                              │
│                   │  SettingsManager │                              │
│                   │  Logger          │                              │
└───────────────────┴──────────────────┴─────────────────────────────┘
```

### Source Tree

```
clip-stacker/
├── .github/
│   └── workflows/
│       └── release.yml          # CI/CD pipeline
├── assets/
│   └── icons/
│       └── clip-stacker.svg     # App icon
├── packaging/
│   └── linux/
│       ├── build-deb.sh         # One-command .deb builder
│       ├── clip-stacker.desktop.in  # Desktop entry template
│       └── install-autostart.sh # Autostart helper
├── resources/
│   ├── qml/
│   │   ├── Main.qml            # QML popup definition
│   │   └── components/
│   │       └── HistoryDelegate.qml
│   └── resources.qrc           # Qt resource bundle
├── src/
│   ├── main.cpp                # Entry point
│   ├── app/
│   │   └── ApplicationController.cpp/.h
│   ├── core/
│   │   ├── ClipboardDatabase.cpp/.h   # SQLite persistence
│   │   ├── ClipboardEntry.h           # Data model
│   │   ├── ClipboardListener.cpp/.h   # Live clipboard monitor
│   │   ├── HistoryListModel.cpp/.h    # Qt model for the popup
│   │   ├── HistoryManager.cpp/.h      # History coordination
│   │   ├── HotkeyManager.cpp/.h      # X11 global hotkey
│   │   ├── InputSimulator.cpp/.h      # Synthetic paste (XTest)
│   │   ├── Logger.cpp/.h             # Logging
│   │   ├── SettingsManager.cpp/.h    # App settings
│   │   └── SyncEngine.cpp/.h         # Multi-device sync
│   └── ui/
│       ├── PopupController.cpp/.h     # Popup window logic
│       ├── SettingsDialog.cpp/.h      # Settings UI
│       └── TrayController.cpp/.h      # System tray
├── CMakeLists.txt               # Build system
├── LICENSE                      # MIT License
└── README.md                    # This file
```

### Technology Stack

| Component | Technology |
|---|---|
| Language | C++20 |
| GUI Framework | Qt 6.4+ (Widgets + QML) |
| Build System | CMake 3.24+ |
| Database | SQLite (via Qt6::Sql) |
| Global Hotkey | X11 `XGrabKey` passive grab |
| Paste Simulation | X11 XTest `XTestFakeKeyEvent` |
| Focus Tracking | EWMH `_NET_ACTIVE_WINDOW` |
| Packaging | CPack DEB generator |
| CI/CD | GitHub Actions |

---

## 🚢 GitHub Actions CI/CD

Every push to `main`, `beta`, or `alpha` triggers a 3-job automated pipeline:

1. **Prepare** — determines version, generates changelog, pushes git tag
2. **Build matrix** — 5 parallel builds across distros and architectures
3. **Release** — collects all packages and publishes the GitHub Release

**Package matrix (5 artefacts per release):**

| Format | Target Distros | amd64 / x86\_64 | arm64 / aarch64 |
|--------|---------------|:--------------:|:---------------:|
| `.deb` | Ubuntu 22.04+, Debian, Mint, Pop!\_OS | ✓ | — |
| `.rpm` | Fedora 37+, openSUSE, RHEL, AlmaLinux | ✓ | ✓ |
| `.tar.gz` | Arch Linux, generic / other distros | ✓ | ✓ |

**Branch → Release channel mapping:**

| Branch | Channel | Version Example | Pre-release? |
|---|---|---|---|
| `main` | Stable | `v0.1.0` | No |
| `beta` | Beta | `v0.1.0-beta.3` | Yes |
| `alpha` | Alpha | `v0.1.0-alpha.7` | Yes |

See [.github/workflows/release.yml](.github/workflows/release.yml) for the full workflow.

---

## 🤝 Contributing

Contributions are welcome! Here's how to get started:

1. **Fork** the repository
2. **Create a branch** for your feature or fix:
   ```bash
   git checkout -b feat/my-feature
   ```
3. **Make your changes** and test locally
4. **Commit** using [conventional commits](https://www.conventionalcommits.org/):
   ```bash
   git commit -m "feat: add dark theme support"
   git commit -m "fix: resolve paste delay on KDE"
   ```
5. **Push** and open a **Pull Request**

### What to commit

| Commit | Don't commit |
|---|---|
| Source code (`src/`) | `build/` directory |
| QML files (`resources/qml/`) | `cmake-build-*/` |
| Icons (`assets/`) | `_CPack_Packages/` |
| Packaging scripts (`packaging/`) | `*.deb` files |
| `CMakeLists.txt`, `README.md`, `.gitignore`, `LICENSE` | `*.sqlite`, `*.log` |
| GitHub workflows (`.github/`) | Editor temp files (`*.swp`, `*~`) |

---

## 🔧 Troubleshooting

<details>
<summary><strong>Ctrl+Super+V does not open the popup</strong></summary>

- The global hotkey requires an **X11 session**. It will not work on pure Wayland.
- Another application may have already grabbed this shortcut. Check your desktop environment's keyboard settings.
- **Workaround:** Use the tray icon menu, or bind a custom shortcut to:
  ```bash
  clip-stacker --toggle-popup
  ```
</details>

<details>
<summary><strong>The app does not paste automatically after selecting an item</strong></summary>

- Automatic paste requires **X11 XTest** support.
- On Wayland, clip-stacker copies the item to your clipboard, but you need to press `Ctrl+V` manually.
- Some sandboxed applications (Flatpak/Snap) may block synthetic input.
</details>

<details>
<summary><strong>I see generated files in git status</strong></summary>

These are build artifacts. Make sure your `.gitignore` is up to date:

```bash
git checkout -- .gitignore
git clean -fd build/ _CPack_Packages/
```
</details>

<details>
<summary><strong>The tray icon does not appear</strong></summary>

- Make sure your desktop environment supports the system tray (also called the notification area or status notifier).
- On GNOME, install the [AppIndicator extension](https://extensions.gnome.org/extension/615/appindicator-support/).
- On KDE, the tray should work out of the box.
</details>

<details>
<summary><strong>Build fails with missing Qt modules</strong></summary>

Install all required dependencies for your distribution. See [Build From Source](#-build-from-source) for distribution-specific commands.
</details>

---

## 🔒 Privacy & Security

- **All data stays local.** Clipboard history is stored in a SQLite database on your machine.
- **No network calls.** clip-stacker does not phone home, send telemetry, or contact any server.
- **Sensitive content handling.** The app uses heuristics to detect sensitive content (e.g., passwords) and can skip storing them.
- **Sync is opt-in.** Shared-folder sync only exports non-sensitive entries and is entirely local (no cloud).
- **Logs and databases** are runtime artifacts stored locally and are excluded from Git via `.gitignore`.

---

## 📄 License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.

---

<p align="center">
  Made with ❤️ for the Linux desktop
</p>
