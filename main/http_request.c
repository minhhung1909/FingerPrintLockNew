#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "cJSON.h"
#include "http_request.h"
#include "finger_run.h"

#define WEB_SERVER "192.168.137.1"
#define WEB_PORT "8000"
#define WEB_PATH_GET "/trigger_event_get"


static const char *TAG = "HTTP_REQUEST";
static int event_sign = -1;
static int id_finger_get = -1;

int get_sign() {
    return event_sign;
}

void set_sign(int sign) {
    event_sign = sign;
}

int get_id_finger() {
    return id_finger_get;
}

void set_id_finger(int id) {
    id_finger_get = id;
}

int get_event(char recv_buf[]) {
    cJSON *message = cJSON_Parse(recv_buf);
    ESP_LOGI(TAG, "Received JSON: %s", recv_buf);
    if(message == NULL) {
        ESP_LOGE(TAG, "Error before: [%s]", cJSON_GetErrorPtr());
    }
    cJSON *done_obj = cJSON_GetObjectItem(message, "done");

    if(cJSON_IsTrue(done_obj)) {
        ESP_LOGI(TAG, "Haven't Event");
        return -1;
    }

    cJSON *id_obj = cJSON_GetObjectItem(message, "id_finger");
    if(id_obj != NULL){
        set_id_finger(id_obj->valueint);
    }
    cJSON *openDoor_obj = cJSON_GetObjectItem(message, "open_door");
    cJSON *addFinger_obj = cJSON_GetObjectItem(message, "add_Finger");
    cJSON *changeFinger_obj = cJSON_GetObjectItem(message, "changeFinger");
    cJSON *deleteFinger_obj = cJSON_GetObjectItem(message, "deleteoneFinger");
    if(cJSON_IsTrue(openDoor_obj)){
        return OPEN_DOOR;
    }else if(cJSON_IsTrue(addFinger_obj)){
        return ADD_NEW_FINGERPRINT;
    }else if(cJSON_IsTrue(deleteFinger_obj)){
        get_id_finger();
        return DELETE_FINGERPRINT_WITH_ID;
    }else if(cJSON_IsTrue(changeFinger_obj)){
        get_id_finger();
        return EDIT_FINGERPRINT;
    }else{
        ESP_LOGE(TAG, "Event not found!");
        return 0;
    }
}

bool check_ok(char data[]){
    char data_check[2];
    int cnt = 0;
    for(int i = 0; i< strlen(data); i++){
        if(data[i] == '['){
            do{
                data_check[cnt] = data[i];
                cnt++;
                i++;
            }while (data[i] != ']');
        }
    }
    if (strcmp(data_check, "OK") == 0){
        return true;
    }
    ESP_LOGW(TAG, "Data check: %s", data_check);
    return false;
}

void http_post(char json_data[], const char api_post[]) {
    char REQUEST[512];
    
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[512];
    do {
        // Kết nối đến máy chủ
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
        if (err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if (s < 0) {
            ESP_LOGE(TAG, "Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Allocated socket");

        if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "Socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "Connected");
        freeaddrinfo(res);

        // Chuẩn bị dữ liệu JSON
        // int id_Finger = 9; // Thay bằng giá trị thực tế
        // sprintf(json_data, "{\"id_finger\": %d}", id_Finger);

        // Xác định độ dài của dữ liệu JSON
        int content_length = strlen(json_data);

        // Xây dựng yêu cầu HTTP POST
        sprintf(REQUEST, "POST %s HTTP/1.1\r\n"
                         "Host: %s:%s\r\n"
                         "Content-Type: application/json\r\n"
                         "Content-Length: %d\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "%s",
                api_post, WEB_SERVER, WEB_PORT, content_length, json_data);

        // Gửi yêu cầu
        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "Socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket send success");

        // Thiết lập thời gian chờ phản hồi
        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                       sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "Failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Set socket receiving timeout success");

        // Đọc phản hồi từ máy chủ
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            if (r > 0) {
                recv_buf[r] = '\0';
                ESP_LOGI(TAG, "Received: %s", recv_buf);
            }
        } while (r > 0);

        close(s);
    } while (check_ok(recv_buf));
    ESP_LOGI(TAG, "Done reading from socket.");
}

void http_get_task(void *pvParameters) {
    char data_receive[256];
    static const char *REQUEST = "GET " WEB_PATH_GET " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    
    while(1) {
        char recv_buf[1024];
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        if (write(s, REQUEST, strlen(REQUEST)) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        int json_len = 0;
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            
            char *start = strchr(recv_buf, '{');
            if (start) {
                char *end = strchr(start, '}');
                if (end) {
                    json_len = (end - start) + 1;
                    memcpy(data_receive, start, json_len);
                    data_receive[json_len] = '\0';
                    break;
                }
            }
        } while(r > 0);

        event_sign = get_event(data_receive);
        set_sign(event_sign);

        do{
            vTaskDelay(100 / portTICK_PERIOD_MS); //waiting for done
        }while(event_sign != -1);

        close(s);

        vTaskDelay(5000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG, "Starting again!");
    }
}
