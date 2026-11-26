add_library(opusfm_portal STATIC
    chooserdialog.cpp
    chooserdialog.h
    filechooserportal.cpp
    filechooserportal.h
)
target_include_directories(opusfm_portal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(opusfm_portal
    PUBLIC
        opusfm_app
        Qt6::DBus
        KF6::KIOFileWidgets
        KF6::WidgetsAddons
        KF6::WindowSystem
)

# D-Bus starts the backend only after the user opts in, which writes a
# per-user activation file pointing at this executable.
add_executable(opus-file-manager-portal main.cpp)
target_link_libraries(opus-file-manager-portal PRIVATE opusfm_portal)

install(TARGETS opus-file-manager-portal ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
