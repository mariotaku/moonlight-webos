import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1
import Eos.Controls 0.1

ColumnLayout {
    anchors.fill: parent

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

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true

        Button {
            id: mainText
            anchors.centerIn: parent
            text: "Launch GameStream"
            onClicked: navigateTo("qrc:/StreamingView.qml", qsTr("StreamingView"))
        }
    }
}