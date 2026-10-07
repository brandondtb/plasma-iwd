pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

import io.radiation.plasmaiwd

PlasmoidItem {
    id: root

    readonly property bool inPanel: [PlasmaCore.Types.TopEdge, PlasmaCore.Types.RightEdge, PlasmaCore.Types.BottomEdge, PlasmaCore.Types.LeftEdge].includes(Plasmoid.location)
    readonly property bool connecting: Iwd.state === "connecting" || Iwd.state === "roaming"

    readonly property string statusIcon: {
        switch (Iwd.state) {
        case "unavailable":
        case "off":
            return "network-wireless-off";
        case "connecting":
        case "roaming":
            return "network-wireless-acquiring";
        case "connected":
            return "network-wireless-connected-" + ["00", "25", "50", "75", "100"][Iwd.signalQuality];
        default:
            return "network-wireless-disconnected";
        }
    }

    readonly property string statusText: {
        switch (Iwd.state) {
        case "unavailable":
            return Iwd.available ? i18n("No wireless device") : i18n("iwd is not running");
        case "off":
            return i18n("Wi-Fi is off");
        case "connecting":
            return i18n("Connecting…");
        case "roaming":
            return i18n("Roaming on %1", Iwd.connectedSsid);
        case "connected":
            return i18n("Connected to %1", Iwd.connectedSsid);
        case "disconnecting":
            return i18n("Disconnecting…");
        default:
            return i18n("Not connected");
        }
    }

    Plasmoid.icon: inPanel ? statusIcon + "-symbolic" : statusIcon
    Plasmoid.busy: connecting
    // Passive would tuck the icon into the tray's overflow; stay visible like plasma-nm.
    Plasmoid.status: Iwd.state === "unavailable" ? PlasmaCore.Types.HiddenStatus : PlasmaCore.Types.ActiveStatus

    toolTipMainText: i18n("Wi-Fi")
    toolTipSubText: statusText

    switchWidth: Kirigami.Units.gridUnit * 10
    switchHeight: Kirigami.Units.gridUnit * 10

    onExpandedChanged: {
        if (root.expanded) {
            Iwd.scan();
        }
    }

    fullRepresentation: FullRepresentation {
        Layout.minimumWidth: Kirigami.Units.iconSizes.medium * 10
        Layout.minimumHeight: Kirigami.Units.gridUnit * 20
        statusText: root.statusText
    }

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("Enable Wi-Fi")
            icon.name: "network-wireless-on"
            priority: PlasmaCore.Action.LowPriority
            checkable: true
            checked: Iwd.powered
            visible: Iwd.hasDevice
            onTriggered: Iwd.setPowered(!Iwd.powered)
        }
    ]
}
