import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.cherrygi

Rectangle {
    id: root
    color: Kirigami.Theme.backgroundColor
    border.color: CherryStyle.borderColor
    border.width: 1

    StackLayout {
        anchors.fill: parent
        currentIndex: {
            if (appController.activeTab === "history") return 1;
            if (appController.selectedStashId !== "") return 2;
            return 0;
        }

        // Changes Mode -> Diff Viewer
        DiffViewer {
            id: mainDiffViewer
            filePath: appController.selectedFilePath
            isHistorical: false
        }

        // History Mode -> Commit Inspector
        CommitInspector {
            id: mainCommitInspector
        }

        // Stash Mode -> Stash Inspector
        StashInspector {
            id: mainStashInspector
        }
    }
}
