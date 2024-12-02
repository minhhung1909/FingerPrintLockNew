idf_component_register(SRCS "main.cpp" "finger_run.cpp"
                    INCLUDE_DIRS ".")                   

# spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)

# target_add_binary_data(${COMPONENT_TARGET} "../data/index.html" TEXT)