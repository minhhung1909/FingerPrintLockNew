#include <Adafruit_Fingerprint.h>
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Keypad.h>
#include "finger_run.h"
#include "driver/touch_sensor.h"
#include <esp_adc/adc_oneshot.h>
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"

extern "C"{
    #include "esp_adc/adc_continuous.h"
    #include <cJSON.h>
    #include <esp_http_client.h>
    #include "http_request.h"
    #include <iot_button.h>
}
static const char *TAG_FINGERPRINT = "FINGER PRINT";
static const char *TAG_BUTTON = "Matrix KeyPad";
static const char *TAG_TOUCH = "TOUCH";

char passwordUser[7] = "123456";

#define ADC1_GPIO4_CHANNEL ADC_CHANNEL_4
#define ROWS_NUM 4
#define COLS_NUM 3

button_handle_t wake_btn;
byte rowGpios[ROWS_NUM] = {39, 38, 37, 36};
byte colGpios[COLS_NUM] = {3, 8, 18};
static const char keypadMap[ROWS_NUM][COLS_NUM] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};

HardwareSerial mySerial(1);
static Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial, 1234);
int event_control_finger = -1;

Keypad keypad = Keypad(makeKeymap(keypadMap), rowGpios, colGpios, ROWS_NUM, COLS_NUM);

int search_ID_new(){
    for (int i = 1; i < MAX_STORAGE_FINGERPRINT; i++) {
        if (finger.loadModel(i) != FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "ID: %d", i);
            return i;
        }
    }
    return -1;
}

