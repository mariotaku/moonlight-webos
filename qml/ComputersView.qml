import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import Eos.Controls 0.1

ColumnLayout {
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
                text: "Add device"
            }
            Button {
                text: "Help"
            }
            Button {
                text: "Settings"
            }
        }
    }

    GridView {
        id: grid
        Layout.fillWidth: true
        Layout.fillHeight: true
        cellWidth: 80; cellHeight: 80

        model: ContactModel {}
        delegate: contactDelegate
        highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
        focus: true
    }
}