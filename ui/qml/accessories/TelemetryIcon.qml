import QtQuick

Canvas {
    property string iconType: ""
    property color iconColor: "#b5bcc2"

    width: 13
    height: 13

    onIconTypeChanged: requestPaint()
    onIconColorChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.strokeStyle = iconColor
        ctx.fillStyle = iconColor
        ctx.lineWidth = 1.5
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        if (iconType === "shield") {
            ctx.beginPath()
            ctx.moveTo(6.5, 1.2)
            ctx.lineTo(11, 3)
            ctx.lineTo(10.2, 7.5)
            ctx.lineTo(6.5, 11.5)
            ctx.lineTo(2.8, 7.5)
            ctx.lineTo(2, 3)
            ctx.closePath()
            ctx.stroke()
        } else if (iconType === "mode") {
            ctx.beginPath()
            ctx.arc(6.5, 6.5, 5, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(6.5, 6.5)
            ctx.lineTo(9.5, 3.5)
            ctx.stroke()
        } else if (iconType === "altitude" || iconType === "climb") {
            ctx.beginPath()
            ctx.moveTo(6.5, 1.5)
            ctx.lineTo(10.5, 5.5)
            ctx.moveTo(6.5, 1.5)
            ctx.lineTo(2.5, 5.5)
            ctx.moveTo(6.5, 1.5)
            ctx.lineTo(6.5, 11.5)
            ctx.stroke()
        } else if (iconType === "home") {
            ctx.beginPath()
            ctx.moveTo(2, 6)
            ctx.lineTo(6.5, 2)
            ctx.lineTo(11, 6)
            ctx.moveTo(3.5, 5.5)
            ctx.lineTo(3.5, 11)
            ctx.lineTo(9.5, 11)
            ctx.lineTo(9.5, 5.5)
            ctx.stroke()
        } else if (iconType === "speed") {
            ctx.beginPath()
            ctx.arc(6.5, 7.5, 5, Math.PI, Math.PI * 2)
            ctx.moveTo(6.5, 7.5)
            ctx.lineTo(10, 5)
            ctx.stroke()
        } else if (iconType === "time") {
            ctx.beginPath()
            ctx.arc(6.5, 6.5, 5, 0, Math.PI * 2)
            ctx.moveTo(6.5, 6.5)
            ctx.lineTo(6.5, 3.5)
            ctx.moveTo(6.5, 6.5)
            ctx.lineTo(9, 8)
            ctx.stroke()
        } else if (iconType === "distance") {
            ctx.beginPath()
            ctx.moveTo(2, 9)
            ctx.bezierCurveTo(4, 3, 9, 10, 11, 4)
            ctx.stroke()
        } else if (iconType === "attitude") {
            ctx.beginPath()
            ctx.moveTo(2, 7)
            ctx.lineTo(11, 7)
            ctx.moveTo(4, 4)
            ctx.lineTo(9, 10)
            ctx.stroke()
        } else if (iconType === "battery") {
            ctx.strokeRect(1.5, 4, 9, 5)
            ctx.fillRect(10.5, 5.5, 1.2, 2)
            ctx.fillRect(3, 5.5, 5, 2)
        } else if (iconType === "satellite") {
            ctx.beginPath()
            ctx.arc(4.5, 4.5, 2, 0, Math.PI * 2)
            ctx.moveTo(7, 7)
            ctx.lineTo(11, 11)
            ctx.moveTo(2, 11)
            ctx.quadraticCurveTo(6.5, 8.5, 11, 11)
            ctx.stroke()
        } else if (iconType === "gps") {
            ctx.beginPath()
            ctx.arc(6.5, 5.5, 3.5, 0, Math.PI * 2)
            ctx.moveTo(6.5, 9)
            ctx.lineTo(6.5, 12)
            ctx.stroke()
        } else if (iconType === "motor") {
            var cx = width / 2
            var cy = height / 2
            var hubRadius = Math.min(width, height) * 0.17
            var armInner = Math.min(width, height) * 0.26
            var armOuter = Math.min(width, height) * 0.43

            ctx.beginPath()
            ctx.arc(cx, cy, hubRadius, 0, Math.PI * 2)
            ctx.stroke()
            for (var i = 0; i < 4; i++) {
                ctx.save()
                ctx.translate(cx, cy)
                ctx.rotate((Math.PI / 2) * i)
                ctx.beginPath()
                ctx.moveTo(0, -armInner)
                ctx.lineTo(0, -armOuter)
                ctx.stroke()
                ctx.restore()
            }
        }
    }
}
