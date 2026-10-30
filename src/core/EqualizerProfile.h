#pragma once

#include "EqualizerMath.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <array>
#include <optional>

namespace v9::eq {

// A named set of band gains, stored in the same JSON shape MCHOSE HUB exports so files can be
// exchanged with it:  { "name": "Bass boost", "data": [ -4, -3, ... ] }
struct Profile {
    QString name;
    std::array<double, BandCount> gains{};
    bool builtIn = false;

    bool operator==(const Profile &) const = default;
};

// Nothing in a file is trusted: `data` is padded/truncated to ten bands, numbers may carry a
// decimal point, and each gain is clamped to ±20 dB. A missing name falls back to `fallbackName`.
std::optional<Profile> parseProfile(const QByteArray &json, const QString &fallbackName, QString *error = nullptr);
QByteArray serializeProfile(const Profile &profile);

QList<Profile> builtInProfiles();

// File name for a profile, with characters unsafe in file names replaced.
QString profileFileName(const QString &name);

} // namespace v9::eq
