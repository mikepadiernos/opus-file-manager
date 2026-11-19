add_library(opusfm_core STATIC
    archives.cpp
    archives.h
    archivewriter.cpp
    archivewriter.h
    favoritesmodel.cpp
    favoritesmodel.h
    flatview.cpp
    flatview.h
    foldermetadata.cpp
    foldermetadata.h
    ignorelist.cpp
    ignorelist.h
    labelcolor.cpp
    labelcolor.h
    density.cpp
    density.h
    executables.cpp
    executables.h
    navigationhistory.cpp
    namefilter.cpp
    namefilter.h
    navigationhistory.h
    session.cpp
    servicemenus.cpp
    servicemenus.h
    session.h
    tabcolors.cpp
    tabcolors.h
    startup.cpp
    startup.h
    symlinks.cpp
    symlinks.h
    symbolicicon.cpp
    symbolicicon.h
    tabmodel.cpp
    tabmodel.h
)

kconfig_add_kcfg_files(opusfm_core GENERATE_MOC settings.kcfgc)

target_include_directories(opusfm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(opusfm_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        KF6::Archive
        KF6::ConfigCore
        KF6::KIOCore
        KF6::ConfigGui
        KF6::KIOGui
)
