import QtQuick 6.0

// Small section-header label (uppercase, muted)
Text {
    font.family:    Theme.fontFamily
    font.pixelSize: Theme.fontXs
    font.weight:    Font.Medium
    font.letterSpacing: 0.8
    color:          Theme.textMuted
    text:           text.toUpperCase()
}
