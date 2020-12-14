import QtQuick 2.6
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

import WebOS.Global 1.0
import Eos.Controls 0.1
import Eos.Style 0.1

import StreamingPreferences 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import SystemProperties 1.0

Flickable {
    id: settingsPage
    objectName: qsTr("Settings")

    boundsBehavior: Flickable.OvershootBounds

    function onStackViewActivated() {
        // This enables Tab and BackTab based navigation rather than arrow keys.
        // It is required to shift focus between controls on the settings page.
        SdlGamepadKeyNavigation.setUiNavMode(true)

        // Highlight the first item if a gamepad is connected
        if (SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            resolutionComboBox.forceActiveFocus(Qt.TabFocus)
        }
    }

    function onStackViewDeactivating() {
        SdlGamepadKeyNavigation.setUiNavMode(false)

        // Save the prefs so the Session can observe the changes
        StreamingPreferences.save()
    }

    Stack.onStatusChanged: {
        if (Stack.status == Stack.Active) {
            onStackViewActivated()
        } else if (Stack.status == Stack.Deactivating) {
            onStackViewDeactivating()
        }
    }

    Row {
        Column {
            padding: 10
            id: settingsColumn1
            width: settingsPage.width / 2
            spacing: 15

            GroupBox {
                id: basicSettingsGroupBox
                width: (parent.width - (parent.leftPadding + parent.rightPadding))
                title: qsTr("Basic Settings")
                // font.pointSize: 12

                Column {
                    anchors.fill: parent
                    spacing: 5

                    BodyText {
                        width: parent.width
                        id: resFPStitle
                        text: qsTr("Resolution and FPS")
                        wrapMode: Text.Wrap
                    }

                    BodyText {
                        width: parent.width
                        id: resFPSdesc
                        text: qsTr("Setting values too high for your PC or network connection may cause lag, stuttering, or errors.")
                        wrapMode: Text.Wrap
                    }

                    Row {
                        spacing: 5
                        width: parent.width

                        ComboBox {
                            id: resolutionComboBox
                            // maximumWidth: parent.width / 2
                            //textRole: "text"
                            model: ["720p", "1080p", "1440p", "4K"]
                        }

                        ComboBox {
                            id: fpsComboBox

                            model: ["30 FPS", "60 FPS"]
                        }
                    }

                }
            }
        }
    }
}
