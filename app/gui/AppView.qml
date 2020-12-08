import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1

import AppModel 1.0
import ComputerManager 1.0

GridView {
    property int computerIndex
    property AppModel appModel : createModel()
    property bool activated
    property bool showHiddenGames
    property bool showGames

    id: appGrid
    focus: true
    cellWidth: 230; cellHeight: 297;

    function computerLost()
    {
        // Go back to the PC view on PC loss
        stackView.pop()
    }

    Component.onCompleted: {
        // Don't show any highlighted item until interacting with them.
        // We do this here instead of onActivated to avoid losing the user's
        // selection when backing out of a different page of the app.
        currentIndex = -1
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex, showHiddenGames)
        return model
    }

    model: appModel

    delegate: NavigableItemDelegate {
        width: 220; height: 287;
        grid: appGrid

        // Dim the app if it's hidden
        opacity: model.hidden ? 0.4 : 1.0

        Image {
            property bool isPlaceholder: false

            id: appIcon
            anchors.horizontalCenter: parent.horizontalCenter
            y: 10
            source: model.boxart

            onSourceSizeChanged: {
                if ((sourceSize.width == 130 && sourceSize.height == 180) || // GFE 2.0 placeholder image
                    (sourceSize.width == 628 && sourceSize.height == 888) || // GFE 3.0 placeholder image
                    (sourceSize.width == 200 && sourceSize.height == 266))   // Our no_app_image.png
                {
                    isPlaceholder = true
                }
                else
                {
                    isPlaceholder = false
                }

                width = 200
                height = 267
            }

            // Display a tooltip with the full name if it's truncated
            // ToolTip.text: model.name
            // ToolTip.delay: 1000
            // ToolTip.timeout: 5000
            // ToolTip.visible: (parent.hovered || parent.highlighted) && (!appNameText.visible || appNameText.truncated)
        }

    }
}