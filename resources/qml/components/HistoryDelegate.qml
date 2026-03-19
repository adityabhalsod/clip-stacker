import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: delegateRoot

    // Declare the model-backed properties accessed by the delegate and the parent list view.
    required property int entryId
    required property string previewText
    required property string contentType
    required property string sourceApplication
    required property string createdAt
    required property bool pinned
    required property bool favorite
    required property bool sensitive

    // Match the delegate height to the content while keeping a comfortable hit target.
    width: ListView.view ? ListView.view.width : 400
    height: 88

    Rectangle {
        id: card

        // Draw each history item as a tactile card with a clear active state.
        anchors.fill: parent
        radius: 18
        color: ListView.isCurrentItem ? "#182130" : "#f6ebe0"
        border.width: 1
        border.color: ListView.isCurrentItem ? "#182130" : "#dacbbb"

        Behavior on color {
            ColorAnimation { duration: 110 }
        }

        MouseArea {
            // Support mouse activation while keeping the current row in sync.
            anchors.fill: parent
            hoverEnabled: true
            onEntered: ListView.view.currentIndex = index
            onClicked: {
                historyManager.activateEntry(entryId)
                popupController.hidePopup()
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            Rectangle {
                // Show a compact type badge for quick scanning across mixed clipboard payloads.
                radius: 12
                color: ListView.isCurrentItem ? "#f0d7c2" : "#182130"
                Layout.alignment: Qt.AlignTop
                implicitWidth: 62
                implicitHeight: 28

                Label {
                    anchors.centerIn: parent
                    text: contentType.toUpperCase()
                    color: ListView.isCurrentItem ? "#182130" : "#f8f1e9"
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    // Present the preview as the main content summary of the clipboard item.
                    Layout.fillWidth: true
                    text: previewText
                    color: ListView.isCurrentItem ? "#f8f1e9" : "#152030"
                    elide: Text.ElideRight
                    font.pixelSize: 15
                    font.bold: true
                }

                Label {
                    // Surface metadata without overwhelming the main preview line.
                    Layout.fillWidth: true
                    text: [sourceApplication || "Unknown app", createdAt, sensitive ? "Sensitive" : ""].filter(Boolean).join("  •  ")
                    color: ListView.isCurrentItem ? "#d4d9e1" : "#607087"
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignTop
                spacing: 6

                Button {
                    // Let users pin frequently reused items so automatic pruning does not remove them.
                    text: pinned ? "Pinned" : "Pin"
                    onClicked: historyManager.togglePinned(entryId)
                }

                Button {
                    // Let users star items for lightweight visual prioritization.
                    text: favorite ? "Starred" : "Star"
                    onClicked: historyManager.toggleFavorite(entryId)
                }
            }
        }
    }
}