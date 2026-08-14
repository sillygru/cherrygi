cmake_minimum_required(VERSION 3.20)

project(cherrygi VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Disable static QML plugin imports (Kirigami plugins are dynamic shared libraries)
set(QT_NO_IMPORT_QML_PLUGINS ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(ZLIB REQUIRED)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
)

# Qt QML policies
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

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
    src/resources.qrc
    src/core/Types.h
    src/core/IGitService.h
    src/core/GitRepositoryReader.h
    src/core/GitRepositoryReader.cpp
    src/core/GitCliService.h
    src/core/GitCliService.cpp
    src/core/MockGitService.h
    src/core/MockGitService.cpp
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/GitImageProvider.h
    src/core/GitImageProvider.cpp
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

# Mark CherryStyle.qml as a QML singleton type
set_source_files_properties(src/qml/style/CherryStyle.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(cherrygi
    URI org.kde.cherrygi
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    NO_PLUGIN
    QML_FILES
        src/qml/Main.qml
        src/qml/style/CherryStyle.qml
        src/qml/components/HeaderBar.qml
        src/qml/components/BackendSelectionDialog.qml
        src/qml/components/AppSettingsDialog.qml
        src/qml/components/PublishRepositoryDialog.qml
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
        src/qml/components/ImageDiffViewer.qml
        src/qml/components/CommitInspector.qml
        src/qml/components/StashInspector.qml
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
    ZLIB::ZLIB
    KF6::Kirigami
    KF6::CoreAddons
    KF6::I18n
    KF6::IconThemes
    KF6::ColorScheme
    KF6::ConfigCore
)

install(TARGETS cherrygi ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

# Install the desktop entry and the source SVG in the standard hicolor theme.
# Keeping the icon as SVG lets Plasma and other launchers render it sharply at
# any taskbar, launcher, or high-DPI size.
install(FILES org.kde.cherrygi.desktop
    DESTINATION ${KDE_INSTALL_APPDIR}
)

install(FILES src/icons/cherrygi.svg
    DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps
)
