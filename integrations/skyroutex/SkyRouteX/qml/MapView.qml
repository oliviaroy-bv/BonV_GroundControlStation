import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0
import FlightPlanner 1.0

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────────
    signal takeoffPlaced(double lat, double lon)
    signal landPlaced(double lat, double lon)

    function setRoute(points)          { map.routePoints = points }
    function clearRoute()              { map.clearRoute() }
    function panTo(lat, lon)           { map.panTo(lat, lon) }
    function setTakeoffMarker(lat, lon){ map.takeoffLat = lat; map.takeoffLon = lon; map.hasTakeoff = true }
    function setLandMarker(lat, lon)   { map.landLat = lat;    map.landLon = lon;    map.hasLand    = true }
    function setClickModeInternal(mode){ map.clickMode = mode }
    property string cacheDir: ""

    onCacheDirChanged: {
        if (cacheDir !== "")
            map.setCacheDir(cacheDir)
    }

    Component.onCompleted: {
        if (cacheDir !== "")
            map.setCacheDir(cacheDir)
    }

    // ── Search state ──────────────────────────────────────────────────────────
    property bool   searching:      false
    property string searchError:    ""
    property var    searchResults:  []   // [{name, lat, lon}, ...]
    property bool   showResults:    false

    // Called by backend geocode signals
    function onGeocodeDone(results) {
        searching   = false
        searchResults = results
        showResults = results.length > 0
        if (results.length === 0) searchError = "No results found"
        else searchError = ""
    }

    function onGeocodeError(msg) {
        searching   = false
        searchError = msg
        showResults = false
    }

    // ── Layout ────────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Search bar ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color:  Theme.bg
            Rectangle {
                anchors.bottom: parent.bottom
                height: 0.5; width: parent.width
                color: Theme.border
            }

            RowLayout {
                anchors {
                    left: parent.left; right: parent.right
                    leftMargin: 10; rightMargin: 10
                    verticalCenter: parent.verticalCenter
                }
                spacing: 8

                // Search icon
                Canvas {
                    width: 16; height: 16
                    Layout.alignment: Qt.AlignVCenter
                    onPaint: {
                        var c = getContext("2d")
                        c.clearRect(0, 0, 16, 16)
                        c.strokeStyle = Theme.textMuted
                        c.lineWidth   = 1.5
                        c.beginPath(); c.arc(6, 6, 4.5, 0, Math.PI * 2); c.stroke()
                        c.beginPath(); c.moveTo(9.5, 9.5); c.lineTo(14, 14); c.stroke()
                    }
                }

                // Search input
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    radius: Theme.radius
                    color:  searchField.activeFocus ? Theme.bgInputFocus : Theme.bgInput
                    border.color: searchField.activeFocus ? Theme.borderFocus : Theme.border
                    border.width: searchField.activeFocus ? 1 : 0.5

                    TextInput {
                        id: searchField
                        anchors {
                            left: parent.left; right: clearBtn.left
                            leftMargin: 8; rightMargin: 4
                            verticalCenter: parent.verticalCenter
                        }
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontBase
                        color:          Theme.textPrimary
                        clip:           true
                        selectByMouse:  true

                        // Placeholder
                        Text {
                            anchors.fill: parent
                            text:    "Search location or paste lat, lon..."
                            color:   Theme.textHint
                            font:    searchField.font
                            visible: searchField.text.length === 0 && !searchField.activeFocus
                            verticalAlignment: Text.AlignVCenter
                        }

                        Keys.onReturnPressed:  root.doSearch(text)
                        Keys.onEnterPressed:   root.doSearch(text)
                        Keys.onEscapePressed: {
                            showResults = false
                            searchError = ""
                            focus = false
                        }
                    }

                    // Clear X button
                    Rectangle {
                        id: clearBtn
                        anchors { right: parent.right; rightMargin: 4; verticalCenter: parent.verticalCenter }
                        width: 18; height: 18; radius: 9
                        color:   clearBtnArea.containsMouse ? Theme.bgSection : "transparent"
                        visible: searchField.text.length > 0

                        Text {
                            anchors.centerIn: parent
                            text:       "×"
                            font.pixelSize: 14
                            color:      Theme.textMuted
                        }
                        MouseArea {
                            id: clearBtnArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape:  Qt.PointingHandCursor
                            onClicked: {
                                searchField.text = ""
                                showResults      = false
                                searchError      = ""
                                searchField.forceActiveFocus()
                            }
                        }
                    }
                }

                // Search / loading indicator
                Rectangle {
                    width: 60; height: 28
                    radius: Theme.radius
                    color:  searching ? Theme.bgSection : Theme.blueMid
                    border.color: searching ? Theme.border : "transparent"
                    border.width: 0.5

                    Text {
                        anchors.centerIn: parent
                        text:           searching ? "..." : "Search"
                        font.family:    Theme.fontFamily
                        font.pixelSize: Theme.fontSm
                        font.weight:    Font.Medium
                        color:          searching ? Theme.textMuted : "#FFFFFF"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape:  Qt.PointingHandCursor
                        onClicked:    if (!searching) root.doSearch(searchField.text)
                    }
                }

                // Divider
                Rectangle { width: 0.5; height: 20; color: Theme.border }

                // Place mode buttons
                Text {
                    text: "Place:"
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    color:          Theme.textMuted
                }

                Repeater {
                    model: [
                        { label: "Takeoff", mode: 0, activeColor: "#185FA5" },
                        { label: "Landing", mode: 1, activeColor: "#3B6D11" }
                    ]
                    delegate: Rectangle {
                        height: 26
                        width:  modeLabel.implicitWidth + 16
                        radius: Theme.radius
                        color:  map.clickMode === modelData.mode ? modelData.activeColor : Theme.bgSection
                        border.color: map.clickMode === modelData.mode ? "transparent" : Theme.border
                        border.width: 0.5
                        Behavior on color { ColorAnimation { duration: 100 } }

                        Text {
                            id: modeLabel
                            anchors.centerIn: parent
                            text:           modelData.label
                            font.family:    Theme.fontFamily
                            font.pixelSize: Theme.fontSm
                            font.weight:    map.clickMode === modelData.mode ? Font.Medium : Font.Normal
                            color:          map.clickMode === modelData.mode ? "#FFFFFF" : Theme.textSecond
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape:  Qt.PointingHandCursor
                            onClicked:    map.clickMode = modelData.mode
                        }
                    }
                }

                // Divider
                Rectangle { width: 0.5; height: 20; color: Theme.border }

                // Map source
                FpComboBox {
                    implicitWidth: 130; implicitHeight: 26
                    model: ["Bing Satellite", "Bing Maps", "Google Satellite", "Google Maps", "OpenStreetMap"]
                    onCurrentIndexChanged: map.tileSource = currentIndex
                }

                // Zoom
                Text {
                    text: "Z " + map.zoomLevel
                    font.family:    Theme.fontFamily
                    font.pixelSize: Theme.fontSm
                    color:          Theme.textMuted
                    width:          30
                }
                FpButton { text: "−"; implicitWidth: 26; implicitHeight: 26; onClicked: if (map.zoomLevel > 3)  map.zoomLevel-- }
                FpButton { text: "+"; implicitWidth: 26; implicitHeight: 26; onClicked: if (map.zoomLevel < 21) map.zoomLevel++ }

                // Divider
                Rectangle { width: 0.5; height: 20; color: Theme.border }

                FpButton {
                    text: "Clear"; implicitWidth: 48; implicitHeight: 26
                    onClicked: { map.hasTakeoff = false; map.hasLand = false; map.clearRoute() }
                }
            }
        }

        // ── Map ───────────────────────────────────────────────────────────────
        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            MapWidget {
                id: map
                anchors.fill: parent
                centerLat: 20.5; centerLon: 78.9; zoomLevel: 6

                onTakeoffPlaced: function(lat, lon) { root.takeoffPlaced(lat, lon) }
                onLandPlaced:    function(lat, lon) { root.landPlaced(lat, lon) }
                onTileStatusChanged: function(s)    { tileStatusText.text = s }
            }

            // ── Search results dropdown ───────────────────────────────────────
            Rectangle {
                id: resultsDropdown
                visible:      root.showResults || root.searchError !== ""
                anchors {
                    top:   parent.top
                    left:  parent.left
                    leftMargin: 10
                }
                width:   340
                height:  resultsList.implicitHeight + (root.searchError !== "" ? 36 : 0)
                radius:  Theme.radiusLg
                color:   Theme.bg
                border.color: Theme.border
                border.width: 0.5
                z: 10

                // Drop shadow effect via border
                layer.enabled: true

                ColumnLayout {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: 0

                    // Error message
                    Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        visible: root.searchError !== ""
                        color:   "transparent"

                        Text {
                            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                            text:           root.searchError
                            font.family:    Theme.fontFamily
                            font.pixelSize: Theme.fontBase
                            color:          Theme.red
                        }
                    }

                    // Results list
                    Column {
                        id: resultsList
                        Layout.fillWidth: true

                        Repeater {
                            model: root.searchResults

                            delegate: Rectangle {
                                width:  resultsDropdown.width
                                height: 44
                                color:  resultArea.containsMouse ? Theme.blueLight : "transparent"

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    height: 0.5; width: parent.width
                                    color: Theme.borderLight
                                    visible: index < root.searchResults.length - 1
                                }

                                ColumnLayout {
                                    anchors {
                                        left: parent.left; right: parent.right
                                        leftMargin: 12; rightMargin: 12
                                        verticalCenter: parent.verticalCenter
                                    }
                                    spacing: 2

                                    Text {
                                        text:           modelData.name
                                        font.family:    Theme.fontFamily
                                        font.pixelSize: Theme.fontBase
                                        color:          Theme.textPrimary
                                        elide:          Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Text {
                                        text:           modelData.lat.toFixed(6) + ",  " + modelData.lon.toFixed(6)
                                        font.family:    Theme.fontFamily
                                        font.pixelSize: Theme.fontSm
                                        color:          Theme.textMuted
                                    }
                                }

                                MouseArea {
                                    id: resultArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape:  Qt.PointingHandCursor
                                    onClicked: {
                                        // Pan map to result and zoom in
                                        map.panTo(modelData.lat, modelData.lon)
                                        map.zoomLevel  = 14
                                        root.showResults = false
                                        searchField.text = modelData.name
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Tap outside results to close
            MouseArea {
                anchors.fill: parent
                visible:      root.showResults
                z:            9
                onClicked:    root.showResults = false
            }
        }

        // ── Status strip ──────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 22
            color: Theme.bg
            Rectangle { anchors.top: parent.top; height: 0.5; width: parent.width; color: Theme.border }

            Row {
                anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                spacing: 12

                Text {
                    id: tileStatusText
                    text: "Ready"
                    font.family:    Theme.fontFamily
                    font.pixelSize: 10
                    color:          Theme.textMuted
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    width: 0.5; height: 12; color: Theme.border
                    anchors.verticalCenter: parent.verticalCenter
                    visible: markerInfo.text !== ""
                }

                Text {
                    id: markerInfo
                    font.family:    Theme.fontFamily
                    font.pixelSize: 10
                    color:          Theme.textMuted
                    anchors.verticalCenter: parent.verticalCenter
                    text: {
                        if (map.hasTakeoff && map.hasLand)
                            return "T: " + map.takeoffLat.toFixed(5) + ", " + map.takeoffLon.toFixed(5)
                                 + "   L: " + map.landLat.toFixed(5) + ", " + map.landLon.toFixed(5)
                        if (map.hasTakeoff)
                            return "Takeoff: " + map.takeoffLat.toFixed(5) + ", " + map.takeoffLon.toFixed(5)
                                 + "  — click map to place Landing"
                        return "Click map to place Takeoff marker"
                    }
                }
            }
        }
    }

    // ── Search logic ──────────────────────────────────────────────────────────

    // Connections to backend geocode signals
    Connections {
        target: backend
        ignoreUnknownSignals: true
        function onGeocodeDone(results)  { root.onGeocodeDone(results) }
        function onGeocodeError(msg)     { root.onGeocodeError(msg) }
    }

    function doSearch(query) {
        query = query.trim()
        if (query.length === 0) return

        searchError  = ""
        showResults  = false

        // ── Try parsing as "lat, lon" first ───────────────────────────────────
        // Accepts:  "28.7381, 77.1025"  or  "28.7381619 84.3887329"
        var coordRegex = /^(-?\d+\.?\d*)[,\s]+(-?\d+\.?\d*)$/
        var match = query.match(coordRegex)
        if (match) {
            var lat = parseFloat(match[1])
            var lon = parseFloat(match[2])
            if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
                map.panTo(lat, lon)
                map.zoomLevel = 14
                // Show as a single result so user can confirm
                searchResults = [{ name: "Coordinates: " + lat.toFixed(6) + ", " + lon.toFixed(6),
                                   lat: lat, lon: lon }]
                showResults   = true
                return
            }
        }

        // ── Otherwise geocode via Nominatim ───────────────────────────────────
        searching = true
        backend.geocodeSearch(query)
    }
}
