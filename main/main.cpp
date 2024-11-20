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
    void deleteAllFingerprints();
    void enrollNewFingerprint();
    void taskCheckKeypad(void *param);
    void taskCheckFingerprint(void *param);

}

/* ---------- START INCLUDE ADRUINO LIB ---------- */

#include <Arduino.h>
#include "Adafruit_Fingerprint.h"
#include "Keypad.h"

/* ---------- END INCLUDE ADRUINO LIB ---------- */


/* ---------- START DEFINE ---------- */

#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_16)

/* ---------- END DEFINE ---------- */

/* ------------- START VARIABLE ------------- */

static const char *TAG_CHECK = "check";
static const char *TAG_FINGERPRINT = "FINGERPRINT";

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

static uint8_t nextFingerprintID = 1; 
HardwareSerial mySerial(1);
static Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial, 1234);
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
static String password = "**"; 

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
    mySerial.begin(57600, SERIAL_8N1, 17, 16);
    ESP_LOGI(TAG_FINGERPRINT, "Start Sensor Fingerprint");
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
        vTaskDelay(100);
        if (buffer_info_wifi_post[0] != '\0')
        {
            wifi_init_sta();
            buffer_info_wifi_post[0] = '\0';
            stop_webserver();
            ESP_LOGI(TAG_CHECK, "stop webserver...");
            break;
        }
    }

    xTaskCreate(&app_ota_task, "ota_example_task", 8192, NULL, 5, NULL);
    finger.begin(57600);
    if (finger.verifyPassword()) {
        ESP_LOGI(TAG_FINGERPRINT, "Start Sensor Fingerprint Successfully");
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Cannot connect to Sensor Fingerprint");
        while (1);
    }
    // xTaskCreate(taskCheckKeypad, "taskCheckKeypad", 4096, NULL, 1, NULL); 
    // xTaskCreate(taskCheckFingerprint, "taskCheckFingerprint", 4096, NULL, 2, NULL);

    // get ipv4
    /*
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(TAG_CHECK, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
    */

}

/* ---------- END MAIN ---------- */

/* ---------- START FUNCTION ---------- */


// Hàm để xóa toàn bộ dữ liệu vân tay
void deleteAllFingerprints() {
    ESP_LOGI(TAG_FINGERPRINT, "Xóa tất cả dữ liệu vân tay trong bộ nhớ...");
    if (finger.emptyDatabase() == FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Đã xóa thành công tất cả dữ liệu vân tay.");
        nextFingerprintID = 1;
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Xóa dữ liệu vân tay thất bại.");
    }
}

// Hàm để ghi lại vân tay mới
void enrollNewFingerprint() {
    ESP_LOGI(TAG_FINGERPRINT, "Ghi lại vân tay mới với ID #: %d", nextFingerprintID);
    if (finger.getImage() != FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Đặt ngón tay lên cảm biến không thành công.");
        return;
    }

    if (finger.image2Tz(1) != FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Chuyển đổi hình ảnh thất bại.");
        return;
    }

    ESP_LOGI(TAG_FINGERPRINT, "Nhấc ngón tay ra...");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG_FINGERPRINT, "Đặt lại ngón tay...");
    while (finger.getImage() != FINGERPRINT_OK);

    if (finger.image2Tz(2) != FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Chuyển đổi hình ảnh lần thứ hai thất bại.");
        return;
    }

    if (finger.createModel() == FINGERPRINT_OK) {
        if (finger.storeModel(nextFingerprintID) == FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Ghi vân tay thành công!");
            nextFingerprintID++;
        } else {
            ESP_LOGI(TAG_FINGERPRINT, "Lưu vân tay thất bại.");
        }
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Tạo mô hình vân tay thất bại.");
    }
}

void taskCheckKeypad(void *param) {
  while (1) {
    ESP_LOGI(TAG_FINGERPRINT, "Nhấn '1' để ghi vân tay mới, '2' để xóa toàn bộ vân tay");
    char key = keypad.getKey();
    if (key) {
      if (key == '1') {
        ESP_LOGI(TAG_FINGERPRINT, "Lệnh ghi vân tay mới nhận được.");
        enrollNewFingerprint();
      } else if (key == '2') {
        ESP_LOGI(TAG_FINGERPRINT, "Lệnh xóa toàn bộ vân tay nhận được.");
        deleteAllFingerprints();
      }
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void taskCheckFingerprint(void *param) {
    while (1) {
        ESP_LOGI(TAG_FINGERPRINT, "Chờ vân tay hoặc đợi nhập mật khẩu để mở khóa");
        if (finger.getImage() == FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Đã phát hiện ngón tay.");
            if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
                ESP_LOGI(TAG_FINGERPRINT, "Tìm thấy vân tay ID #: %d", finger.fingerID);
            } else {
                ESP_LOGI(TAG_FINGERPRINT, "Không tìm thấy vân tay trong bộ nhớ.");
            }
        } else {
            unsigned long startTime = millis();
            String enteredPassword = "";
            while (millis() - startTime < 5000) {
                char key = keypad.getKey();
                if (key) {
                    enteredPassword += key;
                    ESP_LOGI(TAG_FINGERPRINT, "Đã nhập: %s", enteredPassword.c_str());
                    if (enteredPassword == password) {
                        ESP_LOGI(TAG_FINGERPRINT, "Mật khẩu đúng!");
                        break;
                    }
                }
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }

            if (enteredPassword != password) {
                ESP_LOGI(TAG_FINGERPRINT, "Mật khẩu sai hoặc không kịp thời gian");
            }
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

/* ---------- END FUNCTION ---------- */