import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1

import WebOS.Global 1.0
import Eos.Controls 0.1

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SdlGamepadKeyNavigation 1.0
   
GridView {
    property ComputerModel computerModel : createModel()

    id: pcGrid
    focus: true
    
    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    function onStackViewActivated() {
        // Setup signals on CM
        ComputerManager.computerAddCompleted.connect(addComplete)

        // This is a bit of a hack to do this here as opposed to main.qml, but
        // we need it enabled before calling getConnectedGamepads() and PcView
        // is never destroyed, so it should be okay.
        SdlGamepadKeyNavigation.enable()

        // Highlight the first item if a gamepad is connected
        if (currentIndex == -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }
    }

    function onStackViewDeactivating() {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
    }

    Stack.onStatusChanged: {
        if (Stack.status == Stack.Active) {
            onStackViewActivated()
        } else if (Stack.status == Stack.Deactivating) {
            onStackViewDeactivating()
        }
    }

    function pairingComplete(error)
    {
        // Close the PIN dialog
        pairDialog.close()

        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = qsTr("Unable to connect to the specified PC.")

            if (detectedPortBlocking) {
                errorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Moonlight. Streaming over the Internet may not work while connected to this network.")
            }
            else {
                errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
            }

            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        // model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        width: 300; height: 320;
        grid: pcGrid

        Image {
            id: pcIcon
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/res/desktop_windows-256px.png"
            sourceSize {
                width: 200
                height: 200
            }
        }

        Image {
            // TODO: Tooltip
            id: stateIcon
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -15
            visible: !model.statusUnknown && (!model.online || !model.paired)
            source: !model.online ? "qrc:/res/baseline-warning-96px.png" : "qrc:/res/baseline-lock-96px.png"
            sourceSize {
                width: 75
                height: 75
            }
        }

        BusyIndicator {
            id: statusUnknownSpinner
            anchors.horizontalCenter: pcIcon.horizontalCenter
            anchors.verticalCenter: pcIcon.verticalCenter
            anchors.verticalCenterOffset: -15
            width: 75
            height: 75
            visible: model.statusUnknown
        }

        Text {
            color: "white"
            id: pcNameText
            text: model.name

            width: parent.width
            anchors.top: pcIcon.bottom
            anchors.bottom: parent.bottom
            font.pointSize: 36
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            elide: Text.ElideRight
        }

        
        NavigableMenu {
            id: pcContextMenu
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: qsTr("View All Apps")
                onTriggered: {
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name, "showHiddenGames": true})
                    stackView.push(appView)
                }
                visible: model.online && model.paired
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: qsTr("Wake PC")
                onTriggered: computerModel.wakeComputer(index)
                visible: !model.online && model.wakeable
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: qsTr("Test Network")
                onTriggered: {
                    computerModel.testConnectionForComputer(index)
                    testConnectionDialog.open()
                }
            }

            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: qsTr("Rename PC")
                onTriggered: {
                    renamePcDialog.pcIndex = index
                    renamePcDialog.originalName = model.name
                    renamePcDialog.open()
                }
            }
            NavigableMenuItem {
                parentMenu: pcContextMenu
                text: qsTr("Delete PC")
                onTriggered: {
                    deletePcDialog.pcIndex = index
                    // get confirmation first, actual closing is called from the dialog
                    deletePcDialog.open()
                }
            }
        }

        onClicked: {
            if (model.online) {
                if (model.paired) {
                    // go to game view
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                else {
                    if (!model.busy) {
                        var pin = computerModel.generatePinString()

                        // Kick off pairing in the background
                        computerModel.pairComputer(index, pin)

                        // Display the pairing dialog
                        pairDialog.pin = pin
                        pairDialog.open()
                    }
                    else {
                        // cannot pair while something is streaming or attempting to pair
                        errorDialog.text = qsTr("You cannot pair while a previous session is still running on the host PC. Quit any running games or reboot the host PC, then try pairing again.")
                        errorDialog.helpText = ""
                        errorDialog.open()
                    }
                }
            } else if (!model.online) {
                // Using open() here because it may be activated by keyboard
                pcContextMenu.open()
            }
        }

        onPressAndHold: {
            // popup() ensures the menu appears under the mouse cursor
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                // Qt 5.9 doesn't have popup()
                pcContextMenu.open()
            }
        }
    }
        
    MessageDialog {
        id: errorDialog

        standardButtons: Dialog.Ok | Dialog.Help

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        // helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
        property string helpText
        property string helpUrl
        property string helpTextSeparator: " "
    }

    MessageDialog {
        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        
        id: pairDialog
        standardButtons: StandardButton.Cancel
        text:qsTr("Please enter %1 on your GameStream PC. This dialog will close when pairing is completed.").arg(pin)
    }

}