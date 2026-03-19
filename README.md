# clip-stacker

clip-stacker is a Linux desktop clipboard manager built with C++ and Qt 6. It runs in the background, stores clipboard history in SQLite, exposes a tray icon, and opens a searchable popup near the cursor for quick paste workflows.

It is designed primarily for Ubuntu-class desktop systems and packaged as a `.deb` artifact.

## Who This Is For

### Non-technical users

clip-stacker helps you:

- keep a history of copied text, images, HTML, RTF, and file references
- open clipboard history instantly with `Super+V`
- reopen clipboard history from the tray menu
- pin important clipboard items so they are not removed
- search old clipboard items quickly
- keep the app running in the background with auto-start support

### Technical users

clip-stacker provides:

- a modular Qt 6 codebase using CMake
- a widget-based popup UI with native tray workflow and cursor-aware placement
- SQLite-backed persistence with deduplication and metadata
- X11 global `Super+V` popup activation and X11 XTest-based paste simulation
- CLI fallback through a command entry point when compositor restrictions block global hotkeys
- shared-folder sync support for non-sensitive clipboard data
- Debian packaging through CPack and CMake install rules

## Main Features

- Clipboard history for plain text, HTML, images, RTF payloads, and file URI lists
- SQLite-backed storage with timestamps, source application tracking where available, and deduplication
- Pin and favorite support for important entries
- Searchable popup UI with keyboard navigation and focus-loss dismissal
- Global `Super+V` shortcut on X11 to open the popup at the current cursor position
- Tray icon with quick actions, notifications, auto-start, and settings access
- `.deb` packaging pipeline for Ubuntu-style installation

## Current Platform Support

### Linux desktop sessions

The supported path currently includes:

- clipboard monitoring through Qt
- tray icon workflow for opening clipboard history
- global `Super+V` popup toggle on X11 sessions
- active window class lookup for source application detection on X11
- simulated `Ctrl+V` paste after restoring a clipboard item on X11
- popup toggle through CLI fallback when you want to bind your own desktop shortcut

Optional desktop shortcut command:

```bash
clip-stacker --toggle-popup
```

Bind that command in your desktop environment's keyboard shortcut settings if you want a custom shortcut.

## Quick Start For Non-technical Users

### 1. Install the package

If you already have a generated package file, install it with:

```bash
sudo apt install ./clip-stacker_0.1.0_amd64.deb
```

### 2. Start the app

Launch `clip-stacker` from the application menu.

### 3. Use it daily

- copy text, images, or files normally
- press `Super+V` anywhere to open clipboard history near the cursor on X11
- open the tray icon menu to show clipboard history
- open history from the tray icon menu if you prefer mouse access
- use `clip-stacker --toggle-popup` as a manual shortcut fallback on desktops where global grabs are restricted
- use search to find older clipboard items
- pin items you want to keep

### 4. Configure it

Open the tray icon menu and choose `Settings`.

You can change:

- history limit

## Technical Overview

### Architecture

The application is split into four main layers:

1. `Core services`
  Clipboard capture, database storage, history coordination, sync, paste simulation, logging, and settings.
2. `UI bridge`
  C++ controllers connect services to the popup, global hotkey backend, and tray integration.
3. `Popup UI`
  Searchable, keyboard-driven clipboard history interface shown near the cursor.
4. `Packaging and desktop integration`
  Desktop entry, icon installation, autostart helper, and `.deb` generation.

### Source Layout

- `src/app/` application bootstrap and runtime wiring
- `src/core/` clipboard, storage, sync, hotkey, settings, and logging services
- `src/ui/` tray controller, popup controller, and settings dialog
- `resources/qml/` popup UI and delegates
- `assets/icons/` application icon assets
- `packaging/linux/` packaging and autostart helpers

## Build Requirements

Ubuntu packages typically required:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-declarative-dev \
  qt6-tools-dev qt6-svg-dev qt6-wayland libx11-dev libxtst-dev libxfixes-dev
```

## Build From Source

### Standard build

```bash
cmake -S . -B build
cmake --build build -j2
```

### Release build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

### Run locally

```bash
./build/clip-stacker
```

## Package As `.deb`

### Using CPack directly

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
cpack --config build/CPackConfig.cmake -G DEB
```

### Using the helper script

```bash
./packaging/linux/build-deb.sh
```

Expected output:

- `clip-stacker_0.1.0_amd64.deb`

## Git Commit Guidance

This repository should keep source files, build configuration, resources, and packaging scripts in Git.

### Commit these files

- source code under `src/`
- QML under `resources/qml/`
- icons under `assets/`
- packaging scripts under `packaging/`
- `CMakeLists.txt`
- `README.md`
- `.gitignore`
- `LICENSE`

### Do not commit these files

- `build/`
- `cmake-build-*/`
- `_CPack_Packages/`
- generated `.deb` files
- local `.sqlite` databases
- local `.log` files
- editor swap or user files such as `*.swp`, `*.user`, and `*~`

### Why these files are ignored

Ignored files are machine-local or generated artifacts. They make Git history noisy, increase repository size, and can create misleading diffs because they are rebuilt on demand.

In your current repo state, these local generated artifacts should stay out of Git:

- `_CPack_Packages/`
- `clip-stacker_0.1.0_amd64.deb`
- anything inside `build/`

## Privacy Notes

- Sensitive-entry handling still uses internal defaults in the application logic.
- Shared-folder sync exports only non-sensitive entries.
- Logs and databases are local runtime artifacts and should not be committed.

## Limitations

- Wayland does not provide a universal regular-app API for global hotkeys or synthetic paste input.
- Shared-folder sync is not end-to-end encrypted cloud sync.
- Source application detection is strongest on X11 and may be unavailable elsewhere.

## Troubleshooting

### The app starts but does not paste automatically

- Automatic paste currently depends on X11 XTest support.
- On Wayland, the app restores the clipboard item, but you may need to paste manually.

### `Super+V` does not open the popup

- Global `Super+V` currently depends on an X11 session.
- On Wayland, most compositors do not allow regular applications to register a universal global shortcut.
- In that case, use the tray menu or bind `clip-stacker --toggle-popup` in your desktop keyboard shortcut settings.

### I see package output in Git status

Those files are generated artifacts and should not be committed. This repository now ignores the main generated package paths through `.gitignore`.

## Development Notes

- The project targets Qt 6.4 or newer.
- The default local build directory is `build/`.
- The generated Debian package was validated through CPack.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).
