#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

// MCHOSE V9 PRO (C-Media CM2025) "AOTA" firmware images: parsing, the voice-prompt bank in
// tone.bin, and silencing chosen prompts. Format notes: docs/ANALYSIS.md §2.4, credit to the
// Be-Quiet-MCHOSE write-up. Only tone.bin is ever modified; size and layout never change.
namespace v9::fw {

struct Partition {
    QString name;
    quint32 offset = 0;
    quint32 size = 0;
    quint32 crc = 0;
    int directoryOffset = 0; // where this entry lives in the 0x200 table
};

struct Clip {
    int index = 0;
    int offset = 0; // into decrypted tone.bin
    int length = 0; // bytes, multiple of FrameSize
    double seconds() const;
};

enum class PromptLanguage {
    Unknown,
    English,
    Chinese,
};

inline constexpr int FrameSize = 32;
inline constexpr int MicOnClip = 8;  // English V9 PRO firmware, confirmed by Be-Quiet-MCHOSE
inline constexpr int MicOffClip = 9;

quint32 crc32(const QByteArray &data);
QByteArray xorPad(const QByteArray &data);

class Image
{
public:
    // Returns nullopt and sets `error` when the data is not a usable V9 PRO headset image.
    static std::optional<Image> parse(const QByteArray &data, QString *error = nullptr);

    const QByteArray &data() const { return m_data; }
    const QList<Partition> &partitions() const { return m_partitions; }
    const QList<Clip> &clips() const { return m_clips; }
    PromptLanguage language() const { return m_language; }

    // A repacked copy of the image with `clipIndices` replaced by comfort noise. Each clip keeps
    // its sync frame so the codec stream stays valid. Empty list → byte-identical copy.
    std::optional<QByteArray> silenced(const QList<int> &clipIndices, QString *error = nullptr) const;

    // True when every listed clip is already silent in this image.
    bool isSilenced(const QList<int> &clipIndices) const;

private:
    std::optional<QByteArray> repack(const QByteArray &toneEncrypted, QString *error) const;

    QByteArray m_data;
    QList<Partition> m_partitions;
    int m_tone = -1;
    int m_otaXml = -1;
    QByteArray m_tonePlain;
    QList<Clip> m_clips;
    PromptLanguage m_language = PromptLanguage::Unknown;
};

} // namespace v9::fw
