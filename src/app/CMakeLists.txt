add_library(opusfm_app STATIC
    appearancestyle.cpp
    appearancestyle.h
    batchconversiondialog.cpp
    batchconversiondialog.h
    batchrenamedialog.cpp
    batchrenamedialog.h
    carddelegate.cpp
    carddelegate.h
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
    customstylesheet.cpp
    customstylesheet.h
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
    searchpanel.cpp
    searchpanel.h
    searchstoredialog.cpp
    searchstoredialog.h
    searchwidget.cpp
    searchwidget.h
    settingsdialog.cpp
    settingsdialog.h
    settingswidgets.cpp
    settingswidgets.h
    startuppresentation.cpp
    startuppresentation.h
    symbolicactionicons.cpp
    symbolicactionicons.h
    terminalpanel.cpp
    terminalpanel.h
    toolbarcustomizer.cpp
    toolbarcustomizer.h
    transferqueuedialog.cpp
    transferqueuedialog.h
    treelines.cpp
    treelines.h
    windoweffects.cpp
    windoweffects.h
    workspacedialog.cpp
    workspacedialog.h
    workspaceappearanceeditor.cpp
    workspaceappearanceeditor.h
    workspacelocations.cpp
    workspacelocations.h
    opusfm.qrc
    branding.qrc
)

target_include_directories(opusfm_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/src)

target_link_libraries(opusfm_app
    PUBLIC
        opusfm_core
        Qt6::Concurrent
        Qt6::Widgets
        Qt6::Xml
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::Bookmarks
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
        KF6::WindowSystem
        KF6::XmlGui
)

add_executable(opus-file-manager main.cpp applicationicon.qrc)

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
configure_file(org.kde.opusfilemanager.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.desktop.untranslated @ONLY)

# Merge the catalogues' translations of the desktop entry and AppStream
# metadata. msgfmt expects <language>.po files and a LINGUAS list in one folder.
set(opusfm_desktop_file ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.desktop)
set(opusfm_metainfo_file ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.metainfo.xml)
set(opusfm_metadata_po_dir ${CMAKE_CURRENT_BINARY_DIR}/metadata-po)
set(opusfm_metadata_languages)
set(opusfm_metadata_catalogs)
if(NOT KF_SKIP_PO_PROCESSING)
    file(GLOB opusfm_po_files ${PROJECT_SOURCE_DIR}/po/*/opus-file-manager.po)
    foreach(po_file IN LISTS opusfm_po_files)
        get_filename_component(language_dir ${po_file} DIRECTORY)
        get_filename_component(language ${language_dir} NAME)
        configure_file(${po_file} ${opusfm_metadata_po_dir}/${language}.po COPYONLY)
        list(APPEND opusfm_metadata_languages ${language})
        list(APPEND opusfm_metadata_catalogs ${opusfm_metadata_po_dir}/${language}.po)
    endforeach()
endif()
list(JOIN opusfm_metadata_languages "\n" opusfm_linguas)
file(CONFIGURE OUTPUT ${opusfm_metadata_po_dir}/LINGUAS CONTENT "${opusfm_linguas}\n")
add_custom_command(
    OUTPUT ${opusfm_desktop_file}
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --desktop --template=${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.desktop.untranslated
            -d ${opusfm_metadata_po_dir} -o ${opusfm_desktop_file}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.desktop.untranslated ${opusfm_metadata_po_dir}/LINGUAS ${opusfm_metadata_catalogs}
    VERBATIM
)
add_custom_command(
    OUTPUT ${opusfm_metainfo_file}
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --xml --template=${CMAKE_CURRENT_SOURCE_DIR}/org.kde.opusfilemanager.metainfo.xml
            -d ${opusfm_metadata_po_dir} -o ${opusfm_metainfo_file}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/org.kde.opusfilemanager.metainfo.xml ${opusfm_metadata_po_dir}/LINGUAS ${opusfm_metadata_catalogs}
    VERBATIM
)
add_custom_target(opusfm-metadata ALL DEPENDS ${opusfm_desktop_file} ${opusfm_metainfo_file})

install(TARGETS opus-file-manager ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS ${opusfm_desktop_file} DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${opusfm_metainfo_file} DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES org.kde.opusfilemanager.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
# Hand-drawn small sizes keep the mark legible on the pixel grid, where the
# scaled-down scalable icon would blur.
foreach(icon_size 16x16 22x22 32x32)
    install(FILES icons/${icon_size}/org.kde.opusfilemanager.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/${icon_size}/apps)
endforeach()
install(FILES icons/symbolic/org.kde.opusfilemanager-symbolic.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/symbolic/apps)
