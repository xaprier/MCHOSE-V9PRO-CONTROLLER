#include "EqualizerMath.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace v9::eq {

namespace {

constexpr double BrillianceFrequency = 6000.0;
constexpr double ClarityFrequency = 2500.0;
constexpr double ClarityQ = 0.9;
constexpr double ShelfQ = 0.707;

struct Coefficients {
    double b0, b1, b2, a0, a1, a2;
};

Coefficients coefficients(const Biquad &f)
{
    const double a = std::pow(10.0, f.gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f.frequency / SampleRate;
    const double cosw = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * f.q);

    switch (f.type) {
    case FilterType::Peaking:
        return {1 + alpha * a, -2 * cosw, 1 - alpha * a, 1 + alpha / a, -2 * cosw, 1 - alpha / a};
    case FilterType::LowShelf: {
        const double s = 2 * std::sqrt(a) * alpha;
        return {a * ((a + 1) - (a - 1) * cosw + s), 2 * a * ((a - 1) - (a + 1) * cosw), a * ((a + 1) - (a - 1) * cosw - s),
                (a + 1) + (a - 1) * cosw + s, -2 * ((a - 1) + (a + 1) * cosw), (a + 1) + (a - 1) * cosw - s};
    }
    case FilterType::HighShelf: {
        const double s = 2 * std::sqrt(a) * alpha;
        return {a * ((a + 1) + (a - 1) * cosw + s), -2 * a * ((a - 1) + (a + 1) * cosw), a * ((a + 1) + (a - 1) * cosw - s),
                (a + 1) - (a - 1) * cosw + s, 2 * ((a - 1) - (a + 1) * cosw), (a + 1) - (a - 1) * cosw - s};
    }
    }
    return {1, 0, 0, 1, 0, 0};
}

double magnitudeDb(const Biquad &f, double frequency)
{
    if (f.gainDb == 0.0)
        return 0.0;
    const Coefficients c = coefficients(f);
    const double w = 2.0 * std::numbers::pi * frequency / SampleRate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    const std::complex<double> h = (c.b0 + c.b1 * z1 + c.b2 * z2) / (c.a0 + c.a1 * z1 + c.a2 * z2);
    return 20.0 * std::log10(std::abs(h));
}

} // namespace

const std::array<BandSpec, BandCount> &bands()
{
    static const std::array<BandSpec, BandCount> specs{{
        {"20", FilterType::LowShelf, 32.0, ShelfQ},
        {"60", FilterType::Peaking, 60.0, 1.2},
        {"100", FilterType::Peaking, 100.0, 1.2},
        {"250", FilterType::Peaking, 250.0, 1.1},
        {"500", FilterType::Peaking, 500.0, 1.1},
        {"1k", FilterType::Peaking, 1000.0, 1.1},
        {"2k", FilterType::Peaking, 2000.0, 1.1},
        {"4k", FilterType::Peaking, 4000.0, 1.1},
        {"8k", FilterType::Peaking, 8000.0, 1.1},
        {"20k", FilterType::HighShelf, 14000.0, ShelfQ},
    }};
    return specs;
}

double bassGainDb(const Enhancements &e)
{
    return e.bassEnabled ? 1.2 * std::clamp(e.bassLevel, 0, 10) : 0.0;
}

double brillianceGainDb(const Enhancements &e)
{
    return e.brillianceEnabled ? 0.8 * std::clamp(e.brillianceLevel, 0, 10) : 0.0;
}

double clarityGainDb(const Enhancements &e)
{
    return e.clarityEnabled ? 0.6 * std::clamp(e.clarityLevel, 0, 10) : 0.0;
}

QList<Biquad> filters(const Settings &settings)
{
    QList<Biquad> list;
    list.reserve(BandCount + 3);
    for (int i = 0; i < BandCount; ++i) {
        const BandSpec &spec = bands()[i];
        list.append({spec.type, spec.frequency, spec.q, std::clamp(settings.gains[i], MinGainDb, MaxGainDb)});
    }
    const Enhancements &e = settings.enhancements;
    list.append({FilterType::LowShelf, double(std::clamp(e.bassCutoffHz, 50, 400)), ShelfQ, bassGainDb(e)});
    list.append({FilterType::HighShelf, BrillianceFrequency, ShelfQ, brillianceGainDb(e)});
    list.append({FilterType::Peaking, ClarityFrequency, ClarityQ, clarityGainDb(e)});
    return list;
}

double responseDb(const Settings &settings, double frequency)
{
    double total = settings.preampDb;
    for (const Biquad &f : filters(settings))
        total += magnitudeDb(f, frequency);
    return total;
}

QList<QPointF> responseCurve(const Settings &settings, int points)
{
    QList<QPointF> curve;
    curve.reserve(points);
    const double lo = std::log10(20.0);
    const double hi = std::log10(20000.0);
    for (int i = 0; i < points; ++i) {
        const double f = std::pow(10.0, lo + (hi - lo) * i / (points - 1));
        curve.append({f, responseDb(settings, f)});
    }
    return curve;
}

double headroomPreampDb(const Settings &settings)
{
    Settings withoutPreamp = settings;
    withoutPreamp.preampDb = 0.0;
    double peak = 0.0;
    for (const QPointF &p : responseCurve(withoutPreamp, 240))
        peak = std::max(peak, p.y());
    return -std::ceil(peak * 10.0) / 10.0;
}

} // namespace v9::eq
