cmake_minimum_required(VERSION 3.20)

project(cherrygi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
)

find_package(KF6 REQUIRED COMPONENTS
    Kirigami
    CoreAddons
    I18n
    IconThemes
    ColorScheme
    Config
)

set(CHERRYGI_SOURCES
    src/main.cpp
    src/core/Types.h
    src/core/IGitService.h
    src/core/MockGitService.h
    src/core/MockGitService.cpp
    src/core/AppController.h
    src/core/AppController.cpp
    src/models/RepositoryListModel.h
    src/models/RepositoryListModel.cpp
    src/models/BranchListModel.h
    src/models/BranchListModel.cpp
    src/models/ChangedFilesModel.h
    src/models/ChangedFilesModel.cpp
    src/models/CommitHistoryModel.h
    src/models/CommitHistoryModel.cpp
    src/models/DiffModel.h
    src/models/DiffModel.cpp
    src/models/StashModel.h
    src/models/StashModel.cpp
)

qt_add_executable(cherrygi ${CHERRYGI_SOURCES})

qt_add_qml_module(cherrygi
    URI org.kde.cherrygi
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        src/qml/Main.qml
        src/qml/style/CherryStyle.qml
        src/qml/components/HeaderBar.qml
        src/qml/components/RepoDropdown.qml
        src/qml/components/BranchDropdown.qml
        src/qml/components/RemoteDropdown.qml
        src/qml/components/SidebarPanel.qml
        src/qml/components/ChangesTab.qml
        src/qml/components/HistoryTab.qml
        src/qml/components/CommitBox.qml
        src/qml/components/MainContentArea.qml
        src/qml/components/DiffHeader.qml
        src/qml/components/DiffViewer.qml
        src/qml/components/CommitInspector.qml
        src/qml/components/UndoToast.qml
)

target_include_directories(cherrygi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
)

target_link_libraries(cherrygi PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::Kirigami
    KF6::CoreAddons
    KF6::I18n
    KF6::IconThemes
    KF6::ColorScheme
    KF6::ConfigCore
)

install(TARGETS cherrygi ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
