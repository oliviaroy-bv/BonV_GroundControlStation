# Bundled GStreamer SDK

Place the Windows GStreamer SDK in this directory when you want the project to build and run video streaming without requiring a separate machine-wide GStreamer install.

Supported layouts:

```text
GStreamer/bin
GStreamer/lib/pkgconfig
GStreamer/lib/gstreamer-1.0
GStreamer/libexec/gstreamer-1.0
```

or the official installer layout:

```text
GStreamer/gstreamer/1.0/msvc_x86_64/bin
GStreamer/gstreamer/1.0/msvc_x86_64/lib/pkgconfig
GStreamer/gstreamer/1.0/msvc_x86_64/lib/gstreamer-1.0
GStreamer/gstreamer/1.0/msvc_x86_64/libexec/gstreamer-1.0
```

Use the MSVC x86_64 SDK when building with Visual Studio/MSVC. The CMake build prefers this bundled SDK when it finds it here, then copies the needed GStreamer runtime files next to the built executable.
