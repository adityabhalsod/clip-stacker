import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: root

    // Keep the popup frameless, transient, and above normal windows like the Windows 11 clipboard panel.
    width: 460
    height: 520
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    title: "clip-stacker"

    // Fade and scale the popup for a lighter modern feel without heavy animation overhead.
    opacity: visible ? 1.0 : 0.0
    property real panelScale: visible ? 1.0 : 0.96

    // Reset the popup state each time it is shown so search always starts from the full history list.
    function prepareForShow() {
        searchField.text = ""
        historyManager.setSearchQuery("")
        if (historyView.count > 0) {
            historyView.currentIndex = 0
        }
        searchField.forceActiveFocus()
    }

    // Close the popup when it loses focus so it behaves like a transient system panel.
    onActiveChanged: {
        if (!active) {
            popupController.hidePopup()
        }
    }

    Rectangle {
        id: shadowLayer

        // Paint a soft shadow backdrop that gives the popup a floating system-panel feel.
        anchors.fill: panel
        anchors.margins: -10
        radius: 30
        color: "#160f10"
        opacity: 0.4
    }

    Rectangle {
        id: panel

        // Render the main popup surface with a warm layered gradient instead of a flat default panel.
        anchors.fill: parent
        radius: 24
        scale: root.panelScale
        border.width: 1
        border.color: "#2f3c4f"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#f7f1ea" }
            GradientStop { position: 0.55; color: "#efe2d4" }
            GradientStop { position: 1.0; color: "#d8d6ea" }
        }

        Behavior on scale {
            NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            id: contentColumn

            anchors.fill: parent
            anchors.margins: 18
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        // Give the panel a strong, compact heading that reads quickly.
                        text: "Clipboard"
                        color: "#182130"
                        font.pixelSize: 28
                        font.bold: true
                    }

                    Label {
                        // Provide a short status line to reinforce keyboard-first behavior.
                        text: "Search, browse, and paste from history"
                        color: "#4d5d72"
                        font.pixelSize: 13
                    }
                }

                Rectangle {
                    // Show a small badge that hints at the X11 primary shortcut.
                    radius: 14
                    color: "#182130"
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: shortcutLabel.implicitWidth + 18
                    implicitHeight: shortcutLabel.implicitHeight + 10

                    Label {
                        id: shortcutLabel
                        anchors.centerIn: parent
                        text: "Super+V"
                        color: "#f7f1ea"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
            }

            TextField {
                id: searchField

                // Filter the history in real time as the user types.
                Layout.fillWidth: true
                placeholderText: "Search clipboard history"
                selectByMouse: true
                font.pixelSize: 14
                color: "#182130"
                onTextChanged: historyManager.setSearchQuery(text)
                Keys.onDownPressed: {
                    historyView.forceActiveFocus()
                    if (historyView.count > 0) {
                        historyView.currentIndex = Math.max(0, historyView.currentIndex)
                    }
                }
                background: Rectangle {
                    radius: 16
                    border.width: 1
                    border.color: searchField.activeFocus ? "#182130" : "#8ea0b8"
                    color: "#fdf9f4"
                }
            }

            Rectangle {
                // Separate the search bar from the history list with a subtle divider panel.
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 20
                color: "#fffaf5"
                border.width: 1
                border.color: "#d4c6b8"

                ListView {
                    id: historyView

                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    spacing: 8
                    model: historyModel
                    currentIndex: count > 0 ? 0 : -1
                    keyNavigationWraps: true
                    delegate: HistoryDelegate {}
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                    }

                    // Handle keyboard navigation and activation without forcing the user onto the mouse.
                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Down) {
                            currentIndex = Math.min(count - 1, currentIndex + 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up) {
                            currentIndex = Math.max(0, currentIndex - 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            if (currentIndex >= 0 && currentItem) {
                                historyManager.activateEntry(currentItem.entryId)
                                popupController.hidePopup()
                            }
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape) {
                            popupController.hidePopup()
                            event.accepted = true
                        }
                    }
                }

                Label {
                    // Show an empty-state message when there is no captured history yet.
                    anchors.centerIn: parent
                    visible: historyView.count === 0
                    text: "Copy something to build your history"
                    color: "#67768b"
                    font.pixelSize: 15
                }
            }

            Label {
                // Reserve a footer for accessibility and Wayland fallback guidance.
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Enter pastes the selected item. Esc closes the popup. If your compositor blocks global hotkeys, bind clip-stacker --toggle-popup in system shortcuts."
                color: "#536275"
                font.pixelSize: 12
            }
        }
    }
}