include(ECMAddTests)

ecm_add_tests(
    executablestest.cpp
    navigationhistorytest.cpp
    sessiontest.cpp
    startuptest.cpp
    symlinkstest.cpp
    tabmodeltest.cpp
    LINK_LIBRARIES opusfm_core Qt6::Test KF6::ConfigCore
    NAME_PREFIX "opusfm-"
)

ecm_add_test(mainwindowtest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)

ecm_add_test(settingstest.cpp
    LINK_LIBRARIES opusfm_app Qt6::Test
    NAME_PREFIX "opusfm-"
    GUI
)
