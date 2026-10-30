#pragma once

#include "AudioMixer.h"
#include "EqualizerEngine.h"
#include "HeadsetController.h"
#include "MicMonitor.h"
#include "NoiseSuppressor.h"
#include "PipeWireGraph.h"
#include "ReportLogModel.h"
#include "Soundboard.h"

#include <QQmlEngine>

// The core library stays free of QtQml; these foreign declarations expose its objects as QML
// singletons. Instances are created and owned by main() before the engine loads.

struct HeadsetForeign
{
    Q_GADGET
    QML_FOREIGN(HeadsetController)
    QML_NAMED_ELEMENT(Headset)
    QML_SINGLETON

public:
    static inline HeadsetController *instance = nullptr;

    static HeadsetController *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct MixerForeign
{
    Q_GADGET
    QML_FOREIGN(AudioMixer)
    QML_NAMED_ELEMENT(Mixer)
    QML_SINGLETON

public:
    static inline AudioMixer *instance = nullptr;

    static AudioMixer *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct EqualizerForeign
{
    Q_GADGET
    QML_FOREIGN(EqualizerEngine)
    QML_NAMED_ELEMENT(Equalizer)
    QML_SINGLETON

public:
    static inline EqualizerEngine *instance = nullptr;

    static EqualizerEngine *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct NoiseSuppressionForeign
{
    Q_GADGET
    QML_FOREIGN(NoiseSuppressor)
    QML_NAMED_ELEMENT(NoiseSuppression)
    QML_SINGLETON

public:
    static inline NoiseSuppressor *instance = nullptr;

    static NoiseSuppressor *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct MonitorForeign
{
    Q_GADGET
    QML_FOREIGN(MicMonitor)
    QML_NAMED_ELEMENT(Monitor)
    QML_SINGLETON

public:
    static inline MicMonitor *instance = nullptr;

    static MicMonitor *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct HidLogForeign
{
    Q_GADGET
    QML_FOREIGN(ReportLogModel)
    QML_NAMED_ELEMENT(HidLog)
    QML_SINGLETON

public:
    static inline ReportLogModel *instance = nullptr;

    static ReportLogModel *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct PatchbayForeign
{
    Q_GADGET
    QML_FOREIGN(PipeWireGraph)
    QML_NAMED_ELEMENT(Patchbay)
    QML_SINGLETON

public:
    static inline PipeWireGraph *instance = nullptr;

    static PipeWireGraph *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};

struct SoundboardForeign
{
    Q_GADGET
    QML_FOREIGN(Soundboard)
    QML_NAMED_ELEMENT(Soundboard)
    QML_SINGLETON

public:
    static inline Soundboard *instance = nullptr;

    static Soundboard *create(QQmlEngine *, QJSEngine *engine)
    {
        Q_ASSERT(instance);
        Q_ASSERT(engine->thread() == instance->thread());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};
