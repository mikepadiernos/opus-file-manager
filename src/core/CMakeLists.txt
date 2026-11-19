add_library(opusfm_core STATIC
    navigationhistory.cpp
    navigationhistory.h
    session.cpp
    session.h
    symbolicicon.cpp
    symbolicicon.h
    tabmodel.cpp
    tabmodel.h
)

target_include_directories(opusfm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(opusfm_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        KF6::ConfigCore
)
