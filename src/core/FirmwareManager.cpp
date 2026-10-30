#include "FirmwareManager.h"

#include "HeadsetController.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QStandardPaths>

namespace fw = v9::fw;

namespace {

const QString OtaBase = QStringLiteral("https://cdn.mchose.com.cn/OTA/PC/prod/otaFile/");
const QString ManifestUrl = OtaBase + QStringLiteral("otaConfig_prod.json");
const QString ProductKey = QStringLiteral("MCHOSE V9 PRO PID14429");

QString clipRole(fw::PromptLanguage language, int index)
{
    if (language != fw::PromptLanguage::English)
        return {};
    if (index == fw::MicOnClip)
        return QStringLiteral("Mic on");
    if (index == fw::MicOffClip)
        return QStringLiteral("Mic off");
    return {};
}

} // namespace

FirmwareManager::FirmwareManager(HeadsetController *headset, QObject *parent)
    : QObject(parent)
    , m_headset(headset)
    , m_flasher(new FirmwareFlasher(this))
{
    connect(m_flasher, &FirmwareFlasher::stateChanged, this, &FirmwareManager::flashStateChanged);
    connect(m_flasher, &FirmwareFlasher::logMessage, this, &FirmwareManager::addLog);
    connect(m_flasher, &FirmwareFlasher::progressChanged, this, [this](double progress) {
        m_flashProgress = progress;
        emit flashProgressChanged();
    });
    connect(m_flasher, &FirmwareFlasher::finished, this, [this] {
        m_headset->setSuspended(false);
        emit flashStateChanged();
    });
}

QString FirmwareManager::firmwareDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/v9pro-control/firmware");
}

QString FirmwareManager::imageSummary() const
{
    if (!m_image)
        return {};
    const char *language = m_image->language() == fw::PromptLanguage::English ? "English prompts"
                         : m_image->language() == fw::PromptLanguage::Chinese ? "Chinese prompts"
                                                                                : "unknown prompt layout";
    return tr("%1 KiB · %2 partitions · %3 clips · %4")
        .arg(m_image->data().size() / 1024)
        .arg(m_image->partitions().size())
        .arg(m_image->clips().size())
        .arg(QString::fromLatin1(language));
}

QVariantList FirmwareManager::clips() const
{
    QVariantList list;
    if (!m_image)
        return list;
    for (const fw::Clip &clip : m_image->clips()) {
        list.append(QVariantMap{
            {QStringLiteral("index"), clip.index},
            {QStringLiteral("seconds"), clip.seconds()},
            {QStringLiteral("role"), clipRole(m_image->language(), clip.index)},
            {QStringLiteral("selected"), m_selected.contains(clip.index)},
            {QStringLiteral("silent"), m_image->isSilenced({clip.index})},
        });
    }
    return list;
}

bool FirmwareManager::loadFile(const QUrl &file)
{
    QFile in(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!in.open(QIODevice::ReadOnly)) {
        addLog(tr("Cannot open %1").arg(in.fileName()));
        return false;
    }
    setImage(in.readAll(), QFileInfo(in).fileName());
    return m_image.has_value();
}

void FirmwareManager::setImage(const QByteArray &data, const QString &name)
{
    QString error;
    m_image = fw::Image::parse(data, &error);
    m_patched.clear();
    m_selected.clear();
    if (!m_image) {
        m_imageName.clear();
        addLog(tr("%1: %2").arg(name, error));
    } else {
        m_imageName = name;
        if (m_image->language() == fw::PromptLanguage::English)
            m_selected = {fw::MicOnClip, fw::MicOffClip};
        addLog(tr("Loaded %1 (%2)").arg(name, imageSummary()));
    }
    emit imageChanged();
    emit clipsChanged();
    emit patchedChanged();
}

