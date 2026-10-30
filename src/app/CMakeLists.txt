qt_add_executable(v9pro-control
    main.cpp
)

# Must precede qt_add_qml_module so the generated qmldir marks Theme as a singleton.
set_source_files_properties(qml/Theme.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(v9pro-control
    URI V9Control
    SOURCES
        QmlTypes.h
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/components/ActionButton.qml
        qml/components/BatteryRing.qml
        qml/components/Card.qml
        qml/components/EqualizerGraph.qml
        qml/components/IconButton.qml
        qml/components/LevelSlider.qml
        qml/components/ModeTile.qml
        qml/components/NavButton.qml
        qml/components/ProfileItem.qml
        qml/components/SectionHeader.qml
        qml/components/SegmentedControl.qml
        qml/components/StatusPill.qml
        qml/components/StyledComboBox.qml
        qml/components/StyledTextField.qml
        qml/components/ToggleSwitch.qml
        qml/components/WarningBanner.qml
        qml/pages/DevicePage.qml
        qml/pages/EqualizerPage.qml
        qml/pages/HomePage.qml
        qml/pages/FirmwarePanel.qml
        qml/pages/LabPage.qml
    RESOURCES
        qml/icons/battery.svg
        qml/icons/equalizer.svg
        qml/icons/flask.svg
        qml/icons/gamepad.svg
        qml/icons/headset.svg
        qml/icons/info.svg
        qml/icons/mic.svg
        qml/icons/mic-off.svg
        qml/icons/music.svg
        qml/icons/target.svg
        qml/icons/volume.svg
        qml/icons/volume-off.svg
)

target_link_libraries(v9pro-control
    PRIVATE
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Widgets
        V9::Core
)

target_compile_options(v9pro-control
    PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
)

install(TARGETS v9pro-control
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

target_compile_definitions(v9pro-control
    PRIVATE
        PROJECT_VERSION="${PROJECT_VERSION}"
)
