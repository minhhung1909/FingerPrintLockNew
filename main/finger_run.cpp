extern "C"{
    #include <esp_log.h>
}
    #include <Adafruit_Fingerprint.h>
    #include <Arduino.h>
    #include <HardwareSerial.h>
    #include "finger_run.h"


static const char *TAG_FINGERPRINT = "FINGER PRINT";

HardwareSerial mySerial(1);
static Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial, 1234);
int value_control;

int search_ID_new(){
    for (int i = 1; i < MAX_STORAGE_FINGERPRINT; i++) {
        if (finger.loadModel(i) != FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "ID: %d", i);
            return i;
        }
    }
    return -1;
}

void enrollNewFingerprint() { 
    int finger_ID = search_ID_new();
    if (finger.getImage() != FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Đặt ngón tay lên cảm biến không thành công.");
        return;
    }
    if (finger.image2Tz(1) != FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Chuyển đổi hình ảnh thất bại.");
        return;  
    }

    ESP_LOGI(TAG_FINGERPRINT, "Nhấc ngón tay ra và đặt lại...");
    vTaskDelay(500 / portTICK_PERIOD_MS);

    while (finger.getImage() != FINGERPRINT_OK);

    if (finger.image2Tz(2) == FINGERPRINT_OK && finger.createModel() == FINGERPRINT_OK) {
        if (finger.storeModel(finger_ID) == FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Ghi vân tay thành công!");
        } else {
            ESP_LOGI(TAG_FINGERPRINT, "Lưu vân tay thất bại.");
        }
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Tạo mô hình vân tay thất bại.");
    }
}

void check_password_Finger() { // ở đây phải trigger sự kiện chạm vào cảm biến.
    while(1){
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
        ESP_LOGI(TAG_FINGERPRINT, "Không kịp thời gian");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void delete_one_finger(int finger_ID){
    if (finger.deleteModel(finger_ID) == FINGERPRINT_OK) {
        ESP_LOGI(TAG_FINGERPRINT, "Xóa vân tay thành công!");
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Xóa vân tay thất bại.");
    }
}

void edit_finger_print(int finger_ID){
    if (finger.deleteModel(finger_ID) == FINGERPRINT_OK) {
        if (finger.getImage() != FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Đặt ngón tay lên cảm biến không thành công.");
            return;
        }
        if (finger.image2Tz(1) != FINGERPRINT_OK) {
            ESP_LOGI(TAG_FINGERPRINT, "Chuyển đổi hình ảnh thất bại.");
            return;  
        }

        ESP_LOGI(TAG_FINGERPRINT, "Nhấc ngón tay ra và đặt lại...");
        vTaskDelay(500 / portTICK_PERIOD_MS);

        while (finger.getImage() != FINGERPRINT_OK);

        if (finger.image2Tz(2) == FINGERPRINT_OK && finger.createModel() == FINGERPRINT_OK) {
            if (finger.storeModel(finger_ID) == FINGERPRINT_OK) {
                ESP_LOGI(TAG_FINGERPRINT, "Ghi vân tay thành công!");
            } else {
                ESP_LOGI(TAG_FINGERPRINT, "Lưu vân tay thất bại.");
            }
        } else {
            ESP_LOGI(TAG_FINGERPRINT, "Tạo mô hình vân tay thất bại.");
        }
        
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Chỉnh sửa vân tay thất bại.");
    }
}

void taskMainFingerprint(void *param) {
    int fingerID_delete = 0; // id ddeer k bij looix
    while (1) {
        switch (value_control) {
            case ADD_NEW_FINGERPRINT:
                ESP_LOGI(TAG_FINGERPRINT, "Ghi vân tay mới.");
                enrollNewFingerprint();
                break;
            
            case DELETE_ONE_FINGERPRINT:
                delete_one_finger(fingerID_delete);
                ESP_LOGI(TAG_FINGERPRINT, "Đã xoá được vân tay.",);
                break;
            
            case EDIT_FINGERPRINT:
                edit_finger_print(fingerID_delete);
                break;

            case VERIFY_FINGERPRINT:
                ESP_LOGI(TAG_FINGERPRINT, "Đang xác thực vân tay... ");
                check_password_Finger();
                break;
            
            case DELETE_ALL_FINGERPRINT:
                ESP_LOGI(TAG_FINGERPRINT, "Xóa toàn bộ vân tay.");
                // deleteAllFingerprints();
                break;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void init_First_Checkpass() {
    mySerial.begin(57600, SERIAL_8N1, 17, 16);
    ESP_LOGI(TAG_FINGERPRINT, "Khởi động cảm biến vân tay.");

    if (finger.verifyPassword()) {
        ESP_LOGI(TAG_FINGERPRINT, "Kết nối với cảm biến thành công.");
        xTaskCreate(&taskMainFingerprint, "taskMainFingerprint", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGI(TAG_FINGERPRINT, "Không thể kết nối với cảm biến.");
    }
}