void FirmwareManager::downloadStock(bool english)
{
    if (m_busy)
        return;
    if (!m_network)
        m_network = new QNetworkAccessManager(this);

    setBusy(true);
    addLog(tr("Fetching OTA manifest…"));
    QNetworkReply *manifest = m_network->get(QNetworkRequest(QUrl(ManifestUrl)));
    connect(manifest, &QNetworkReply::finished, this, [this, manifest, english] {
        manifest->deleteLater();
        if (manifest->error() != QNetworkReply::NoError) {
            addLog(tr("Manifest download failed: %1").arg(manifest->errorString()));
            setBusy(false);
            return;
        }
        const QJsonObject product = QJsonDocument::fromJson(manifest->readAll()).object().value(ProductKey).toObject();
        const QString dir = product.value(QStringLiteral("dir")).toString();
        QString fileName;
        // type 1 = English prompts, type 0 = Chinese.
        for (const QJsonValue &entry : product.value(QStringLiteral("headset")).toArray()) {
            const QJsonObject object = entry.toObject();
            if (object.value(QStringLiteral("type")).toInt(-1) == (english ? 1 : 0))
                fileName = object.value(QStringLiteral("fileName")).toString();
        }
        if (dir.isEmpty() || fileName.isEmpty() || fileName.contains(QLatin1Char('/'))) {
            addLog(tr("V9 PRO headset image not listed in the manifest."));
            setBusy(false);
            return;
        }

        const QUrl url(OtaBase + dir + QStringLiteral("/headset/") + fileName);
        addLog(tr("Downloading %1…").arg(url.toString()));
        QNetworkReply *image = m_network->get(QNetworkRequest(url));
        connect(image, &QNetworkReply::finished, this, [this, image, fileName] {
            image->deleteLater();
            setBusy(false);
            if (image->error() != QNetworkReply::NoError) {
                addLog(tr("Firmware download failed: %1").arg(image->errorString()));
                return;
            }
            const QByteArray data = image->readAll();
            QDir().mkpath(firmwareDirectory());
            QFile out(firmwareDirectory() + QLatin1Char('/') + fileName);
            if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                out.write(data);
                addLog(tr("Saved stock image to %1 (keep it as your recovery image).").arg(out.fileName()));
            }
            setImage(data, fileName);
        });
    });
}

void FirmwareManager::setClipSelected(int index, bool selected)
{
    if (selected && !m_selected.contains(index))
        m_selected.append(index);
    else if (!selected)
        m_selected.removeAll(index);
    std::sort(m_selected.begin(), m_selected.end());
    m_patched.clear();
    emit clipsChanged();
    emit patchedChanged();
}

bool FirmwareManager::buildPatched()
{
    if (!m_image || m_selected.isEmpty())
        return false;

    QString error;
    const auto noop = m_image->silenced({}, &error);
    if (!noop || *noop != m_image->data()) {
        addLog(tr("Self-check failed: repacking the stock image is not byte-identical. Refusing to patch."));
        return false;
    }
    const auto patched = m_image->silenced(m_selected, &error);
    if (!patched) {
        addLog(error);
        return false;
    }
    if (!fw::Image::parse(*patched) || !fw::Image::parse(*patched)->isSilenced(m_selected)) {
        addLog(tr("Self-check failed: the patched image does not re-parse as expected."));
        return false;
    }
    m_patched = *patched;
    QStringList indices;
    for (int i : m_selected)
        indices << QString::number(i);
    addLog(tr("Built patched image silencing clips %1 (CRCs updated, size unchanged).").arg(indices.join(QStringLiteral(", "))));
    emit patchedChanged();
    return true;
}

bool FirmwareManager::savePatched(const QUrl &file)
{
    if (m_patched.isEmpty())
        return false;
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(m_patched) != m_patched.size()) {
        addLog(tr("Cannot write %1").arg(out.fileName()));
        return false;
    }
    addLog(tr("Saved patched image to %1").arg(out.fileName()));
    return true;
}

bool FirmwareManager::flash(const QString &kind, const QString &confirmation)
{
    if (confirmation != QLatin1String("FLASH") || !m_image || m_flasher->isBusy())
        return false;
    if (!m_headset->dongleConnected() || !m_headset->headsetOnline()) {
        addLog(tr("Headset must be on and connected over 2.4G to flash."));
        return false;
    }

    QByteArray payload;
    if (kind == QLatin1String("noop")) {
        const auto noop = m_image->silenced({});
        if (!noop || *noop != m_image->data())
            return false;
        payload = *noop;
    } else if (kind == QLatin1String("patched")) {
        payload = m_patched;
    } else if (kind == QLatin1String("stock")) {
        payload = m_image->data();
    }
    if (payload.isEmpty())
        return false;

    const QString node = m_headset->devicePath();
    m_headset->setSuspended(true);
    m_flashProgress = 0.0;
    emit flashProgressChanged();
    addLog(tr("Flashing %1 image to the headset via %2. Keep the headset on and close to the dongle.").arg(kind, node));
    m_flasher->start(node, payload, true);
    emit flashStateChanged();
    return true;
}

void FirmwareManager::abortFlash()
{
    m_flasher->abort();
}

void FirmwareManager::clearLog()
{
    m_log.clear();
    emit logChanged();
}

void FirmwareManager::addLog(const QString &message)
{
    m_log.append(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + message);
    emit logChanged();
}

void FirmwareManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}
