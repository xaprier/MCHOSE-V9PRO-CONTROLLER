#include "FilterChainConfig.h"

#include <QStringList>

#include <cmath>

namespace v9::pw {

namespace {

const char *label(eq::FilterType type)
{
    switch (type) {
    case eq::FilterType::LowShelf:
        return "bq_lowshelf";
    case eq::FilterType::HighShelf:
        return "bq_highshelf";
    case eq::FilterType::Peaking:
        break;
    }
    return "bq_peaking";
}

QString num(double value)
{
    return QString::number(value, 'f', 4);
}

double dbToLinear(double db)
{
    return std::pow(10.0, db / 20.0);
}

// Node names in signal order, matching eq::filters().
QStringList filterNames()
{
    QStringList names;
    for (int i = 1; i <= eq::BandCount; ++i)
        names << QStringLiteral("eq_%1").arg(i);
    names << QStringLiteral("bass") << QStringLiteral("brilliance") << QStringLiteral("clarity");
    return names;
}

} // namespace

QString quoted(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString equalizerConfig(const eq::Settings &settings, const QString &targetSink)
{
    const QList<eq::Biquad> filters = eq::filters(settings);
    const QStringList names = filterNames();

    QString nodes = QStringLiteral(
                        "      { type = builtin name = preamp label = mixer control = { \"Gain 1\" = %1 } }\n")
                        .arg(num(dbToLinear(settings.preampDb)));
    QString links = QStringLiteral("      { output = \"preamp:Out\" input = \"%1:In\" }\n").arg(names.first());
    for (qsizetype i = 0; i < filters.size(); ++i) {
        const eq::Biquad &f = filters[i];
        nodes += QStringLiteral("      { type = builtin name = %1 label = %2 control = { \"Freq\" = %3 \"Q\" = %4 \"Gain\" = %5 } }\n")
                     .arg(names[i], QLatin1String(label(f.type)), num(f.frequency), num(f.q), num(f.gainDb));
        if (i + 1 < filters.size())
            links += QStringLiteral("      { output = \"%1:Out\" input = \"%2:In\" }\n").arg(names[i], names[i + 1]);
    }

    return QStringLiteral(R"CFG(context.modules = [
  { name = libpipewire-module-filter-chain
    args = {
      node.description = "V9 PRO Equalizer"
      media.name = "V9 PRO Equalizer"
      audio.channels = 2
      audio.position = [ FL FR ]
      filter.graph = {
        nodes = [
%1        ]
        links = [
%2        ]
        inputs = [ "preamp:In 1" ]
        outputs = [ "%3:Out" ]
      }
      capture.props = {
        node.name = %4
        media.class = Audio/Sink
        device.icon-name = audio-headset
      }
      playback.props = {
        node.name = "v9pro_equalizer_output"
        node.passive = true
        target.object = %5
        stream.dont-remix = true
      }
    }
  }
]
)CFG")
        .arg(nodes, links, names.last(), quoted(EqualizerSinkName), quoted(targetSink));
}

QString equalizerParams(const eq::Settings &settings)
{
    const QList<eq::Biquad> filters = eq::filters(settings);
    const QStringList names = filterNames();

    QStringList params;
    params << quoted(QStringLiteral("preamp:Gain 1")) << num(dbToLinear(settings.preampDb));
    for (qsizetype i = 0; i < filters.size(); ++i) {
        params << quoted(names[i] + QStringLiteral(":Freq")) << num(filters[i].frequency);
        params << quoted(names[i] + QStringLiteral(":Gain")) << num(filters[i].gainDb);
    }
    return QStringLiteral("{ params = [ %1 ] }").arg(params.join(QLatin1Char(' ')));
}

QString noiseSuppressionConfig(NoiseEngine engine, const QString &targetSource, int vadThreshold)
{
    if (engine == NoiseEngine::RNNoise) {
        return QStringLiteral(R"CFG(context.modules = [
  { name = libpipewire-module-filter-chain
    args = {
      node.description = "V9 PRO Microphone (noise suppressed)"
      media.name = "V9 PRO Microphone (noise suppressed)"
      audio.channels = 1
      audio.position = [ MONO ]
      filter.graph = {
        nodes = [
          { type = ladspa name = rnnoise plugin = "librnnoise_ladspa" label = noise_suppressor_mono
            control = { "VAD Threshold (%)" = %1 } }
        ]
      }
      capture.props = {
        node.name = "v9pro_mic_denoise_input"
        node.passive = true
        target.object = %2
      }
      playback.props = {
        node.name = %3
        media.class = Audio/Source
        device.icon-name = audio-input-microphone
      }
    }
  }
]
)CFG")
            .arg(num(qBound(0, vadThreshold, 99)), quoted(targetSource), quoted(NoiseSourceName));
    }

