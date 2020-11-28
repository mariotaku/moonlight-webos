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
import Eos.Window 0.1
import Eos.Controls 0.1

WebOSWindow {
    id: root
    width: 1920
    height: 1080
    visible: true
    appId: "com.limelight.webos"
    title: "Moonlight"
    color: "black"

    StackView {
        id: stackView
        initialItem: initialView
        anchors.fill: parent
        focus: true

        onCurrentItemChanged: {
            // Ensure focus travels to the next view when going back
            if (currentItem) {
                currentItem.forceActiveFocus()
            }
        }

        Keys.onEscapePressed: {
            root.navigateUp();
        }

        Keys.onBackPressed: {
            root.navigateUp();
        }

        Keys.onPressed: {
            if (event.key == Qt.Key_0) {
                root.navigateUp();
            }
        }
    }

    Keys.onPressed: {
        console.log(event.key);
    }

    onWindowStateChanged: {
        console.log(initialView)
        console.log("WINDOW_CHANGED, status:" + windowState)
    }

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