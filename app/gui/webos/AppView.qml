import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Layouts 1.1

import WebOS.Global 1.0
import Eos.Window 0.1
import Eos.Controls 0.1

import AppModel 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

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

    function onStackViewActivated() {
        appModel.computerLost.connect(computerLost)
        activated = true

        // Highlight the first item if a gamepad is connected
        if (currentIndex == -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }

        if (!showGames && !showHiddenGames) {
            // Check if there's a direct launch app
            var directLaunchAppIndex = model.getDirectLaunchAppIndex();
            if (directLaunchAppIndex >= 0) {
                // Start the direct launch app if nothing else is running
                currentIndex = directLaunchAppIndex
                currentItem.launchOrResumeSelectedApp(false)

                // Set showGames so we will not loop when the stream ends
                showGames = true
            }
        }
    }

    function onStackViewDeactivating() {
        appModel.computerLost.disconnect(computerLost)
        activated = false
    }

    Stack.onStatusChanged: {
        if (Stack.status == Stack.Active) {
            onStackViewActivated()
        } else if (Stack.status == Stack.Deactivating) {
            onStackViewDeactivating()
        }
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

        Column {
            anchors.centerIn: appIcon
            visible: model.running

            Button {
                text: "Resume Game"
                onClicked: launchOrResumeSelectedApp(true)
            }

            Button {
                text: "Quit Game"

                onClicked: doQuitGame()
            }
        }
        
        function launchOrResumeSelectedApp(quitExistingApp)
        {
            var runningId = appModel.getRunningAppId()
            if (runningId !== 0 && runningId !== model.appid) {
                if (quitExistingApp) {
                    quitAppDialog.appName = appModel.getRunningAppName()
                    quitAppDialog.segueToStream = true
                    quitAppDialog.nextAppName = model.name
                    quitAppDialog.nextAppIndex = index
                    quitAppDialog.open()
                }

                return
            }

            var component = Qt.createComponent("StreamSegue.qml")
            if (component.status != Component.Ready)
            {
                console.error(component.errorString());
                return;
            }
            var segue = component.createObject(stackView, {"appName": model.name, "session": appModel.createSessionForApp(index)})
            stackView.push(segue)
        }

        onClicked: {
            // Only allow clicking on the box art for non-running games.
            // For running games, buttons will appear to resume or quit which
            // will handle starting the game and clicks on the box art will
            // be ignored.
            if (!model.running) {
                launchOrResumeSelectedApp(true)
            }
        }
        
        function doQuitGame() {
            quitAppDialog.appName = appModel.getRunningAppName()
            quitAppDialog.segueToStream = false
            quitAppDialog.open()
        }

    }
    
    highlight: Rectangle { color: "lightsteelblue"; radius: 5 }
}