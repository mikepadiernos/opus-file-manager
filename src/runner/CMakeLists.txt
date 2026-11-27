add_library(opusfm_runner STATIC
    opusrunner.cpp
    opusrunner.h
)
target_include_directories(opusfm_runner PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(opusfm_runner
    PUBLIC
        opusfm_core
        Qt6::DBus
)

add_executable(opus-file-manager-krunner main.cpp)
target_link_libraries(opus-file-manager-krunner PRIVATE opusfm_runner)

# KRunner starts the helper through D-Bus activation when it first searches.
configure_file(org.kde.opusfilemanager.krunner.service.in ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.krunner.service @ONLY)

install(TARGETS opus-file-manager-krunner ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.opusfilemanager.krunner.service DESTINATION ${KDE_INSTALL_DBUSSERVICEDIR})
install(FILES org.kde.opusfilemanager.krunner.desktop DESTINATION ${KDE_INSTALL_DATAROOTDIR}/krunner/dbusplugins)
