set(app_src "app_http_server.cpp")

set(pri_req)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_wifi json esp_http_server ${pri_req})

spiffs_create_partition_image(storage ../../data FLASH_IN_PROJECT)

target_add_binary_data(${COMPONENT_TARGET} "../../data/index.html" TEXT)

