# V9 Pro Control

Unofficial Linux control application for the **MCHOSE V9 PRO** wireless headset
(USB `291d:385d`). MCHOSE HUB has no Linux version and does not work under Wine; this app talks to
the headset directly over hidraw and to PipeWire. No network access.

## Features

- Battery level and charge state (live, including pushes from the headset)
- Dongle / headset connection state
- On-device sound mode: Game 1, Game 2, Music
- Firmware versions of dongle and headset
- Headphone and microphone volume / mute (PipeWire)
- Sidetone level (ALSA, experimental)

Planned: 10-band equalizer and effects via PipeWire filter-chain, Hub-compatible EQ profiles,
microphone noise suppression, tray icon. See `docs/ANALYSIS.md` for the full protocol and feature map.

## Build

Requires Qt 6.8+ (Quick, QuickControls2, Widgets), CMake 3.21+, a C++20 compiler, and at runtime
`pactl` (pipewire-pulse) and `amixer` (alsa-utils).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
./build/src/app/v9pro-control
```

## Device access without root

```bash
sudo cp packaging/70-mchose-v9pro.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
```

Replug the dongle afterwards.
