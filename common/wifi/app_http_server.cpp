extern "C" {
    #include "app_http_server.h"
    #include <esp_wifi.h>
    #include <esp_event.h>
    #include <esp_log.h>
    #include <esp_system.h>
    #include <sys/param.h>
    #include "esp_netif.h"
    #include <esp_http_server.h>
    #include <cJSON.h>
    #include <string.h>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/event_groups.h"
    #include "esp_wifi.h"
    #include "esp_log.h"
    #include "lwip/err.h"
    #include "lwip/sys.h"
    #include "app_http_server.h"
}


extern "C"{
    static esp_err_t http_get_handler(httpd_req_t *req);
    static esp_err_t http_post_handler(httpd_req_t *req);
    static esp_err_t get_wif_handler(httpd_req_t *req);
    esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err);
    void switch_set_callback(void *cb);
    void stop_webserver(void);
    void start_webserver(void);
    void wifi_scan(void);
    void stop_webserver(void);
    void switch_set_callback(void *cb);
    static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);
    void wifi_init_sta();
}

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static const char *TAG = "TAG: ";
static const char *TAG_SCAN = "scan";
char *json_string;
static httpd_handle_t server = NULL;

static switch_handle_t p_switch_handle = NULL;

wifi_info_t infoWF[DEFAULT_SCAN_LIST_SIZE];
int sizeWF;
char buffer_info_wifi_post[100] = {"\0"};
uint8_t trigger_wifi;

void wifi_scan(void){
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_LOGI(TAG_SCAN, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    sizeWF = ap_count;
    cJSON *root = cJSON_CreateObject();
    
    for (int i = 0; i < ap_count; i++) {
        strncpy(infoWF[i].ssid, (char *)ap_info[i].ssid, sizeof(infoWF[i].ssid) - 1);
        infoWF[i].ssid[sizeof(infoWF[i].ssid) - 1] = '\0'; 
        infoWF[i].rssi = (int8_t)ap_info[i].rssi;
        infoWF[i].authmode = (wifi_auth_mode_t)ap_info[i].authmode;
        cJSON *wifi_info = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_info, "ssid", (char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(wifi_info, "rssi", infoWF[i].rssi);
        char index_str[10];
        snprintf(index_str, sizeof(index_str), "%d", i);
        cJSON_AddItemToObject(root, index_str, wifi_info);
    }
    json_string = cJSON_Print(root);    
    ESP_LOGI(TAG_SCAN, "json_string: %s", json_string);
}

/* An HTTP GET handler */
static esp_err_t http_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    const char* resp_str = (const char*) index_html_start;
    size_t resp_len = index_html_end - index_html_start;
    httpd_resp_send(req, resp_str, resp_len);
    return ESP_OK;
}

static const httpd_uri_t http_get = {
    .uri       = "/get",
    .method    = HTTP_GET,
    .handler   = http_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = (void *)"Hello World!"
};

/* An HTTP GET handler */
static esp_err_t get_wif_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    return ESP_OK;
}

static const httpd_uri_t http_get_infoWF = {
    .uri       = "/wifi",
    .method    = HTTP_GET,
    .handler   = get_wif_handler,
    .user_ctx  = NULL
};
/* An HTTP POST handler */
static esp_err_t http_post_handler(httpd_req_t *req)
{
    int ret, data_len = req->content_len;
    /* Read the data for the request */
    httpd_req_recv(req, buffer_info_wifi_post, data_len);
    /* Log data received */
    ESP_LOGI(TAG, "Check first: %.*s", data_len, buffer_info_wifi_post );

    // End response
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static const httpd_uri_t http_post = {
    .uri       = "/post",
    .method    = HTTP_POST,
    .handler   = http_post_handler,
    .user_ctx  = (void *)"NULL"
};

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &http_get);
        httpd_register_uri_handler(server, &http_post);
        httpd_register_uri_handler(server, &http_get_infoWF);
    }
}

void stop_webserver(void){
    httpd_stop(server);
}

void switch_set_callback(void *cb){
    if(cb)
        p_switch_handle = (void(*) (int))cb;
}

/*--------------------------------------------------------------------*/


static const char *TAG_STATION = "wifi station";

static EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_STATION, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG_STATION,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_STATION, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(){
    ESP_LOGI(TAG_STATION, "Parsing and initializing wifi: %s", buffer_info_wifi_post);
    
    char *ssid;
    char *password;

    ssid = strtok(buffer_info_wifi_post, "|");
    password = strtok(NULL, "|");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG_STATION, "config wifi Ok.");
}