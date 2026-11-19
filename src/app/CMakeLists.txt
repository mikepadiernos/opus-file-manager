add_library(opusfm_app STATIC
    combinedpanel.cpp
    combinedpanel.h
    contentheight.cpp
    contentheight.h
    fileoperations.cpp
    fileoperations.h
    folderview.cpp
    folderview.h
    foldertree.cpp
    foldertree.h
    mainwindow.cpp
    mainwindow.h
    pane.cpp
    pane.h
    opusfm.qrc
)

target_include_directories(opusfm_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/src)

target_link_libraries(opusfm_app
    PUBLIC
        opusfm_core
        Qt6::Widgets
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        KF6::KIOCore
        KF6::KIOFileWidgets
        KF6::KIOGui
        KF6::KIOWidgets
        KF6::WidgetsAddons
        KF6::XmlGui
)

add_executable(opus-file-manager main.cpp)

target_link_libraries(opus-file-manager
    PRIVATE
        opusfm_app
        Qt6::DBus
        KF6::Crash
        KF6::DBusAddons
        KF6::WindowSystem
)

# Exec= carries the absolute bindir so a --user install under ~/.local works
# even when ~/.local/bin is not on PATH.
configure_file(org.kde.opusfilemanager.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.desktop @ONLY)

install(TARGETS opus-file-manager ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.desktop DESTINATION ${KDE_INSTALL_APPDIR})
