idf_component_register(SRCS "time_sync.c" "main.cpp" "finger_run.cpp" "http_request.c" "time_sync.c"
                    INCLUDE_DIRS ".")                   

# spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)

# target_add_binary_data(${COMPONENT_TARGET} "../data/index.html" TEXT)