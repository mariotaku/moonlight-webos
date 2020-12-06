import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

import Eos.Controls 0.1

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0

ColumnLayout {
    property ComputerModel computerModel : createModel()

    focus: true

    Header {
        id: pageHeader
        Layout.fillWidth: true
        headerText: "Moonlight"

        RowLayout {   
            anchors {
                right: parent.right
                bottom: parent.bottom
                rightMargin: 10
                bottomMargin: 10
            }                 

            Button {
                id: addDevice
                text: "Add device"
                KeyNavigation.right: help
            }
            Button {
                id: help
                text: "Help"
                KeyNavigation.left: addDevice
                KeyNavigation.right: settings
            }
            Button {
                id: settings
                text: "Settings"
                KeyNavigation.left: help
            }
        }
    }

    GridView {
        id: pcGrid
        Layout.fillWidth: true
        Layout.fillHeight: true
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

            BodyText {
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
            
            
            MouseArea {
                anchors.fill: parent
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
            }
        }
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
        focus: true
    }


    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        // model.pairingCompleted.connect(pairingComplete)
        // model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }
}