add_library(opusfm_core STATIC
    executables.cpp
    executables.h
    navigationhistory.cpp
    navigationhistory.h
    session.cpp
    session.h
    startup.cpp
    startup.h
    symlinks.cpp
    symlinks.h
    tabmodel.cpp
    tabmodel.h
)

kconfig_add_kcfg_files(opusfm_core GENERATE_MOC settings.kcfgc)

target_include_directories(opusfm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(opusfm_core
    PUBLIC
        Qt6::Core
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::KIOGui
)
