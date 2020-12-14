// Copyright (c) 2020 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

import QtQuick 2.4
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.1

import WebOS.Global 1.0
import Eos.Window 0.1
import Eos.Controls 0.1

import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0

WebOSWindow {
    property bool pollingActive: false

    id: window
    visible: true
    width: 1920
    height: 1080

    title: "Moonlight"
    color: "black"
    keyMask: WebOSWindow.KeyMaskBack | keyMask

    Component.onCompleted: {
        windowProperties["_WEBOS_ACCESS_POLICY_KEYS_BACK"] = "true"
    }

    ColumnLayout {
        anchors.fill: parent

        Header {
            id: pageHeader
            Layout.fillWidth: true
            headerText: window.title

            RowLayout {   
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    rightMargin: 10
                    bottomMargin: 10
                }                 

                Button {
                    id: addDevice
                    iconSource: "qrc:/res/webos/add-device.png"
                }
                Button {
                    id: showHelp
                    iconSource: "qrc:/res/webos/help.png"
                }
                Button {
                    id: settings
                    iconSource: "qrc:/res/webos/settings.png"
                    onClicked: navigateTo("qrc:/gui/webos/SettingsView.qml", qsTr("Settings"))
                }
            }
        }
        
        StackView {
            id: stackView
            initialItem: initialView
            Layout.fillWidth: true
            Layout.fillHeight: true
            focus: true

            onCurrentItemChanged: {
                // Ensure focus travels to the next view when going back
                if (currentItem) {
                    currentItem.forceActiveFocus();
                    pageHeader.headerText = currentItem.objectName || window.title;
                }
            }
        }
        
        Keys.onPressed: {
            switch (event.key) {
                case WebOS.Key_webOS_Back: {
                    window.navigateUp();
                    break;
                }
                case WebOS.Key_webOS_Blue: {
                    addDevice.clicked(null);
                    break;
                }
                case WebOS.Key_webOS_Green: {
                    showHelp.clicked(null);
                    break;
                }
                case WebOS.Key_webOS_Yellow: {
                    settings.clicked(null);
                    break;
                }
            }
        }

        Keys.onEscapePressed: {
            window.navigateUp()
        }
        
        Keys.onBackPressed: {
            window.navigateUp()
        }
    }

    // This timer keeps us polling for 5 minutes of inactivity
    // to allow the user to work with Moonlight on a second display
    // while dealing with configuration issues. This will ensure
    // machines come online even if the input focus isn't on Moonlight.
    Timer {
        id: inactivityTimer
        interval: 5 * 60000
        onTriggered: {
            if (!active && pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
    }

    onVisibleChanged: {
        // When we become invisible while streaming is going on,
        // stop polling immediately.
        if (!visible) {
            inactivityTimer.stop()

            if (pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
        else if (active) {
            // When we become visible and active again, start polling
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }
    }

    onActiveChanged: {
        if (active) {
            // Stop the inactivity timer
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }
        else {
            // Start the inactivity timer to stop polling
            // if focus does not return within a few minutes.
            inactivityTimer.restart()
        }
    }

    property bool initialized: false

    function navigateUp() {
        if (stackView.depth > 1) {
            stackView.pop()
        }
        else {
            Qt.quit()
        }
    }

    function navigateTo(url, objectName) {
        var existingItem = stackView.find(function(item, index) {
            return item.objectName === objectName
        })

        if (existingItem !== null) {
            // Pop to the existing item
            stackView.pop(existingItem)
        } else {
            // Create a new item
            stackView.push(url)
        }
    }
}