#pragma once

#include "AudioMixer.h"
#include "HeadsetController.h"

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

struct AudioMixerForeign
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
