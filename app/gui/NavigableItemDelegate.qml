import QtQuick 2.4

Item {
    property GridView grid

    // highlighted: grid.activeFocus && grid.currentItem === this

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
}
