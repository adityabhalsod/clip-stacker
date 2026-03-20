cmake_minimum_required(VERSION 3.24)

# Define the project metadata used by CMake, packaging, and desktop integration.
project(clip-stacker VERSION 0.1.0 LANGUAGES CXX)

# Require a modern C++ toolchain for Qt 6 and the native Linux integrations.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt's automatic code generation for QObject-based classes and resources.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Prefer release-grade defaults for packaging unless the user overrides them.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Locate the Qt modules needed by the background service, popup UI, storage, and DBus integration.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Sql DBus Svg Network)

# Locate X11 libraries used for native hotkeys, active-window inspection, and simulated paste on X11.
find_package(X11 REQUIRED)

# Locate the XTest extension used to synthesize Ctrl+V under X11.
find_library(X11_XTEST_LIBRARY Xtst REQUIRED)

# Configure the installed desktop entry with the final application name and executable path.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/clip-stacker.desktop.in
    ${CMAKE_CURRENT_BINARY_DIR}/clip-stacker.desktop
    @ONLY
)

# Build the desktop application from the core service, UI bridge, and platform integration sources.
qt_add_executable(clip-stacker
    src/main.cpp
    src/app/ApplicationController.cpp
    src/core/ClipboardDatabase.cpp
    src/core/ClipboardListener.cpp
    src/core/HistoryListModel.cpp
    src/core/HistoryManager.cpp
    src/core/HotkeyManager.cpp
    src/core/InputSimulator.cpp
    src/core/Logger.cpp
    src/core/SettingsManager.cpp
    src/core/SyncEngine.cpp
    src/ui/PopupController.cpp
    src/ui/SettingsDialog.cpp
    src/ui/TrayController.cpp
    resources/resources.qrc
)

# Publish the public include paths used by the modular source tree.
target_include_directories(clip-stacker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link the Qt runtime and the native Linux libraries required by the integrations.
target_link_libraries(clip-stacker PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::DBus
    Qt6::Svg
    Qt6::Network
    ${X11_LIBRARIES}
    ${X11_XTEST_LIBRARY}
)

# Add the X11 include directories so the native integration headers resolve cleanly.
target_include_directories(clip-stacker PRIVATE
    ${X11_INCLUDE_DIR}
)

# Install the executable into the standard binary location for Debian-style packages.
install(TARGETS clip-stacker RUNTIME DESTINATION bin)

# Install the desktop entry for launchers, autostart integration, and desktop menus.
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/clip-stacker.desktop DESTINATION share/applications)

# Install the scalable application icon into the hicolor icon theme hierarchy.
install(FILES assets/icons/clip-stacker.svg DESTINATION share/icons/hicolor/scalable/apps)

# Install pre-generated PNG icons at standard sizes so desktop menus display the icon reliably.
# The PNGs are committed to assets/icons/ and installed directly — no build-time rasterization needed.
foreach(_SIZE 48 128 256)
    install(FILES "assets/icons/clip-stacker-${_SIZE}.png"
            DESTINATION "share/icons/hicolor/${_SIZE}x${_SIZE}/apps"
            RENAME clip-stacker.png)
endforeach()

# Install the helper script used to enable autostart for the current user session.
install(PROGRAMS packaging/linux/install-autostart.sh DESTINATION lib/clip-stacker)

# ── CPack shared metadata (used by all generators) ──────────────────────────
# Default generators: calling `cpack` produces all three formats.
# Each CI job overrides this with `cpack -G DEB`, `cpack -G RPM`, or `cpack -G TGZ`.
set(CPACK_GENERATOR "DEB;RPM;TGZ")
set(CPACK_PACKAGE_NAME clip-stacker)
set(CPACK_PACKAGE_VENDOR "Aditya")
set(CPACK_PACKAGE_CONTACT "adityabhalsod99@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Clipboard history manager for Linux desktop environments")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)

# Ensure all package formats install under /usr so XDG paths resolve correctly.
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# ── Debian / Ubuntu (.deb) ───────────────────────────────────────────────────
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Aditya")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
# Explicit runtime dependencies covering Ubuntu 22.04 (libqt6coreN) and Ubuntu 24.04 /
# arm64 (libqt6coreNt64 — the 64-bit time_t ABI transition rename).  Using package
# alternatives ("A | B") lets apt pick whichever variant is present on the target system.
# CPACK_DEBIAN_PACKAGE_SHLIBDEPS is intentionally NOT used: dpkg-shlibdeps fails on
# aarch64 because it cannot resolve Qt6 library paths under the multiarch triplet
# (/usr/lib/aarch64-linux-gnu), producing an empty data.tar and a ~1 KB package.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6core6t64 | libqt6core6 (>= 6.4), \
libqt6gui6t64 | libqt6gui6 (>= 6.4), \
libqt6widgets6t64 | libqt6widgets6 (>= 6.4), \
libqt6qml6 (>= 6.4), \
libqt6quick6 (>= 6.4), \
libqt6sql6 (>= 6.4), \
libqt6dbus6 (>= 6.4), \
libqt6svg6 (>= 6.4), \
libqt6network6 (>= 6.4), \
libx11-6, \
libxtst6, \
libxfixes3")
# Post-install / post-remove scripts refresh the desktop database and icon cache.
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postrm")

# ── Fedora / RHEL / openSUSE (.rpm) ─────────────────────────────────────────
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Utilities")
set(CPACK_RPM_PACKAGE_URL "https://github.com/adityabhalsod/clip-stacker")
set(CPACK_RPM_PACKAGE_DESCRIPTION
    "clip-stacker is a fast, lightweight clipboard manager for Linux desktop environments.\n"
    "It captures everything you copy and lets you paste any previous item instantly\n"
    "via a global hotkey popup, just like the Windows 11 clipboard manager.")
# Auto-detect shared library dependencies so the RPM resolves correctly on the target distro.
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
# Run the same post-install / post-uninstall cache refresh used by the .deb.
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postinst")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postrm")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

# ── Generic tarball (.tar.gz) — for Arch Linux and other distros ─────────────
# Produces a portable archive with the same install-prefix layout (/usr/…).
# Users can extract to /usr/local or run: sudo tar -xzf clip-stacker*.tar.gz -C /
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

include(CPack)
