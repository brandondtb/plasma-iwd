pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2

import org.kde.plasma.extras as PlasmaExtras

import io.radiation.plasmaiwd

PlasmaExtras.ExpandableListItem {
    id: item

    required index
    required property string path
    required property string name
    required property string type
    required property bool hidden
    required property bool autoConnect
    required property var lastConnected

    icon: type === "open" ? "network-wireless-100" : "network-wireless-100-locked"
    title: name
    subtitle: {
        const parts = [];
        if (hidden)
            parts.push(i18n("Hidden"));
        if (!autoConnect)
            parts.push(i18n("Manual connect"));
        parts.push(lastConnected && !isNaN(lastConnected) ? i18n("Last connected %1", Qt.formatDateTime(lastConnected, Qt.locale().dateTimeFormat(Locale.ShortFormat))) : i18n("Never connected"));
        return parts.join(" · ");
    }

    defaultActionButtonAction: QQC2.Action {
        icon.name: "edit-delete-remove-symbolic"
        text: i18n("Forget")
        onTriggered: Iwd.forget(item.path)
    }

    contextualActions: [
        QQC2.Action {
            checkable: true
            checked: item.autoConnect
            icon.name: "network-connect-symbolic"
            text: i18n("Connect automatically")
            onTriggered: Iwd.setAutoConnect(item.path, checked)
        }
    ]
}
