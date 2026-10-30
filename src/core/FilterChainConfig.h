#pragma once

#include "EqualizerMath.h"

#include <QString>

// Generates PipeWire configuration (SPA-JSON) for the virtual nodes this app runs through a child
// `pipewire -c <file>` process, and the Props payloads used to change them live via `pw-cli`.
namespace v9::pw {

inline const QString EqualizerSinkName = QStringLiteral("v9pro_equalizer");
inline const QString NoiseSourceName = QStringLiteral("v9pro_mic_denoised");
inline const QString MonitorNodeName = QStringLiteral("v9pro_mic_monitor");
inline const QString SoundboardSinkName = QStringLiteral("v9pro_soundboard");

enum class NoiseEngine {
    WebRtc,  // libpipewire-module-echo-cancel with the webrtc backend; ships with PipeWire
    RNNoise, // librnnoise_ladspa (noise-suppression-for-voice); optional, usually better on voice
};

// Virtual sink "V9 PRO Equalizer" whose output feeds `targetSink`.
QString equalizerConfig(const eq::Settings &settings, const QString &targetSink);
// `{ params = [ "eq_1:Gain" 3.0 ... ] }` for every control of the equalizer graph.
QString equalizerParams(const eq::Settings &settings);

// Virtual source "V9 PRO Microphone (noise suppressed)" fed by `targetSource`.
QString noiseSuppressionConfig(NoiseEngine engine, const QString &targetSource, int vadThreshold);
QString noiseSuppressionParams(int vadThreshold); // RNNoise only; WebRTC has no live controls

// Low-latency loopback from the headset microphone into the headset sink (software sidetone).
QString micMonitorConfig(const QString &source, const QString &sink, double gain);
QString micMonitorParams(double gain);

inline const QString SoundboardListenNodeName = QStringLiteral("v9pro_soundboard_listen");

// Virtual mono sink the soundboard plays into (mono so a mono recorder does not get L+R summed), built as a loopback: a client process cannot export
// a plain null sink, and the loopback's playback side into `listenSink` (the headset; the default
// device when empty) is "hear it myself", muted with soundboardListenParams(false).
QString soundboardConfig(const QString &listenSink);
QString soundboardListenParams(bool listen);

// Quotes and escapes a string for SPA-JSON.
QString quoted(const QString &value);

} // namespace v9::pw
