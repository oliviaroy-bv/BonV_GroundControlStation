import QtQuick 6.0
import QtQuick.Controls 6.0
import QtQuick.Layouts 6.0

// TerrainProfile.qml
// 2D cross-section chart: terrain (brown) vs drone altitude (blue).
// Green shaded area = clearance. Hover for per-waypoint tooltip.
//
// Usage:
//   TerrainProfile {
//       id: profile
//       points: [{distKm, droneAlt, terrainAlt}, ...]
//   }

Item {
    id: root

    property var  points:       []
    property bool hasData:      points.length >= 2
    property real minClearance: 0
    property real droneMin:     0
    property real droneMax:     0
    property real terrainMin:   0
    property real terrainMax:   0

    onPointsChanged: {
        if (points.length >= 2) {
            var minC  = 999999
            var dMin  = points[0].droneAlt
            var dMax  = points[0].droneAlt
            var tMin  = points[0].terrainAlt
            var tMax  = points[0].terrainAlt
            for (var i = 0; i < points.length; i++) {
                var c = points[i].droneAlt - points[i].terrainAlt
                if (c < minC)                  minC = c
                if (points[i].droneAlt   < dMin) dMin = points[i].droneAlt
                if (points[i].droneAlt   > dMax) dMax = points[i].droneAlt
                if (points[i].terrainAlt < tMin) tMin = points[i].terrainAlt
                if (points[i].terrainAlt > tMax) tMax = points[i].terrainAlt
            }
            minClearance = minC
            droneMin     = dMin
            droneMax     = dMax
            terrainMin   = tMin
            terrainMax   = tMax
        }
        canvas.requestPaint()
    }

    readonly property int marginL: 58
    readonly property int marginR: 16
    readonly property int marginT: Theme.isAndroid ? 8 : 16
    readonly property int marginB: Theme.isAndroid ? 24 : 38

    // ── Header ────────────────────────────────────────────────────────────────
    Rectangle {
        id: header
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: Theme.isAndroid ? 26 : 34
        color:  Theme.bg

        Rectangle {
            anchors.bottom: parent.bottom
            height: 0.5; width: parent.width
            color: Theme.border
        }

        Row {
            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 16

            Text {
                text:           "Terrain Profile"
                font.family:    Theme.fontFamily
                font.pixelSize: Theme.fontBase
                font.weight:    Font.Medium
                color:          Theme.textPrimary
                anchors.verticalCenter: parent.verticalCenter
            }

            Row {
                spacing: 12
                visible: root.hasData
                anchors.verticalCenter: parent.verticalCenter

                LegendDot { dotColor: "#185FA5"; label: "Drone path (Min: " + root.droneMin.toFixed(0) + " m  Max: " + root.droneMax.toFixed(0) + " m)" }
                LegendDot { dotColor: "#8B6914"; label: "Terrain (Min: " + root.terrainMin.toFixed(0) + " m  Max: " + root.terrainMax.toFixed(0) + " m)" }
            }
        }

        Rectangle {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            visible:      root.hasData
            height:       22
            width:        badgeTxt.implicitWidth + 16
            radius:       4
            color:        root.minClearance >= 80 ? Theme.greenLight : Theme.amberLight
            border.color: root.minClearance >= 80 ? "#C0DD97"        : "#FAC775"
            border.width: 0.5

            Text {
                id: badgeTxt
                anchors.centerIn: parent
                text:           "Min clearance: " + root.minClearance.toFixed(0) + " m"
                font.family:    Theme.fontFamily
                font.pixelSize: 10
                font.weight:    Font.Medium
                color:          root.minClearance >= 80 ? Theme.green : Theme.amber
            }
        }
    }

    // ── Canvas ────────────────────────────────────────────────────────────────
    Canvas {
        id: canvas
        anchors {
            top:    header.bottom
            left:   parent.left
            right:  parent.right
            bottom: parent.bottom
        }

        function niceStep(raw) {
            var exp  = Math.floor(Math.log10(Math.max(raw, 0.001)))
            var frac = raw / Math.pow(10, exp)
            var nice = frac < 1.5 ? 1 : frac < 3.5 ? 2 : frac < 7.5 ? 5 : 10
            return nice * Math.pow(10, exp)
        }

        function drawLabel(ctx, x, y, text, color) {
            ctx.fillStyle = color
            ctx.beginPath()
            ctx.arc(x, y, 8, 0, Math.PI * 2)
            ctx.fill()
            ctx.fillStyle   = "#FFFFFF"
            ctx.font        = "bold 8px 'Segoe UI'"
            ctx.textAlign   = "center"
            ctx.fillText(text, x, y + 3)
        }

        onPaint: {
            var ctx = getContext("2d")
            var W   = width
            var H   = height
            var mL  = root.marginL
            var mR  = root.marginR
            var mT  = root.marginT
            var mB  = root.marginB
            var pw  = W - mL - mR
            var ph  = H - mT - mB

            ctx.clearRect(0, 0, W, H)
            ctx.fillStyle = "#FAFAF8"
            ctx.fillRect(0, 0, W, H)

            if (!root.hasData || root.points.length < 2) {
                ctx.fillStyle  = "#B4B2A9"
                ctx.font       = "13px 'Segoe UI'"
                ctx.textAlign  = "center"
                ctx.fillText("Generate a mission to see the terrain profile", W / 2, H / 2)
                return
            }

            var pts    = root.points
            var maxDist = pts[pts.length - 1].distKm
            var minAlt  = pts[0].terrainAlt
            var maxAlt  = pts[0].droneAlt

            for (var i = 0; i < pts.length; i++) {
                if (pts[i].terrainAlt < minAlt) minAlt = pts[i].terrainAlt
                if (pts[i].droneAlt   > maxAlt) maxAlt = pts[i].droneAlt
            }

            // Ensure the clearance gap is always clearly visible.
            // We need at least 20% of the plot height to show the clearance zone.
            var rawRange  = maxAlt - minAlt
            var yPad      = Math.max(rawRange * 0.15, 50)  // min 50m padding
            minAlt        = Math.max(0, minAlt - yPad)
            maxAlt        = maxAlt + yPad * 0.5
            var yRange    = maxAlt - minAlt
            if (yRange < 1) yRange = 1

            function xPx(d) { return mL + (d / maxDist) * pw }
            function yPx(a) { return mT + ph - ((a - minAlt) / yRange) * ph }

            // Grid
            ctx.strokeStyle = "#E8E6DF"
            ctx.lineWidth   = 0.5
            ctx.setLineDash([4, 4])

            var yStep  = niceStep(yRange / 5)
            var yStart = Math.ceil(minAlt / yStep) * yStep
            for (var yv = yStart; yv <= maxAlt; yv += yStep) {
                ctx.beginPath()
                ctx.moveTo(mL, yPx(yv))
                ctx.lineTo(W - mR, yPx(yv))
                ctx.stroke()
            }

            var xSteps = Math.min(8, pts.length - 1)
            for (var xi = 0; xi <= xSteps; xi++) {
                var xv = (maxDist / xSteps) * xi
                ctx.beginPath()
                ctx.moveTo(xPx(xv), mT)
                ctx.lineTo(xPx(xv), H - mB)
                ctx.stroke()
            }
            ctx.setLineDash([])

            // Terrain fill
            ctx.beginPath()
            ctx.moveTo(xPx(pts[0].distKm), yPx(pts[0].terrainAlt))
            for (var i = 1; i < pts.length; i++)
                ctx.lineTo(xPx(pts[i].distKm), yPx(pts[i].terrainAlt))
            ctx.lineTo(xPx(pts[pts.length-1].distKm), H - mB)
            ctx.lineTo(xPx(pts[0].distKm), H - mB)
            ctx.closePath()
            ctx.fillStyle = "#C4A882"
            ctx.fill()

            // Terrain outline
            ctx.beginPath()
            ctx.moveTo(xPx(pts[0].distKm), yPx(pts[0].terrainAlt))
            for (var i = 1; i < pts.length; i++)
                ctx.lineTo(xPx(pts[i].distKm), yPx(pts[i].terrainAlt))
            ctx.strokeStyle = "#8B6914"
            ctx.lineWidth   = 1.5
            ctx.stroke()

            // Clearance fill
            ctx.beginPath()
            ctx.moveTo(xPx(pts[0].distKm), yPx(pts[0].droneAlt))
            for (var i = 1; i < pts.length; i++)
                ctx.lineTo(xPx(pts[i].distKm), yPx(pts[i].droneAlt))
            for (var i = pts.length - 1; i >= 0; i--)
                ctx.lineTo(xPx(pts[i].distKm), yPx(pts[i].terrainAlt))
            ctx.closePath()
            ctx.fillStyle = "rgba(99, 153, 34, 0.18)"
            ctx.fill()

            // Drone line
            ctx.beginPath()
            ctx.moveTo(xPx(pts[0].distKm), yPx(pts[0].droneAlt))
            for (var i = 1; i < pts.length; i++)
                ctx.lineTo(xPx(pts[i].distKm), yPx(pts[i].droneAlt))
            ctx.strokeStyle = "#185FA5"
            ctx.lineWidth   = 2
            ctx.stroke()

            // Waypoint dots
            for (var i = 0; i < pts.length; i++) {
                var dx = xPx(pts[i].distKm)
                var dy = yPx(pts[i].droneAlt)
                ctx.beginPath()
                ctx.arc(dx, dy, 5, 0, Math.PI * 2)
                ctx.fillStyle   = "#FFFFFF"
                ctx.fill()
                ctx.strokeStyle = "#185FA5"
                ctx.lineWidth   = 1.5
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(dx, dy, 2.5, 0, Math.PI * 2)
                ctx.fillStyle = "#185FA5"
                ctx.fill()
            }

            // T / L markers
            drawLabel(ctx, xPx(pts[0].distKm),              yPx(pts[0].droneAlt) - 14,              "T", "#185FA5")
            drawLabel(ctx, xPx(pts[pts.length-1].distKm),   yPx(pts[pts.length-1].droneAlt) - 14,   "L", "#3B6D11")

            // Axes
            ctx.strokeStyle = "#D3D1C7"
            ctx.lineWidth   = 0.5
            ctx.beginPath()
            ctx.moveTo(mL, mT); ctx.lineTo(mL, H - mB)
            ctx.moveTo(mL, H - mB); ctx.lineTo(W - mR, H - mB)
            ctx.stroke()

            // Y labels
            ctx.fillStyle = "#888780"
            ctx.font      = "10px 'Segoe UI'"
            ctx.textAlign = "right"
            for (var yv = yStart; yv <= maxAlt; yv += yStep)
                ctx.fillText(yv.toFixed(0) + " m", mL - 6, yPx(yv) + 3)

            // X labels
            ctx.textAlign = "center"
            for (var xi = 0; xi <= xSteps; xi++) {
                var xv = (maxDist / xSteps) * xi
                ctx.fillText(xv.toFixed(1) + " km", xPx(xv), H - mB + 14)
            }

            // Axis titles
            ctx.save()
            ctx.translate(12, mT + ph / 2)
            ctx.rotate(-Math.PI / 2)
            ctx.textAlign = "center"
            ctx.fillStyle = "#5F5E5A"
            ctx.font      = "10px 'Segoe UI'"
            ctx.fillText("Altitude (m)", 0, 0)
            ctx.restore()

            ctx.textAlign = "center"
            ctx.fillStyle = "#5F5E5A"
            ctx.fillText("Distance from Takeoff", mL + pw / 2, H - 6)
        }
    }

    // ── Hover tooltip ─────────────────────────────────────────────────────────
    MouseArea {
        anchors.fill: canvas
        hoverEnabled: true

        onPositionChanged: function(mouse) {
            if (!root.hasData || root.points.length < 2) {
                tooltip.visible = false; return
            }
            var mL  = root.marginL
            var pw  = canvas.width - mL - root.marginR
            var pts = root.points
            var relX = mouse.x - mL
            if (relX < 0 || relX > pw) { tooltip.visible = false; return }

            var frac   = relX / pw
            var distKm = frac * pts[pts.length - 1].distKm
            var best   = 0
            var bestD  = 99999
            for (var i = 0; i < pts.length; i++) {
                var d = Math.abs(pts[i].distKm - distKm)
                if (d < bestD) { bestD = d; best = i }
            }

            var pt = pts[best]
            tooltip.distKm     = pt.distKm
            tooltip.droneAlt   = pt.droneAlt
            tooltip.terrainAlt = pt.terrainAlt
            tooltip.clearance  = pt.droneAlt - pt.terrainAlt

            var tx = mouse.x + 12
            if (tx + tooltip.width > canvas.width) tx = mouse.x - tooltip.width - 8
            tooltip.x       = tx
            tooltip.y       = Math.max(root.marginT,
                              Math.min(mouse.y - 20,
                                       canvas.height - root.marginB - tooltip.height))
            tooltip.visible = true
        }

        onExited: { tooltip.visible = false }
    }

    Rectangle {
        id: tooltip
        visible: false
        z:       10
        width:   148
        height:  tooltipCol.implicitHeight + 14
        radius:  Theme.radius
        color:   Theme.bg
        border.color: Theme.border
        border.width: 0.5

        property real distKm:     0
        property real droneAlt:   0
        property real terrainAlt: 0
        property real clearance:  0

        ColumnLayout {
            id: tooltipCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
            spacing: 3

            TooltipRow { label: "Distance";  value: tooltip.distKm.toFixed(2)     + " km" }
            TooltipRow { label: "Drone alt"; value: tooltip.droneAlt.toFixed(0)   + " m"  }
            TooltipRow { label: "Terrain";   value: tooltip.terrainAlt.toFixed(0) + " m"  }
            TooltipRow { label: "Clearance"; value: tooltip.clearance.toFixed(0)  + " m";
                         highlight: tooltip.clearance < 80 }
        }
    }
}
