qt_add_library(v9core STATIC
    AudioMixer.cpp
    AudioMixer.h
    EqualizerEngine.cpp
    EqualizerEngine.h
    EqualizerMath.cpp
    EqualizerMath.h
    EqualizerProfile.cpp
    EqualizerProfile.h
    FilterChainConfig.cpp
    FilterChainConfig.h
    HeadsetController.cpp
    HeadsetController.h
    HidRawDevice.cpp
    HidRawDevice.h
    LinkRules.cpp
    LinkRules.h
    MicMonitor.cpp
    MicMonitor.h
    NoiseSuppressor.cpp
    NoiseSuppressor.h
    PipeWireFilter.cpp
    PipeWireGraphState.cpp
    PipeWireGraphState.h
    PipeWireFilter.h
    ReportLogModel.cpp
    ReportLogModel.h
    V9Protocol.cpp
    V9Protocol.h
)

add_library(V9::Core ALIAS v9core)

target_include_directories(v9core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(v9core
    PUBLIC
        Qt6::Core
)

target_compile_options(v9core
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)
