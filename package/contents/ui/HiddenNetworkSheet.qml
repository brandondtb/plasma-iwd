import QtQuick
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras

import io.radiation.plasmaiwd

ColumnLayout {
    id: sheet

    signal done

    spacing: Kirigami.Units.smallSpacing

    onVisibleChanged: {
        if (visible) {
            ssid.text = "";
            passphrase.text = "";
            ssid.forceActiveFocus();
        }
    }

    function submit(): void {
        if (!ssid.acceptableInput || !(passphrase.text === "" || passphrase.acceptableInput)) {
            return;
        }
        Iwd.connectHidden(ssid.text, passphrase.text);
        done();
    }

    PlasmaComponents3.Label {
        text: i18n("Connect to a hidden network")
        font.bold: true
    }

    PlasmaComponents3.TextField {
        id: ssid
        Layout.fillWidth: true
        placeholderText: i18n("Network name (SSID)")
        validator: RegularExpressionValidator {
            regularExpression: /^.{1,32}$/
        }
        onAccepted: passphrase.forceActiveFocus()
        Keys.onEscapePressed: sheet.done()
    }

    PlasmaExtras.PasswordField {
        id: passphrase
        Layout.fillWidth: true
        placeholderText: i18n("Passphrase (leave empty for open networks)")
        validator: RegularExpressionValidator {
            regularExpression: /^(?:.{8,63}|[0-9a-fA-F]{64})$/
        }
        onAccepted: sheet.submit()
        Keys.onEscapePressed: sheet.done()
    }

    RowLayout {
        Layout.alignment: Qt.AlignRight

        PlasmaComponents3.Button {
            text: i18n("Cancel")
            icon.name: "dialog-cancel-symbolic"
            onClicked: sheet.done()
        }
        PlasmaComponents3.Button {
            text: i18n("Connect")
            icon.name: "network-connect-symbolic"
            enabled: ssid.acceptableInput && (passphrase.text === "" || passphrase.acceptableInput)
            onClicked: sheet.submit()
        }
    }
}
