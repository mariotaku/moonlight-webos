import QtQuick 2.4

Item {
    property GridView grid

    // highlighted: grid.activeFocus && grid.currentItem === this

    signal clicked(var mouse)
    signal pressAndHold(var mouse)

    Keys.onLeftPressed: {
        grid.moveCurrentIndexLeft()
    }
    Keys.onRightPressed: {
        grid.moveCurrentIndexRight()
    }
    Keys.onDownPressed: {
        grid.moveCurrentIndexDown()
    }
    Keys.onUpPressed: {
        grid.moveCurrentIndexUp()
    }
    Keys.onReturnPressed: {
        clicked()
    }

    MouseArea {
        anchors.fill: parent

        onClicked: {
            parent.clicked(mouse)
        }

        onPressAndHold: {
            parent.pressAndHold(mouse)
        }
    }
}
