#include "FirmwareImage.h"

#include <QtEndian>

#include <array>

namespace v9::fw {

namespace {

const QByteArray Pad = QByteArray::fromHex("a74f9dd95066da064987ae89848f3e28d77c3e2ffdc9905313252ae3e0538173");
const QByteArray IdleFrame = QByteArray::fromHex("3927beae2bc2b43ac25011b293f79d6a041b00f077f9c274b82331175923e68b");
const QByteArray SyncPrefix = QByteArray::fromHex("3b27627ec383");

constexpr int DirectoryOffset = 0x200;
constexpr int HeaderEnd = 0x400;
constexpr int ToneBodyOffset = 0x280;
constexpr double SecondsPerFrame = 0.030;

// Known clip tables (offset, length) for decrypted tone.bin. Used when they match the image;
// otherwise clips are found by scanning for sync frames.
struct KnownTable {
    PromptLanguage language;
    std::array<std::pair<int, int>, 18> clips;
};
const std::array<KnownTable, 2> KnownTables{{
    {PromptLanguage::English,
     {{{864, 3680}, {5120, 3680}, {9376, 4096}, {14016, 4672}, {19232, 4640}, {24448, 4576}, {29376, 3552}, {33440, 3008},
       {36736, 3968}, {41376, 3904}, {46016, 3936}, {50112, 3008}, {53408, 3872}, {57856, 3328}, {61536, 3712},
       {65792, 4480}, {70432, 4000}, {75072, 4832}}}},
    {PromptLanguage::Chinese,
     {{{1056, 6176}, {8960, 6176}, {16672, 3360}, {20192, 4128}, {25056, 4288}, {31040, 2944}, {34336, 3104},
       {39168, 3520}, {44192, 3040}, {47392, 3008}, {50560, 3712}, {54624, 3520}, {59840, 3712}, {65056, 2752},
       {69312, 2752}, {73376, 3872}, {77792, 4256}, {83584, 4832}}}},
}};

quint32 readLe32(const QByteArray &data, int offset)
{
    return qFromLittleEndian<quint32>(data.constData() + offset);
}

void writeLe32(QByteArray &data, int offset, quint32 value)
{
    qToLittleEndian(value, data.data() + offset);
}

bool startsWithAt(const QByteArray &data, int offset, const QByteArray &prefix)
{
    return offset >= 0 && offset + prefix.size() <= data.size() && QByteArrayView(data).sliced(offset, prefix.size()) == prefix;
}

QList<Clip> scanClips(const QByteArray &tone)
{
    // Body is a flat run of 32-byte frames; a clip starts at a sync frame and ends at the first
    // idle (comfort noise) frame after it.
    QList<Clip> clips;
    int offset = ToneBodyOffset;
    while (offset + FrameSize <= tone.size()) {
        if (!startsWithAt(tone, offset, SyncPrefix)) {
            offset += FrameSize;
            continue;
        }
        int end = offset + FrameSize;
        while (end + FrameSize <= tone.size() && !startsWithAt(tone, end, IdleFrame) && !startsWithAt(tone, end, SyncPrefix))
            end += FrameSize;
        clips.append({int(clips.size()), offset, end - offset});
        offset = end;
    }
    return clips;
}

} // namespace

double Clip::seconds() const
{
    return double(length) / FrameSize * SecondsPerFrame;
}

quint32 crc32(const QByteArray &data)
{
    static const auto table = [] {
        std::array<quint32, 256> t{};
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();

    quint32 crc = 0xFFFFFFFFu;
    for (char byte : data)
        crc = table[(crc ^ quint8(byte)) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

QByteArray xorPad(const QByteArray &data)
{
    QByteArray out(data.size(), Qt::Uninitialized);
    for (qsizetype i = 0; i < data.size(); ++i)
        out[i] = char(data[i] ^ Pad[i % Pad.size()]);
    return out;
}

std::optional<Image> Image::parse(const QByteArray &data, QString *error)
{
    auto fail = [error](const QString &message) -> std::optional<Image> {
        if (error)
            *error = message;
        return std::nullopt;
    };

    if (data.size() < HeaderEnd || !data.startsWith("AOTA"))
        return fail(QStringLiteral("Not an AOTA firmware image."));

    Image image;
    image.m_data = data;
    for (int entry = DirectoryOffset; entry + 32 <= HeaderEnd; entry += 32) {
        if (data[entry] == '\0')
            break;
        Partition p;
        p.name = QString::fromLatin1(data.mid(entry, 16).split('\0').first());
        p.offset = readLe32(data, entry + 16);
        p.size = readLe32(data, entry + 20);
        p.crc = readLe32(data, entry + 28);
        p.directoryOffset = entry;
        if (p.offset == 0 && p.size == 0)
            break;
        if (qint64(p.offset) + p.size > data.size())
            return fail(QStringLiteral("Partition %1 extends past the end of the file.").arg(p.name));
        image.m_partitions.append(p);
    }

    for (qsizetype i = 0; i < image.m_partitions.size(); ++i) {
        const QString name = image.m_partitions[i].name.toLower();
        if (name == QLatin1String("tone.bin"))
            image.m_tone = int(i);
        else if (name.endsWith(QLatin1String("ota.xml")))
            image.m_otaXml = int(i);
    }
    if (image.m_tone < 0 || image.m_otaXml < 0)
        return fail(QStringLiteral("Image has no tone.bin or ota.xml; is this a headset image?"));

    const Partition &tone = image.m_partitions[image.m_tone];
    image.m_tonePlain = xorPad(data.mid(tone.offset, tone.size));
    if (!image.m_tonePlain.startsWith("sdfs"))
        return fail(QStringLiteral("tone.bin did not decrypt to a voice prompt bank."));

    for (const KnownTable &table : KnownTables) {
        if (!startsWithAt(image.m_tonePlain, table.clips[0].first, SyncPrefix))
            continue;
        image.m_language = table.language;
        for (int i = 0; i < int(table.clips.size()); ++i)
            image.m_clips.append({i, table.clips[i].first, table.clips[i].second});
        break;
    }
    if (image.m_clips.isEmpty())
        image.m_clips = scanClips(image.m_tonePlain);
    if (image.m_clips.isEmpty())
        return fail(QStringLiteral("No voice prompt clips found in tone.bin."));

    return image;
}

std::optional<QByteArray> Image::silenced(const QList<int> &clipIndices, QString *error) const
{
    QByteArray tone = m_tonePlain;
    for (int index : clipIndices) {
        if (index < 0 || index >= m_clips.size()) {
            if (error)
                *error = QStringLiteral("Clip %1 does not exist.").arg(index);
            return std::nullopt;
        }
        const Clip &clip = m_clips[index];
        if (!startsWithAt(tone, clip.offset, SyncPrefix)) {
            if (error)
                *error = QStringLiteral("Clip %1 does not start with a sync frame; unknown firmware layout.").arg(index);
            return std::nullopt;
        }
        for (int frame = clip.offset + FrameSize; frame + FrameSize <= clip.offset + clip.length; frame += FrameSize)
            tone.replace(frame, FrameSize, IdleFrame);
    }
    return repack(xorPad(tone), error);
}

bool Image::isSilenced(const QList<int> &clipIndices) const
{
    for (int index : clipIndices) {
        if (index < 0 || index >= m_clips.size())
            return false;
        const Clip &clip = m_clips[index];
        if (!startsWithAt(m_tonePlain, clip.offset + FrameSize, IdleFrame))
            return false;
    }
    return !clipIndices.isEmpty();
}

std::optional<QByteArray> Image::repack(const QByteArray &toneEncrypted, QString *error) const
{
    QByteArray out = m_data;
    const Partition &tone = m_partitions[m_tone];
    const Partition &xml = m_partitions[m_otaXml];
    Q_ASSERT(toneEncrypted.size() == qsizetype(tone.size));
    out.replace(tone.offset, tone.size, toneEncrypted);

    const quint32 toneCrc = crc32(toneEncrypted);
    writeLe32(out, tone.directoryOffset + 28, toneCrc);

    // ota.xml repeats the tone CRC as text: <checksum>0x........</checksum>.
    const QByteArray xmlBytes = out.mid(xml.offset, xml.size);
    const qsizetype name = xmlBytes.indexOf("tone.bin");
    const qsizetype open = name < 0 ? -1 : xmlBytes.indexOf("<checksum>", name);
    const qsizetype start = open < 0 ? -1 : open + qsizetype(sizeof("<checksum>") - 1);
    const qsizetype end = start < 0 ? -1 : xmlBytes.indexOf("</checksum>", start);
    const QByteArray text = "0x" + QByteArray::number(toneCrc, 16).rightJustified(8, '0');
    if (end < 0 || end - start != text.size()) {
        if (error)
            *error = QStringLiteral("Unexpected tone.bin checksum field in ota.xml.");
        return std::nullopt;
    }
    out.replace(xml.offset + start, text.size(), text);
    writeLe32(out, xml.directoryOffset + 28, crc32(out.mid(xml.offset, xml.size)));

    writeLe32(out, 0x04, crc32(out.mid(0x08, HeaderEnd - 0x08)));
    return out;
}

} // namespace v9::fw
