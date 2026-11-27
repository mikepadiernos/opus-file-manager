cmake_minimum_required(VERSION 3.25)

project(opus-file-manager VERSION 0.2.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.8)
set(KF_MIN_VERSION 6.20)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(ECMAddTests)
include(ECMSetupVersion)
include(FeatureSummary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX OPUSFM VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/opusfm_version.h)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Concurrent Core Gui Widgets DBus Test Multimedia MultimediaWidgets Network Qml Xml)
find_package(LibArchive REQUIRED)
find_package(KF6Baloo ${KF_MIN_VERSION} QUIET)
find_package(KF6Prison ${KF_MIN_VERSION} QUIET)
set_package_properties(KF6Prison PROPERTIES TYPE OPTIONAL PURPOSE "QR codes for links to files shared on the local network")
find_package(AccountsQt6 QUIET)
set_package_properties(AccountsQt6 PROPERTIES TYPE OPTIONAL PURPOSE "Browsing the files of accounts configured in KDE Online Accounts")
find_package(PlasmaActivities 6.0 REQUIRED)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MPV QUIET IMPORTED_TARGET mpv)
endif()
find_package(Qt6 ${QT_MIN_VERSION} QUIET COMPONENTS OpenGL OpenGLWidgets)
if(MPV_FOUND AND Qt6OpenGLWidgets_FOUND)
    set(OPUSFM_HAVE_MPV ON)
else()
    set(OPUSFM_HAVE_MPV OFF)
endif()
add_feature_info(MpvPlayback OPUSFM_HAVE_MPV "Embedded audio and video playback through libmpv in the preview panel and gallery; needs libmpv and Qt OpenGL Widgets")

include(KDEGitCommitHooks)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    Archive
    Bookmarks
    ColorScheme
    Config
    ConfigWidgets
    CoreAddons
    Crash
    DBusAddons
    FileMetaData
    GlobalAccel
    GuiAddons
    I18n
    IconThemes
    ItemViews
    JobWidgets
    KIO
    NewStuff
    Package
    Parts
    Solid
    StatusNotifierItem
    SyntaxHighlighting
    TextEditor
    WidgetsAddons
    WindowSystem
    XmlGui
)

add_subdirectory(src)

option(OPUSFM_RUST_PROTOTYPE "Build and test the Rust prototype of the advanced search query parser" OFF)
add_feature_info(RustPrototype OPUSFM_RUST_PROTOTYPE "Rust prototype of the advanced search query parser, compared with the C++ parser in tests")
if(OPUSFM_RUST_PROTOTYPE)
    find_package(Corrosion 0.5 REQUIRED)
    add_subdirectory(rust/searchquery)
endif()

ki18n_install(po)

add_custom_target(extract-messages
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract-messages.sh ${CMAKE_CURRENT_BINARY_DIR}/opus-file-manager.pot
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES src/*.cpp src/*.h tests/*.cpp tests/*.h rust/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
