# Hardware test checklist

Things that need the real headset and a human. Record results in `docs/ANALYSIS.md` (DOĞRULANDI /
çalışmıyor + date).

Run: `./build/src/app/v9pro-control` (debug logs: `QT_LOGGING_RULES="v9.*.debug=true"`).

## 1. Sound mode set (Home)
- [x] Click Game 1 / Game 2 / Music; the tile stays selected (read-back) and the sound changes. (2026-10-30)
- [ ] Press the mode button on the headset; the Home page follows within a second (push report).

## 2. Mic button traffic (Lab → HID monitor)
- [x] Start capture, press the mic button 4–5 times, stop. (2026-10-30: no reports at all)
- [ ] Any highlighted line? `TELEPHONY` (report 0x07) or an unknown 0x55 command means the button is
      visible to the host → we can sync it with the PipeWire mic mute. Nothing → confirms no USB traffic.
- [ ] Save the capture next to the analysis if anything shows up.

## 3. Experimental settings (Lab)
- [x] (2026-10-30: no reply, unsupported) Voice prompt volume → Read. Reply? Note the value. If yes, write 1, toggle the mic, listen; write back.
- [x] (2026-10-30: no reply, unsupported) Auto power-off → Read. Reply? Note the raw value.
- [ ] Do NOT use factory reset unless we decide to.

## 4. Mic monitor (Home)
- [x] Monitor on, level 50 %: you hear yourself with small delay. (2026-10-30, also via RNNoise)
- [x] Hardware mode (ALSA 'PCM',1): broken, cuts headset output → removed. (2026-10-30)

## 5. Noise suppression (Home)
- [x] (2026-10-30, RNNoise) Enable; default input becomes "V9 PRO Microphone (noise suppressed)". Record a test in any app
      with a fan/keyboard noise, compare on/off.
- [ ] Optional: install `noise-suppression-for-voice`, restart, try RNNoise.

## 6. Equalizer
- [ ] Enable; default output becomes "V9 PRO Equalizer"; audio keeps playing.
- [ ] Drag points / sliders while music plays: change is immediate, no dropouts.
- [ ] Bass / brightening / vocal clarity audible; auto preamp prevents clipping at +10 dB boosts.
- [ ] Import a Hub-exported profile JSON; export and re-import.
- [ ] Disable; default output returns to the headset.
