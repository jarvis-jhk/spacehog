# Changelog

## 0.1.1

- Linux build made on an older base: runs on distributions with glibc 2.35 or newer
  (Ubuntu 22.04, Debian 12, current SteamOS); 0.1.0 needed glibc 2.38.
- Linux build supports Wayland natively (besides X11).
- Releases are built by GitHub Actions.

## 0.1.0 — first public release

- Native port of Operation Spacehog 1.01 for Linux (also the Steam Deck) and Windows,
  verified tick by tick against the original DOS code.
- Resizable window, fullscreen (Alt+Enter), 4:3 correction, integer scaling.
- Gamepads for both players, hot-plug, everything playable without a keyboard.
- Original FM music on an emulated OPL3; optional General MIDI through a SoundFont.
- Works with the Warpzone launcher: it installs and updates the game and hands over
  the original data it found (`WARPZONE_DATA_GAME`).
