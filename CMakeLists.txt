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

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Concurrent Core Gui Widgets DBus Test Multimedia MultimediaWidgets Network Xml)
find_package(LibArchive REQUIRED)
find_package(KF6Baloo ${KF_MIN_VERSION} QUIET)

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
    GuiAddons
    I18n
    IconThemes
    ItemViews
    KIO
    Parts
    Solid
    SyntaxHighlighting
    TextEditor
    WidgetsAddons
    WindowSystem
    XmlGui
)

add_subdirectory(src)

ki18n_install(po)

add_custom_target(extract-messages
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract-messages.sh ${CMAKE_CURRENT_BINARY_DIR}/opus-file-manager.pot
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES src/*.cpp src/*.h tests/*.cpp tests/*.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
