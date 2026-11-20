add_library(opusfm_app STATIC
    batchconversiondialog.cpp
    batchconversiondialog.h
    batchrenamedialog.cpp
    batchrenamedialog.h
    checksumdialog.cpp
    checksumdialog.h
    combinedpanel.cpp
    combinedpanel.h
    compressdialog.cpp
    compressdialog.h
    contentheight.cpp
    contentheight.h
    contentchartdialog.cpp
    contentchartdialog.h
    cushiontreemap.cpp
    cushiontreemap.h
    customcommanddialog.cpp
    customcommanddialog.h
    densitydelegate.cpp
    densitydelegate.h
    detailsview.cpp
    detailsview.h
    duplicatefinderdialog.cpp
    duplicatefinderdialog.h
    filelistminimap.cpp
    filelistminimap.h
    editorpanel.cpp
    editorpanel.h
    filediffdialog.cpp
    filediffdialog.h
    filesplitdialog.cpp
    filesplitdialog.h
    fileoperations.cpp
    fileoperations.h
    filterbar.cpp
    filterbar.h
    folderview.cpp
    folderview.h
    foldertree.cpp
    foldertree.h
    foldercomparisondialog.cpp
    foldercomparisondialog.h
    foldersyncdialog.cpp
    foldersyncdialog.h
    gallerywindow.cpp
    gallerywindow.h
    imagebackground.cpp
    imagebackground.h
    informationpanel.cpp
    informationpanel.h
    mainwindow.cpp
    mainwindow.h
    metadataeditdialog.cpp
    metadataeditdialog.h
    mediaview.cpp
    mediaview.h
    pane.cpp
    pane.h
    pathfiltereditor.cpp
    pathfiltereditor.h
    pathfilterpresetsdialog.cpp
    pathfilterpresetsdialog.h
    previewpanel.cpp
    previewpanel.h
    searchdialog.cpp
    searchdialog.h
    searchstoredialog.cpp
    searchstoredialog.h
    settingsdialog.cpp
    settingsdialog.h
    settingswidgets.cpp
    settingswidgets.h
    symbolicactionicons.cpp
    symbolicactionicons.h
    terminalpanel.cpp
    terminalpanel.h
    toolbarcustomizer.cpp
    toolbarcustomizer.h
    transferqueuedialog.cpp
    transferqueuedialog.h
    opusfm.qrc
)

target_include_directories(opusfm_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/src)

target_link_libraries(opusfm_app
    PUBLIC
        opusfm_core
        Qt6::Concurrent
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::CoreAddons
        KF6::I18n
        KF6::KIOCore
        KF6::KIOFileWidgets
        KF6::KIOGui
        KF6::KIOWidgets
        KF6::ItemViews
        KF6::Parts
        KF6::Solid
        KF6::SyntaxHighlighting
        KF6::TextEditor
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
