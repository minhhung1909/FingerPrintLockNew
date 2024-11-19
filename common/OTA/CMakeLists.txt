set(app_src "app_OTA.c")

set(pri_req)
idf_component_register(SRCS ${app_src}
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_event app_update esp_http_client mbedtls esp_https_ota esp_wifi ${pri_req})