int enrollNewFingerprint() { 
    int finger_ID = search_ID_new();
    const int MAX_ATTEMPTS = 10;  // Maximum number of attempts
    int attempts = 0;

    while (attempts < MAX_ATTEMPTS) {
        if (finger.getImage() != FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Đặt ngón tay lên cảm biến không thành công.");
            attempts++;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        if (finger.image2Tz(1) != FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Chuyển đổi hình ảnh thất bại.");
            attempts++;
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG_FINGERPRINT, "Nhấc ngón tay ra và đặt lại...");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        while (finger.getImage() != FINGERPRINT_OK) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        if (finger.image2Tz(2) == FINGERPRINT_OK && finger.createModel() == FINGERPRINT_OK) {
            if (finger.storeModel(finger_ID) == FINGERPRINT_OK) {
                ESP_LOGI(TAG_FINGERPRINT, "Ghi vân tay thành công!");
                return finger_ID;
            }
        }
        
        ESP_LOGI(TAG_FINGERPRINT, "Thất bại, thử lại... ");
        attempts++;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG_FINGERPRINT, "Timeout.");
    return -1;
}

void check_password_Finger() {
    const int TIMEOUT_SECONDS = 2; 
    uint32_t startTime = millis();

    while(1) {
        if ((millis() - startTime) > (TIMEOUT_SECONDS * 1000)) {
            ESP_LOGI(TAG_FINGERPRINT, "Hết thời gian chờ");
            finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_RED);
            return;
        }

        if (finger.getImage() == FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Đã phát hiện ngón tay.");
            if (finger.image2Tz() == FINGERPRINT_OK && finger.fingerSearch() == FINGERPRINT_OK) {
                ESP_LOGI(TAG_FINGERPRINT, "Tìm thấy vân tay ID là: %d", finger.fingerID);
                finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
                return;
            } else {
                ESP_LOGI(TAG_FINGERPRINT, "Không tìm thấy vân tay trong bộ nhớ.");
                finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

int delete_one_finger(int finger_ID){
    if (finger.deleteModel(finger_ID) == FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Xóa vân tay thành công!");
        return 1;
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Xóa vân tay thất bại.");
        return -1;
    }
}

int edit_finger_print(int finger_ID){
    if (finger.deleteModel(finger_ID) == FINGERPRINT_OK) {
        return enrollNewFingerprint();
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Chỉnh sửa vân tay thất bại.");
        return -1;
    }
}

void taskMainFingerprint(void *param) {
    printf("Task Main Fingerprint\n");
    
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE);

    xTaskCreate(&http_get_task, "http get task", 8192, NULL, 5, NULL);

    char json_data[64];
    while (1) {
        switch (get_sign()) {
            case VERIFY_FINGERPRINT:
                ESP_LOGI(TAG_FINGERPRINT, "Đang xác thực vân tay... ");
                check_password_Finger();
                set_sign(DONE_EVENT);
                break;

            case OPEN_DOOR_KEYPAD:
                ESP_LOGI(TAG_FINGERPRINT, "Mở cửa.");
                finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
                break;

            case OPEN_DOOR:
                ESP_LOGI(TAG_FINGERPRINT, "Mở cửa.");
                finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
                sprintf(json_data, "{\"open_door\": \"false\"}");
                http_post(json_data, "/set_done");
                set_sign(DONE_EVENT); // Accounce done for get event new
                break;

            case DELETE_FINGERPRINT_WITH_ID:
                if (delete_one_finger(get_id_finger()) == -1) {
                    ESP_LOGI(TAG_FINGERPRINT, "Không thể xóa vân tay.");
                }
                set_sign(DONE_EVENT);
                sprintf(json_data, "{\"deleteoneFinger\": \"false\", \"done\": \"True\"}");
                http_post(json_data, "/set_done");

                sprintf(json_data, "{\"deleteoneFinger\": \"false\", \"done\": \"True\"}");
                http_post(json_data, "/set_done");
                ESP_LOGI(TAG_FINGERPRINT, "Đã xoá được vân tay.",);
                break;

            case EDIT_FINGERPRINT:
                edit_finger_print(get_id_finger());
                set_sign(DONE_EVENT);
                break;


            case OPEN_DOOR_TOUCH:
                finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);
                vTaskDelay(5000 / portTICK_PERIOD_MS);
                finger.LEDcontrol(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE);
                printf("Open Door Touch\n");
                set_sign(DONE_EVENT);
                break;

            case ADD_NEW_FINGERPRINT:
                int finger_id_alloc =  enrollNewFingerprint();                 
                sprintf(json_data, "{\"add_Finger\": \"false\", \"done\": \"True\"}");
                http_post(json_data, "/set_done");
                if(finger_id_alloc < 0){
                    ESP_LOGI(TAG_FINGERPRINT, "Không thể thêm vân tay.");
                    set_sign(DONE_EVENT);
                    break;
                }

                sprintf(json_data, "{\"id_finger\": %d}", finger_id_alloc);
                http_post(json_data, "/enroll_finger");
                set_sign(DONE_EVENT);
                break;

        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void get_keyPad_Task(void *param){
    while (1){
        char passWord[6] = {};
        char key = keypad.getKey();
          if (key == '*') {
            ESP_LOGI(TAG_BUTTON,"Phím bấm: %c", key);
            int cnt_password = 0;
                while (cnt_password < 6)
                {
                    passWord[cnt_password] = keypad.getKey();
                    if(passWord[cnt_password]){
                        ESP_LOGW(TAG_BUTTON, "Phím bấm: %c", passWord[cnt_password]);
                        cnt_password++;
                    }
                    vTaskDelay(5/portTICK_PERIOD_MS);            
                }
                if (strcmp(passWord, "123456") == 0) {
                    printf("Open Door");
                    printf("=====\n %s \n", passWord);
                    set_sign(OPEN_DOOR);
                } else {
                    ESP_LOGI(TAG_BUTTON, "Sai mật khẩu.");
                }
            }
        vTaskDelay(20/portTICK_PERIOD_MS);
    }
}

static button_handle_t g_btns;
#define TEST_ASSERT_NOT_NULL(pointer)
void init_First_Checkpass() {
    esp_log_level_set("gpio", ESP_LOG_NONE);
    ESP_LOGI(TAG_FINGERPRINT, "Khởi động cảm biến vân tay.");

    mySerial.begin(57600, SERIAL_8N1, 17, 16);
    
    ESP_LOGI(TAG_FINGERPRINT, "Khởi động Key Pad.");
    xTaskCreate(&get_keyPad_Task, "Key Pad Task", 2048, NULL, 5, NULL);

    if (finger.verifyPassword()) {
        ESP_LOGI(TAG_FINGERPRINT, "Kết nối với cảm biến thành công.");
        // finger.emptyDatabase();
        xTaskCreate(&taskMainFingerprint, "TaskMainFingerprint", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Không thể kết nối với cảm biến.");
    }
    /*====================================================================*/
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 4,
            .active_level = 1,
        },
    };
    g_btns = iot_button_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_btns);

    while (1) {
        // button_event_t event = iot_button_get_event(g_btns);
        // if (event != BUTTON_NONE_PRESS) {
        //     ESP_LOGI(TAG_TOUCH, "event is %s", iot_button_get_event_str(event));
        // }
        // ESP_LOGI(TAG_TOUCH, "button level is %d", level);
        uint8_t level = iot_button_get_key_level(g_btns);
        if(level == 0){
            set_sign(VERIFY_FINGERPRINT);
            while(get_sign() != DONE_EVENT){
                vTaskDelay(100/portTICK_PERIOD_MS);
            }
        }
        if(get_sign() != DONE_EVENT){
            while(get_sign() != DONE_EVENT){
                vTaskDelay(100/portTICK_PERIOD_MS);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}