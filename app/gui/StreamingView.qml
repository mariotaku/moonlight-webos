import QtQuick 2.4
import Eos.Items 0.1
import Moonlight.Streaming 0.1

Rectangle {
    color: "black"

    StreamingController {
        id: streamingController
    }

    MouseArea {
        anchors.fill: parent
        onClicked: { streamingController.testPlay() }
    }
}