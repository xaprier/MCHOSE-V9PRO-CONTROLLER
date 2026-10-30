#pragma once

#include <QList>
#include <QPointF>
#include <QString>

#include <array>

// Equalizer model shared by the PipeWire filter-chain generator and the UI curve. The filters are
// the RBJ cookbook biquads, which is what PipeWire's builtin bq_* plugins implement, so the drawn
// response matches what is actually heard.
namespace v9::eq {

inline constexpr int BandCount = 10;
inline constexpr double MinGainDb = -20.0;
inline constexpr double MaxGainDb = 20.0;
inline constexpr double SampleRate = 48000.0;

enum class FilterType {
    LowShelf,
    Peaking,
    HighShelf,
};

struct Biquad {
    FilterType type = FilterType::Peaking;
    double frequency = 1000.0;
    double q = 1.0;
    double gainDb = 0.0;
};

// Band centres as MCHOSE HUB labels them. The outermost two are shelves: a 20 Hz peak or a
// 20 kHz peak would be inaudible, while a shelf keeps the label's intent (sub-bass / air).
struct BandSpec {
    const char *label;
    FilterType type;
    double frequency;
    double q;
};
const std::array<BandSpec, BandCount> &bands();

// Hub-style "enhancement" effects approximated with one biquad each. `level` is 0..10 as in Hub.
struct Enhancements {
    bool bassEnabled = false;
    int bassLevel = 5;
    int bassCutoffHz = 120; // 50..400
    bool brillianceEnabled = false;
    int brillianceLevel = 5;
    bool clarityEnabled = false;
    int clarityLevel = 5;

    bool operator==(const Enhancements &) const = default;
};

struct Settings {
    std::array<double, BandCount> gains{};
    double preampDb = 0.0;
    Enhancements enhancements;

    bool operator==(const Settings &) const = default;
};

// Every filter in signal order: 10 bands, then bass, brilliance, clarity (gain 0 when disabled,
// so toggling an effect is a live parameter change rather than a graph rebuild).
QList<Biquad> filters(const Settings &settings);

double bassGainDb(const Enhancements &e);
double brillianceGainDb(const Enhancements &e);
double clarityGainDb(const Enhancements &e);

// Magnitude response in dB of the whole chain (preamp included) at `frequency`.
double responseDb(const Settings &settings, double frequency);

// Log-spaced response from 20 Hz to 20 kHz: x = frequency in Hz, y = gain in dB.
QList<QPointF> responseCurve(const Settings &settings, int points = 160);

// Suggested preamp that keeps the loudest point of the curve at or below 0 dB.
double headroomPreampDb(const Settings &settings);

} // namespace v9::eq
