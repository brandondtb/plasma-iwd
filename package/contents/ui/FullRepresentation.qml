pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras

import io.radiation.plasmaiwd

PlasmaExtras.Representation {
    id: full

    required property string statusText
    property string errorText

    collapseMarginsHint: true

    Connections {
        target: Iwd
        function onErrorOccurred(message: string): void {
            full.errorText = message;
        }
        function onConnectFailed(networkPath: string, message: string): void {
            // Hidden-network failures have no row to show them on.
            if (networkPath === "") {
                full.errorText = message;
            }
        }
    }

    header: PlasmaExtras.PlasmoidHeading {
        contentItem: ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true

                PlasmaComponents3.Switch {
                    text: i18n("Wi-Fi")
                    checked: Iwd.powered
                    enabled: Iwd.hasDevice
                    onToggled: Iwd.setPowered(checked)
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: full.statusText
                    elide: Text.ElideRight
                    opacity: 0.7
                }

                PlasmaComponents3.ToolButton {
                    icon.name: "view-refresh-symbolic"
                    enabled: Iwd.powered && !Iwd.scanning
                    onClicked: Iwd.scan()
                    PlasmaComponents3.ToolTip.text: i18n("Rescan")
                    PlasmaComponents3.ToolTip.visible: hovered
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Rescan")
                }

                PlasmaComponents3.ToolButton {
                    icon.name: "list-add-symbolic"
                    enabled: Iwd.powered
                    checkable: true
                    checked: hiddenSheet.visible
                    onToggled: hiddenSheet.visible = checked
                    PlasmaComponents3.ToolTip.text: i18n("Connect to hidden network…")
                    PlasmaComponents3.ToolTip.visible: hovered
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Connect to hidden network…")
                }
            }

            PlasmaComponents3.TabBar {
                id: tabs
                Layout.fillWidth: true
                PlasmaComponents3.TabButton {
                    text: i18n("Available")
                }
                PlasmaComponents3.TabButton {
                    text: i18n("Known")
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            type: Kirigami.MessageType.Error
            text: full.errorText
            visible: text !== ""
            showCloseButton: true
            onVisibleChanged: if (!visible)
                full.errorText = ""
        }

        HiddenNetworkSheet {
            id: hiddenSheet
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            visible: false
            onDone: visible = false
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            PlasmaComponents3.ScrollView {
                // Derive width from the view, not the list, to avoid a scrollbar/width binding loop.
                contentWidth: availableWidth - availableView.leftMargin - availableView.rightMargin
                PlasmaComponents3.ScrollBar.horizontal.policy: PlasmaComponents3.ScrollBar.AlwaysOff

                contentItem: ListView {
                    id: availableView
                    model: Iwd.networks
                    clip: true
                    spacing: Kirigami.Units.smallSpacing
                    topMargin: Kirigami.Units.smallSpacing
                    bottomMargin: Kirigami.Units.smallSpacing
                    leftMargin: Kirigami.Units.smallSpacing
                    rightMargin: Kirigami.Units.smallSpacing
                    highlight: PlasmaExtras.Highlight {}
                    highlightMoveDuration: 0
                    highlightResizeDuration: 0
                    currentIndex: -1
                    delegate: NetworkDelegate {
                        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                    }

                    PlasmaExtras.PlaceholderMessage {
                        id: availableEmpty
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 2
                        visible: availableView.count === 0
                        iconName: Iwd.powered ? "network-wireless-disconnected" : "network-wireless-off"
                        text: {
                            if (!Iwd.available)
                                return i18n("iwd is not running");
                            if (!Iwd.hasDevice)
                                return i18n("No wireless device found");
                            if (!Iwd.powered)
                                return i18n("Wi-Fi is off");
                            return Iwd.scanning ? i18n("Scanning…") : i18n("No networks found");
                        }
                    }
                }
            }

            PlasmaComponents3.ScrollView {
                contentWidth: availableWidth - knownView.leftMargin - knownView.rightMargin
                PlasmaComponents3.ScrollBar.horizontal.policy: PlasmaComponents3.ScrollBar.AlwaysOff

                contentItem: ListView {
                    id: knownView
                    model: Iwd.knownNetworks
                    clip: true
                    spacing: Kirigami.Units.smallSpacing
                    topMargin: Kirigami.Units.smallSpacing
                    bottomMargin: Kirigami.Units.smallSpacing
                    leftMargin: Kirigami.Units.smallSpacing
                    rightMargin: Kirigami.Units.smallSpacing
                    delegate: KnownNetworkDelegate {
                        width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                    }

                    PlasmaExtras.PlaceholderMessage {
                        id: knownEmpty
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 2
                        visible: knownView.count === 0
                        iconName: "network-wireless"
                        text: i18n("No saved networks")
                    }
                }
            }
        }
    }
}
