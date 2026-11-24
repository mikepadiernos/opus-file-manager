include(ECMAddTests)

ecm_add_test(applicationicontest.cpp applicationicontest.qrc
    TEST_NAME applicationicontest
    LINK_LIBRARIES Qt6::Test KF6::IconThemes
    NAME_PREFIX "opusfm-"
    GUI
)
add_test(NAME opusfm-installer COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/installtest.sh ${PROJECT_SOURCE_DIR}/install.sh)

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
    archivestest.cpp
    batchrenametest.cpp
    batchrenamepresetstest.cpp
    checksumstest.cpp
    customcommandstest.cpp
    favoritesmodeltest.cpp
    filesplittertest.cpp
    foldermetadatatest.cpp
    foldercomparisontest.cpp
    foldersynctest.cpp
    foldersyncprofilestest.cpp
    flatviewtest.cpp
    ignorelisttest.cpp
    indexedsearchtest.cpp
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
    navigationhistorytest.cpp
    previewcontenttest.cpp
    servicemenustest.cpp
    sessiontest.cpp
    savedpathfilterstest.cpp
    savedsearchestest.cpp
    searchcollectiontest.cpp
    tabcolorstest.cpp
    startuptest.cpp
    symlinkstest.cpp
    symboliciconstest.cpp
    tabmodeltest.cpp
    treemaptest.cpp
    transferqueuetest.cpp
    viewsettingstest.cpp
    workspacecatalogtest.cpp
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

ecm_add_test(searchdialogtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
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
