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
        id: computersList
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: computerModel
        delegate: RowLayout {
            width: parent.width / 6
            height: width
            BodyText {
                Layout.fillWidth: true
                text: name
                color: "white"
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