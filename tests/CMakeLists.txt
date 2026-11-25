include(ECMAddTests)

ecm_add_test(applicationicontest.cpp applicationicontest.qrc
    TEST_NAME applicationicontest
    LINK_LIBRARIES Qt6::Test KF6::IconThemes
    NAME_PREFIX "opusfm-"
    GUI
)
add_test(NAME opusfm-installer COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/installtest.sh ${PROJECT_SOURCE_DIR}/install.sh)

set(opusfm_desktop_file ${PROJECT_BINARY_DIR}/src/app/org.kde.opusfilemanager.desktop)
set(opusfm_metainfo_file ${PROJECT_BINARY_DIR}/src/app/org.kde.opusfilemanager.metainfo.xml)
ecm_add_test(appidentitytest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
)
add_dependencies(appidentitytest opusfm-metadata)
ecm_add_test(brandingtest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
)
target_compile_definitions(brandingtest PRIVATE OPUSFM_LOGO_FILE="${PROJECT_SOURCE_DIR}/src/app/branding/plasma-opus-logo.svg")
ecm_add_test(startuppresentationtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)
target_compile_definitions(appidentitytest PRIVATE
    OPUSFM_PROJECT_VERSION="${PROJECT_VERSION}"
    OPUSFM_DESKTOP_FILE="${opusfm_desktop_file}"
    OPUSFM_METAINFO_FILE="${opusfm_metainfo_file}"
)
find_program(APPSTREAMCLI_EXECUTABLE appstreamcli)
if(APPSTREAMCLI_EXECUTABLE)
    add_test(NAME opusfm-appstream-validate COMMAND ${APPSTREAMCLI_EXECUTABLE} validate --no-net --pedantic --explain ${opusfm_metainfo_file})
endif()
find_program(DESKTOP_FILE_VALIDATE_EXECUTABLE desktop-file-validate)
if(DESKTOP_FILE_VALIDATE_EXECUTABLE)
    add_test(NAME opusfm-desktop-file-validate COMMAND ${DESKTOP_FILE_VALIDATE_EXECUTABLE} ${opusfm_desktop_file})
endif()

set(translation_test_catalog ${CMAKE_CURRENT_BINARY_DIR}/translation-locale/fa/LC_MESSAGES/opus-file-manager.mo)
add_custom_command(
    OUTPUT ${translation_test_catalog}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/translation-locale/fa/LC_MESSAGES
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${translation_test_catalog} ${CMAKE_CURRENT_SOURCE_DIR}/data/translation/opus-file-manager.po
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/translation/opus-file-manager.po
    VERBATIM
)
add_custom_target(translation-test-catalog DEPENDS ${translation_test_catalog})
set(translation_production_catalog ${CMAKE_CURRENT_BINARY_DIR}/production-locale/fa/LC_MESSAGES/opus-file-manager.mo)
add_custom_command(
    OUTPUT ${translation_production_catalog}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/production-locale/fa/LC_MESSAGES
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --check -o ${translation_production_catalog} ${PROJECT_SOURCE_DIR}/po/fa/opus-file-manager.po
    DEPENDS ${PROJECT_SOURCE_DIR}/po/fa/opus-file-manager.po
    VERBATIM
)
add_custom_target(translation-production-catalog DEPENDS ${translation_production_catalog})
ecm_add_test(translationtest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
)
add_dependencies(translationtest translation-test-catalog translation-production-catalog opus-file-manager)
target_compile_definitions(translationtest PRIVATE
    OPUSFM_TEST_LOCALE_DIR="${CMAKE_CURRENT_BINARY_DIR}/translation-locale"
    OPUSFM_PRODUCTION_LOCALE_DIR="${CMAKE_CURRENT_BINARY_DIR}/production-locale"
    OPUSFM_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    OPUSFM_EXECUTABLE="$<TARGET_FILE:opus-file-manager>"
)

ecm_add_test(informationtranslationtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)
add_dependencies(informationtranslationtest translation-test-catalog)
target_compile_definitions(informationtranslationtest PRIVATE
    OPUSFM_TEST_LOCALE_DIR="${CMAKE_CURRENT_BINARY_DIR}/translation-locale"
)

ecm_add_tests(
    appearancedefaultstest.cpp
    appfontstest.cpp
    archivestest.cpp
    batchrenametest.cpp
    batchrenamepresetstest.cpp
    checksumstest.cpp
    customcommandstest.cpp
    favoritesmodeltest.cpp
    filesplittertest.cpp
    foldermetadatatest.cpp
    foldercolorstest.cpp
    foldercolorrulestest.cpp
    foldercomparisontest.cpp
    foldersynctest.cpp
    foldersyncprofilestest.cpp
    flatviewtest.cpp
    ignorelisttest.cpp
    indexedsearchtest.cpp
    libraryscantest.cpp
    linkednavigationtest.cpp
    localizeddatestest.cpp
    livesearchtest.cpp
    namefiltertest.cpp
    densitytest.cpp
    draftstoretest.cpp
    duplicatefindertest.cpp
    executablestest.cpp
    imageeditingtest.cpp
    imageconversiontest.cpp
    imagedetailstest.cpp
    markdownexporttest.cpp
    metadataeditortest.cpp
    metadataimporttest.cpp
    navigationhistorytest.cpp
    previewcontenttest.cpp
    servicemenustest.cpp
    sessiontest.cpp
    savedpathfilterstest.cpp
    savedsearchestest.cpp
    searchcollectiontest.cpp
    searchhistorytest.cpp
    searchlocationstest.cpp
    searchquerytest.cpp
    tabcolorstest.cpp
    startuptest.cpp
    stylesheetstest.cpp
    symlinkstest.cpp
    symboliciconstest.cpp
    tabmodeltest.cpp
    taglibrarytest.cpp
    themeprofilestest.cpp
    treemaptest.cpp
    transferqueuetest.cpp
    viewsettingstest.cpp
    workspaceappearancetest.cpp
    workspacecatalogtest.cpp
    workspacerestoretest.cpp
    workspacestartuptest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test KF6::ConfigCore
    NAME_PREFIX "opusfm-"
)

ecm_add_test(galleryeditingtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(workspacedialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(workspacelocationstest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(batchrenamedialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(customcommanddialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(tagginguitest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(metadataeditdialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(foldersyncdialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(transferqueuedialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(editorpaneltest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(listingexporttest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(contentcharttest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(duplicatefinderdialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(searchwidgettest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(doubleshifttest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(filesplitdialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(terminalpaneltest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(remotepreviewtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test Qt6::Network
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(mainwindowtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(settingsbackuptest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
)

ecm_add_test(
    settingsdialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(combinedpaneltest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)
ecm_add_test(colorschemestest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(settingstest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(filtertest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(tabstest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(archivebrowsingtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test KF6::Archive
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(archivewritertest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test KF6::Archive
    NAME_PREFIX "opusfm-"
)

ecm_add_test(flattest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(symbolicappiconstest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

if(OPUSFM_RUST_PROTOTYPE)
    ecm_add_test(rustsearchquerytest.cpp
        LINK_LIBRARIES opusfm_core opus_searchquery Qt6::Test Qt6::Concurrent
        NAME_PREFIX "opusfm-"
    )
    add_test(NAME opusfm-rust-searchquery-cargo
        COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${CMAKE_CURRENT_BINARY_DIR}/cargo-test
            ${Rust_CARGO_CACHED} test --offline --manifest-path ${PROJECT_SOURCE_DIR}/rust/searchquery/Cargo.toml
    )
endif()
