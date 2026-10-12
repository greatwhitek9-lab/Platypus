cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(platypus_sniffer)

target_sources(app PRIVATE
    src/main.c
    src/radio_ble.c
    src/channel_hop.c
    src/proto_framer.c
    src/usb_transport.c
)

target_include_directories(app PRIVATE src)
