extern "C" {
    #include <stdio.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "driver/gpio.h"
    #include "esp_log.h"
    #include "sdkconfig.h"
    #include "esp_mac.h"
    #include "driver/uart.h"
    #include "esp_system.h"
    #include "esp_wifi.h"
    #include "lwip/sys.h"
    #include <esp_wifi.h>
    #include <esp_event.h>
    #include <esp_system.h>
    #include <nvs_flash.h>
    #include <sys/param.h>
    #include "esp_netif.h"
    #include "esp_eth.h"
    #include "esp_tls_crypto.h"
    #include <esp_http_server.h>
    #include <string.h>
    #include "lwip/err.h"
    #include "esp_http_server.h"
    #include "esp_interface.h"
    #include "esp_vfs.h"
    #include <fcntl.h>
    #include "esp_chip_info.h"
    #include "esp_random.h"
    #include "cJSON.h"
    #include <string.h>
    #include "esp_log.h"
    #include "sdkconfig.h"
    #include "sdmmc_cmd.h"
    #include "lwip/apps/netbiosns.h"
    #include "mdns.h"
    #include "esp_spiffs.h"
    #include "esp_event.h"
    #include "lwip/err.h"
    #include "lwip/sys.h"
    #include "esp_spi_flash.h"
    #include "nvs_flash.h"
    #undef IPADDR_NONE
    #include "lwip/ip4_addr.h"
    #include "app_http_server.h"
}

extern "C" {
    void app_main(void);
    void initUART(void);

    static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    void wifi_init_softap(void);
}

/*THIS IS A INCLUDE*/
#undef B0
#undef B110
#undef B1000000
#include <Arduino.h>
#include "Keypad.h"

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

#define ESP_WIFI_SSID      "ESP32"
#define ESP_WIFI_PASS      "123456789"
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       10

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)
#define SCRATCH_BUFSIZE (10240)
#define CONFIG_EXAMPLE_MDNS_HOST_NAME "demo-server"
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
/* END INCLUDE */

/* --------------- BEGIN CLASS --------------- */

/* ---------------- END CLASS ---------------- */

/* ------------- THIS IS A VARIABLE ------------- */
static const char *REST_TAG = "esp-rest";
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

/*--------------------------START ACCESS POINT--------------------------*/

static const char *TAG_AP = "wifi softAP";
const char *TAG_DEBUG = "----------- DEBUG: ";

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG_AP, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG_AP, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    // esp_netif_create_default_wifi_sta();

    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = MAX_STA_CONN,
            .pmf_cfg = {
                    .required = true,
            },
        },
    };
    if (strlen(ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_AP, "wifi_init_softap_STA finished. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, ESP_WIFI_PASS, ESP_WIFI_CHANNEL);
}



/*--------------------------END ACCESS POINT--------------------------*/

/*-----------------------------------------------------*/


/*---------------------------------------------------------------*/


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
    // ESP_LOGI(TAG_CHECK, "data is: %s", buffer_info_wifi_post);
    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if (buffer_info_wifi_post[0] != '\0')
        {
            ESP_LOGI(TAG_CHECK, "data is: %s", buffer_info_wifi_post);
            wifi_init_sta();
            buffer_info_wifi_post[0] = '\0';
            stop_webserver();
            ESP_LOGI(TAG_CHECK, "stop webserver...");
            break;
        }
    }
    // get ipv4 address of esp32 when connect with wifi
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(TAG_CHECK, "IP Address: " IPSTR, IP2STR(&ip_info.ip));

    


}