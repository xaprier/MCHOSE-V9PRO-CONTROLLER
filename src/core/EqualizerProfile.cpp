#include "EqualizerProfile.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace v9::eq {

std::optional<Profile> parseProfile(const QByteArray &json, const QString &fallbackName, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = parseError.error != QJsonParseError::NoError ? parseError.errorString() : QStringLiteral("not a JSON object");
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    const QJsonValue data = object.value(QStringLiteral("data"));
    if (!data.isArray()) {
        if (error)
            *error = QStringLiteral("no \"data\" array in the file");
        return std::nullopt;
    }

    Profile profile;
    profile.name = object.value(QStringLiteral("name")).toString().trimmed();
    if (profile.name.isEmpty())
        profile.name = fallbackName;

    const QJsonArray values = data.toArray();
    for (int i = 0; i < BandCount && i < values.size(); ++i)
        profile.gains[i] = std::clamp(std::round(values[i].toDouble()), MinGainDb, MaxGainDb);
    return profile;
}

QByteArray serializeProfile(const Profile &profile)
{
    QJsonArray data;
    for (double gain : profile.gains)
        data.append(gain);
    return QJsonDocument(QJsonObject{{QStringLiteral("name"), profile.name}, {QStringLiteral("data"), data}}).toJson();
}

QList<Profile> builtInProfiles()
{
    auto make = [](const char *name, std::array<double, BandCount> gains) {
        return Profile{QString::fromUtf8(name), gains, true};
    };
    return {
        make("Flat", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}),
        make("Bass boost", {6, 5, 4, 2, 0, 0, 0, 0, 0, 0}),
        make("FPS footsteps", {-4, -3, -2, -1, 0, 2, 4, 5, 3, 0}),
        make("Vocal", {-2, -2, -1, 0, 2, 4, 4, 2, 0, -1}),
        make("Treble boost", {0, 0, 0, 0, 0, 0, 1, 3, 5, 6}),
        make("Movie", {4, 3, 2, 0, -1, 0, 2, 3, 2, 1}),
    };
}

QString profileFileName(const QString &name)
{
    static const QRegularExpression unsafe(QStringLiteral(R"([/\\:*?"<>|\x00-\x1f])"));
    QString safe = name.trimmed();
    safe.replace(unsafe, QStringLiteral("_"));
    if (safe.isEmpty() || safe.startsWith(QLatin1Char('.')))
        safe.prepend(QStringLiteral("profile"));
    return safe + QStringLiteral(".json");
}

} // namespace v9::eq