    // Echo cancellation runs too, against the (silent) aec sink; only noise suppression matters here.
    return QStringLiteral(R"CFG(context.modules = [
  { name = libpipewire-module-echo-cancel
    args = {
      library.name = aec/libspa-aec-webrtc
      audio.channels = 1
      audio.position = [ MONO ]
      aec.args = {
        webrtc.noise_suppression = true
        webrtc.high_pass_filter = true
        webrtc.gain_control = false
        webrtc.voice_detection = true
      }
      capture.props = {
        node.name = "v9pro_mic_denoise_input"
        node.passive = true
        target.object = %1
      }
      source.props = {
        node.name = %2
        node.description = "V9 PRO Microphone (noise suppressed)"
        device.icon-name = audio-input-microphone
      }
      sink.props = {
        node.name = "v9pro_mic_denoise_reference"
        node.description = "V9 PRO noise suppression reference"
        node.passive = true
      }
      playback.props = {
        node.name = "v9pro_mic_denoise_reference_output"
        node.passive = true
      }
    }
  }
]
)CFG")
        .arg(quoted(targetSource), quoted(NoiseSourceName));
}

QString noiseSuppressionParams(int vadThreshold)
{
    return QStringLiteral("{ params = [ \"rnnoise:VAD Threshold (%)\" %1 ] }").arg(num(qBound(0, vadThreshold, 99)));
}

QString micMonitorConfig(const QString &source, const QString &sink, double gain)
{
    return QStringLiteral(R"CFG(context.modules = [
  { name = libpipewire-module-filter-chain
    args = {
      node.description = "V9 PRO Microphone monitor"
      media.name = "V9 PRO Microphone monitor"
      audio.channels = 1
      audio.position = [ MONO ]
      filter.graph = {
        nodes = [
          { type = builtin name = monitor label = mixer control = { "Gain 1" = %1 } }
        ]
        inputs = [ "monitor:In 1" ]
        outputs = [ "monitor:Out" ]
      }
      capture.props = {
        node.name = "v9pro_mic_monitor_input"
        node.latency = "128/48000"
        target.object = %2
      }
      playback.props = {
        node.name = %3
        node.latency = "128/48000"
        target.object = %4
      }
    }
  }
]
)CFG")
        .arg(num(gain), quoted(source), quoted(MonitorNodeName), quoted(sink));
}

QString soundboardConfig(const QString &listenSink)
{
    const QString target = listenSink.isEmpty() ? QString() : QStringLiteral("        target.object = %1\n").arg(quoted(listenSink));
    // Lowest priorities so the session manager never picks the soundboard as the default output.
    return QStringLiteral(R"CFG(context.modules = [
  { name = libpipewire-module-loopback
    args = {
      node.description = "V9 PRO Soundboard"
      audio.position = [ MONO ]
      capture.props = {
        node.name = %1
        media.class = Audio/Sink
        priority.session = 0
        priority.driver = 0
      }
      playback.props = {
        node.name = %2
%3        node.dont-reconnect = true
      }
    }
  }
]
)CFG")
        .arg(quoted(SoundboardSinkName), quoted(SoundboardListenNodeName), target);
}

QString soundboardListenParams(bool listen)
{
    return listen ? QStringLiteral("{ mute: false }") : QStringLiteral("{ mute: true }");
}

QString micMonitorParams(double gain)
{
    return QStringLiteral("{ params = [ \"monitor:Gain 1\" %1 ] }").arg(num(gain));
}

} // namespace v9::pw
