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
- 10-band equalizer with draggable response curve, bass / brightening / vocal clarity, auto preamp,
  and profiles in MCHOSE HUB's JSON format (PipeWire filter-chain)
- Microphone noise suppression (WebRTC built in, RNNoise if installed)
- Microphone monitor (PipeWire loopback; follows noise suppression)
- Lab: HID monitor, experimental headset settings, and a voice prompt silencer that rebuilds the stock
  firmware with the mic on/off announcements silenced and flashes it over the dongle

See `docs/ANALYSIS.md` for the protocol and feature map.

## Build

Requires Qt 6.8+ (Quick, QuickControls2, Widgets), CMake 3.21+, a C++20 compiler, and at runtime
`pactl` (pipewire-pulse), `pipewire`, `pw-cli`, `pw-dump` and `amixer` (alsa-utils). Optional:
`noise-suppression-for-voice` for RNNoise.

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
