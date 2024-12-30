extern "C" {
    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "driver/gpio.h"
    #include "esp_log.h"
    #include "sdkconfig.h"
    #include "driver/uart.h"
    #include "esp_system.h"
    #include "esp_wifi.h"
    #include "lwip/sys.h"
    #include <nvs_flash.h>
    #include "app_http_server.h"
    #include "app_OTA.h"
    #include "http_request.h"
    #include "time_sync.h"
    #include <time.h>
    #include <sys/time.h>
    #include "esp_timer.h"
}
#include "finger_run.h"

extern "C" {
    void app_main(void);
    void fetch_and_store_time_in_nvs_wrapper(void* args);
}

/* ---------- START DEFINE ---------- */

#define TIME_PERIOD (86400000000ULL)
/* ---------- END DEFINE ---------- */

/* ------------- START VARIABLE ------------- */

static const char *TAG_CHECK = "check";
/* ------------- END ------------- */


/* ---- START FUNCTION ----- */

void fetch_and_store_time_in_nvs_wrapper(void* args) {
    esp_err_t err = fetch_and_store_time_in_nvs(args);
}
/* ---- END FUNCTION ----- */
void app_main(void)
{    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (esp_reset_reason() == ESP_RST_POWERON) {
        ESP_LOGI(TAG_CHECK, "Updating time from NVS");
        ESP_ERROR_CHECK(update_time_from_nvs());
    }

    const esp_timer_create_args_t nvs_update_timer_args = {
        .callback = &fetch_and_store_time_in_nvs_wrapper,
    };
    esp_timer_handle_t nvs_update_timer;
    ESP_ERROR_CHECK(esp_timer_create(&nvs_update_timer_args, &nvs_update_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(nvs_update_timer, TIME_PERIOD));

    app_config();

    init_First_Checkpass();
    
    ESP_LOGI(TAG_CHECK, "===== Start Webserver...");
    start_webserver();
    // xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG_CHECK, "===== Start http_post =====");
    while (1){
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    // xTaskCreate(&app_ota_task, "ota_example_task", 8192, NULL, 5, NULL);
}