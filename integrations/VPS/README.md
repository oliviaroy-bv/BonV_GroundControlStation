# VpsGcs – VPS MAVLink TUNNEL GCS (Qt, TCP)

Build (Qt 5.15+ or Qt 6):

    cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
    cmake --build build -j
    ./build/VpsGcs

Set Host/Port (TCP client), and the Target sys/comp of the VPS, then Connect.
No external MAVLink library is needed: MAVLink 2 framing, CRC (crc_extra: HEARTBEAT=50, TUNNEL=147)
and TUNNEL zero-truncation are handled in MavlinkTcpLink.cpp.
