set(U8G2_CLIB_DIR "${CMAKE_SOURCE_DIR}/.pio/libdeps/esp32-s3-devkitc-1/U8g2/src/clib")
file(GLOB U8G2_CLIB_SOURCES "${U8G2_CLIB_DIR}/*.c")

idf_component_register(
    SRCS
        "main/main.cpp"
        "core/AshuraCore.cpp"
        "core/WiFiManager.cpp"
        "network/WebSocketManager.cpp"
        "network/MessageRouter.cpp"
        "services/DeviceService.cpp"
        "application/wled/WledManager.cpp"
        ${U8G2_CLIB_SOURCES}

    INCLUDE_DIRS
        "."
        "main"
        "core"
        "network"
        "services"
        "application"
        "ui"
        "../include"

    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_timer
        esp_websocket_client
        mdns
        driver
        freertos
        log
        lwip
        heap
        spi_flash
        esp_hw_support
        esp_common
        esp_http_client
        json

    PRIV_REQUIRES
        esp_rom
)

target_compile_options(
    ${COMPONENT_LIB}
    PRIVATE
        -std=gnu++17
        -Wno-unused-function
        -Wno-missing-field-initializers
)

target_include_directories(
    ${COMPONENT_LIB}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${CMAKE_SOURCE_DIR}/.pio/libdeps/esp32-s3-devkitc-1/U8g2/src"
        "${CMAKE_SOURCE_DIR}/.pio/libdeps/esp32-s3-devkitc-1/U8g2/src/clib"
)
