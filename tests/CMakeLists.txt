include(ECMAddTests)

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
    LINK_LIBRARIES opusfm_core Qt6::Test KF6::ConfigCore
    NAME_PREFIX "opusfm-"
)

ecm_add_test(galleryeditingtest.cpp
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
