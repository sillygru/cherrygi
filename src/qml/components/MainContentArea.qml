import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../style"

Rectangle {
    id: root
    color: Kirigami.Theme.backgroundColor
    border.color: CherryStyle.borderColor
    border.width: 1

    StackLayout {
        anchors.fill: parent
        currentIndex: appController.activeTab === "changes" ? 0 : 1

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
    }
}
