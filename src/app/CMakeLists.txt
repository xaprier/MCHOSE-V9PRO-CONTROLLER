qt_add_executable(v9pro-control
    main.cpp
    TrayIcon.cpp
    TrayIcon.h
)

qt_add_resources(v9pro-control "app_icon"
    PREFIX "/icons"
    BASE ../../packaging
    FILES
        ../../packaging/v9pro-control.svg
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
        qml/components/ConfirmDialog.qml
        qml/components/Card.qml
        qml/components/EqualizerGraph.qml
        qml/components/IconButton.qml
        qml/components/LevelSlider.qml
        qml/components/ModeTile.qml
        qml/components/NavButton.qml
        qml/components/PatchNode.qml
        qml/components/ProfileItem.qml
        qml/components/SectionHeader.qml
        qml/components/SegmentedControl.qml
        qml/components/SoundTile.qml
        qml/components/StatusPill.qml
        qml/components/StyledMenu.qml
        qml/components/StyledComboBox.qml
        qml/components/StyledTextField.qml
        qml/components/ToggleSwitch.qml
        qml/components/WarningBanner.qml
        qml/pages/DevicePage.qml
        qml/pages/EqualizerPage.qml
        qml/pages/HomePage.qml
        qml/pages/PatchbayPage.qml
        qml/pages/SoundboardPage.qml
    RESOURCES
        qml/icons/battery.svg
        qml/icons/equalizer.svg
        qml/icons/gamepad.svg
        qml/icons/headset.svg
        qml/icons/info.svg
        qml/icons/mic.svg
        qml/icons/mic-off.svg
        qml/icons/music.svg
        qml/icons/patch.svg
        qml/icons/soundboard.svg
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
install(FILES ../../packaging/v9pro-control.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(FILES ../../packaging/v9pro-control.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

target_compile_definitions(v9pro-control
    PRIVATE
        PROJECT_VERSION="${PROJECT_VERSION}"
)
