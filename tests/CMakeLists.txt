include(ECMAddTests)

ecm_add_tests(
    archivestest.cpp
    favoritesmodeltest.cpp
    flatviewtest.cpp
    ignorelisttest.cpp
    namefiltertest.cpp
    navigationhistorytest.cpp
    sessiontest.cpp
    tabcolorstest.cpp
    tabmodeltest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test KF6::ConfigCore
    NAME_PREFIX "opusfm-"
)

ecm_add_test(mainwindowtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(combinedpaneltest.cpp
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
