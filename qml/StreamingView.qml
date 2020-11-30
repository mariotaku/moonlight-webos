import QtQuick 2.4
import Eos.Items 0.1
import Moonlight.Streaming 0.1

Rectangle {
    color: "black"

    PunchThrough {
        id: videoOutput
        anchors.fill: parent
    }

    StreamingController {
        id: streamingController
    }
}