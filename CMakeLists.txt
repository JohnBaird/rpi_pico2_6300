# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD wiznet_w5100s_evb_pico2 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(rpi_pico2_6300 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Re-run CMake when the embedded factory config changes so GeneratedFactoryConfig.h
# always matches config/config.json.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/config/config.json"
    "${CMAKE_CURRENT_LIST_DIR}/config/wiegand.json"
)

file(READ "${CMAKE_CURRENT_LIST_DIR}/config/config.json" FACTORY_CONFIG_JSON)
file(READ "${CMAKE_CURRENT_LIST_DIR}/config/wiegand.json" WIEGAND_CONFIG_JSON)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/src/config/GeneratedFactoryConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/GeneratedFactoryConfig.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/src/config/GeneratedWiegandConfig.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/GeneratedWiegandConfig.h"
    @ONLY
)

pico_sdk_init()
add_subdirectory(third_party/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src third_party_sd_build)

add_executable(rpi_pico2_6300
    src/main.cpp
    src/config/ConfigManager.cpp
    src/core/App.cpp
    src/devices/Ds2401Manager.cpp
    src/devices/LedManager.cpp
    src/devices/Mcp9808TemperatureSensor.cpp
    src/devices/Rp2350TemperatureSensor.cpp
    src/i2c/ControllerIrqManager.cpp
    src/i2c/I2cCommandTransport.cpp
    src/i2c/I2cManager.cpp
    src/i2c/PacsControllerDevice.cpp
    src/i2c/PiWiegandDeviceManager.cpp
    src/i2c/Rp2350PacsProtocol.cpp
    src/wiegand/WiegandBuilder.cpp
    src/mqtt/MqttManager.cpp
    src/network/EthernetManager.cpp
    src/storage/FlashConfigStore.cpp
    src/storage/SdHwConfig.c
    src/storage/SdCardManager.cpp
    third_party/littlefs/lfs.c
    third_party/littlefs/lfs_util.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/socket.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/wizchip_conf.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W6300/w6300.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/DHCP/dhcp.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/mqtt_interface.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTClient.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTConnectClient.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTConnectServer.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTDeserializePublish.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTFormat.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTPacket.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTSerializePublish.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTSubscribeClient.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTSubscribeServer.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTUnsubscribeClient.c
    third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/MQTTUnsubscribeServer.c
    third_party/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_spi.c
    third_party/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_gpio_irq.c
    third_party/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.c
)

pico_generate_pio_header(rpi_pico2_6300
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/port/ioLibrary_Driver/src/wizchip_qspi_pio.pio
)

target_include_directories(rpi_pico2_6300 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/third_party/littlefs
    ${CMAKE_CURRENT_BINARY_DIR}/generated
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Ethernet/W6300
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/DHCP
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/libraries/ioLibrary_Driver/Internet/MQTT/MQTTPacket/src
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/port
    ${CMAKE_CURRENT_LIST_DIR}/third_party/WIZnet-PICO-C/port/ioLibrary_Driver/inc
)

target_compile_definitions(rpi_pico2_6300 PRIVATE
    _WIZCHIP_=W6300
    DEVICE_BOARD_NAME=W6300_EVB_PICO2
    _WIZCHIP_QSPI_MODE_=QSPI_QUAD_MODE
)

target_link_libraries(rpi_pico2_6300
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    hardware_adc
    hardware_clocks
    hardware_dma
    hardware_flash
    hardware_i2c
    hardware_pio
    hardware_spi
    pico_stdlib
)

pico_enable_stdio_usb(rpi_pico2_6300 1)
pico_enable_stdio_uart(rpi_pico2_6300 1)

pico_add_extra_outputs(rpi_pico2_6300)
