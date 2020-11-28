import QtQuick 2.4
import QtMultimedia 5.6
import Moonlight.Streaming 0.1

Rectangle {
    color: "black"

    VideoOutput {
        id: videoOutput
        source: streamingController
        anchors.fill: parent
    }

    StreamingController {
        id: streamingController
    }
}