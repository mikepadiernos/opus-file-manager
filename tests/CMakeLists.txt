include(ECMAddTests)

ecm_add_tests(
    archivestest.cpp
    favoritesmodeltest.cpp
    foldermetadatatest.cpp
    flatviewtest.cpp
    ignorelisttest.cpp
    namefiltertest.cpp
    densitytest.cpp
    draftstoretest.cpp
    executablestest.cpp
    imageeditingtest.cpp
    imageconversiontest.cpp
    imagedetailstest.cpp
    markdownexporttest.cpp
    navigationhistorytest.cpp
    previewcontenttest.cpp
    servicemenustest.cpp
    sessiontest.cpp
    tabcolorstest.cpp
    startuptest.cpp
    symlinkstest.cpp
    symboliciconstest.cpp
    tabmodeltest.cpp
    viewsettingstest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test KF6::ConfigCore
    NAME_PREFIX "opusfm-"
)

ecm_add_test(galleryeditingtest.cpp
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
