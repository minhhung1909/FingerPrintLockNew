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
}

extern "C" {
    void initUART(void);
    void app_main(void);
}

/* ---------- START INCLUDE ADRUINO LIB ---------- */

#include <Arduino.h>
#include "Keypad.h"

/* ---------- END INCLUDE ADRUINO LIB ---------- */


/* ---------- START DEFINE ---------- */

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

/* ---------- END DEFINE ---------- */

/* ------------- START VARIABLE ------------- */

static const char *TAG_CHECK = "check";

const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {21, 5, 18, 19};
byte colPins[COLS] = {12, 14, 27};
static const int RX_BUF_SIZE = 1024;

/* ------------- END ------------- */


/* ---- START FUNCTION ----- */

void initUART(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
    uart_param_config(UART_NUM_2, &uart_config);
}

/* ---- END FUNCTION ----- */

/* ---------- START MAIN ---------- */
void app_main(void)
{
    // initUART();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    wifi_scan();

    wifi_init_softap();

    // Start the web server
    start_webserver();

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if (buffer_info_wifi_post[0] != '\0')
        {
            wifi_init_sta();
            buffer_info_wifi_post[0] = '\0';
            stop_webserver();
            ESP_LOGI(TAG_CHECK, "stop webserver...");
            break;
        }
    }

    // get ipv4
    /*
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(TAG_CHECK, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
    */

    xTaskCreate(&app_ota_task, "ota_example_task", 8192, NULL, 5, NULL);
}

/* ---------- END MAIN ---------- */