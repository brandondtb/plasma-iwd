pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras

import io.radiation.plasmaiwd

PlasmaExtras.ExpandableListItem {
    id: item

    required index
    required property string path
    required property string name
    required property string type
    required property int signalQuality
    required property bool connected
    required property bool known
    required property string knownPath

    readonly property bool secured: type === "psk" || type === "wep" || type === "8021x"
    // iwd can't connect to WEP, and 802.1X needs a provisioning file we don't write.
    readonly property bool supported: type === "open" || type === "psk" || (type === "8021x" && known)
    readonly property bool needsPassphrase: type === "psk" && !known
    readonly property bool isActive: path === Iwd.connectedPath
    readonly property bool activating: isActive && (Iwd.state === "connecting" || Iwd.state === "roaming")

    // Set while iwd's agent is blocked waiting on this row's password field.
    property bool agentWaiting: false
    property bool pending: false
    // Mirrors of the expanded password field, which lives in a Loader.
    property string passphraseInput
    property bool passphraseAcceptable: false
    property string errorText

    icon: "network-wireless-" + [20, 40, 60, 80, 100][signalQuality] + (secured ? "-locked" : "")
    title: name
    subtitle: {
        if (errorText !== "")
            return errorText;
        if (activating || pending)
            return i18n("Connecting…");
        if (connected)
            return i18n("Connected");
        if (!supported)
            return type === "wep" ? i18n("WEP is not supported") : i18n("Enterprise network (not provisioned)");
        if (known)
            return i18n("Saved");
        return secured ? i18n("Secured") : i18n("Open");
    }
    subtitleColor: errorText !== "" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
    isBusy: activating || pending
    showDefaultActionButtonWhenBusy: false

    customExpandedViewContent: (needsPassphrase || agentWaiting) && !connected ? passwordComponent : null

    defaultActionButtonAction: QQC2.Action {
        enabled: {
            if (!item.supported)
                return false;
            if (item.expanded && item.customExpandedViewContent !== null) {
                return item.passphraseAcceptable;
            }
            return true;
        }
        icon.name: item.connected ? "network-disconnect-symbolic" : "network-connect-symbolic"
        text: item.connected ? i18n("Disconnect") : i18n("Connect")
        onTriggered: item.activate()
    }

    contextualActions: [
        QQC2.Action {
            enabled: item.known
            icon.name: "edit-delete-remove-symbolic"
            text: i18n("Forget")
            onTriggered: Iwd.forget(item.knownPath)
        }
    ]

    function activate(): void {
        if (connected) {
            Iwd.disconnectNetwork();
            return;
        }
        if (customExpandedViewContent !== null && !expanded) {
            expand();
            return;
        }
        const passphrase = customExpandedViewContent !== null ? passphraseInput : "";
        passphraseInput = "";
        errorText = "";
        if (agentWaiting) {
            agentWaiting = false;
            Iwd.providePassphrase(path, passphrase);
        } else {
            pending = true;
            Iwd.connectNetwork(path, passphrase);
        }
        collapse();
    }

    onItemCollapsed: {
        if (agentWaiting) {
            agentWaiting = false;
            Iwd.cancelPassphrase(path);
        }
    }

    onActivatingChanged: if (activating)
        pending = false
    onConnectedChanged: if (connected) {
        pending = false;
        errorText = "";
    }

    Connections {
        target: Iwd
        function onPassphraseRequested(networkPath: string): void {
            if (networkPath === item.path) {
                item.agentWaiting = true;
                item.pending = false;
                item.expand();
            }
        }
        function onPassphraseRequestCanceled(networkPath: string): void {
            if (networkPath === item.path) {
                item.agentWaiting = false;
                item.collapse();
            }
        }
        function onConnectFailed(networkPath: string, message: string): void {
            if (networkPath === item.path) {
                item.pending = false;
                item.errorText = message;
            }
        }
    }

    Component {
        id: passwordComponent

        ColumnLayout {
            PlasmaExtras.PasswordField {
                id: field
                Layout.fillWidth: true
                placeholderText: i18n("Passphrase")
                validator: RegularExpressionValidator {
                    regularExpression: /^(?:.{8,63}|[0-9a-fA-F]{64})$/
                }
                Component.onCompleted: forceActiveFocus()
                onTextChanged: {
                    item.passphraseInput = text;
                    item.passphraseAcceptable = acceptableInput;
                }
                onAccepted: if (acceptableInput)
                    item.activate()
                Keys.onEscapePressed: item.collapse()
            }
        }
    }
}
