/* MQTT publish test

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "tcpip_adapter.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "lora.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "sdkconfig.h"

#include <stdbool.h>
#include <string.h>
#include "ssd1306.h"
#include "ssd1306_draw.h"
#include "ssd1306_font.h"
#include "ssd1306_default_if.h"

static const int I2CDisplayAddress = 0x3C; // default
static const int I2CDisplayWidth = 64;
static const int I2CDisplayHeight = 32;
static const int I2CResetPin = 16;

struct SSD1306_Device I2CDisplay;

#define CONFIG_BROKER_URI                   "mqtt://192.168.1.19:1883"

#define TANK_PUBLISH_TOPIC                  "sensors/tankLvlState"
#define TANK_BATTERY_PUBLISH_TOPIC          "sensors/tankBatLvl"
#define TANK_RSSI_PUBLISH_TOPIC             "sensors/tankRssi"
#define TANK_SNR_PUBLISH_TOPIC              "sensors/tankSnr"

#define MOISTURE_PUBLISH_TOPIC              "sensors/moistureState"
#define MOISTURE_BATTERY_PUBLISH_TOPIC      "sensors/moistureBatLvl"
#define MOISTURE_RSSI_PUBLISH_TOPIC         "sensors/moistureRssi"
#define MOISTURE_SNR_PUBLISH_TOPIC          "sensors/moistureSnr"

#define SMALL_TANK_PUBLISH_TOPIC            "sensors/smallTankLvlState"
#define SMALL_TANK_BATTERY_PUBLISH_TOPIC    "sensors/smallTankBatLvl"
#define SMALL_TANK_RSSI_PUBLISH_TOPIC       "sensors/smallTankRssi"
#define SMALL_TANK_SNR_PUBLISH_TOPIC        "sensors/smallTankSnr"

#define SUBSCIBE_TOPIC                      "sensors/command" 

#define CONFIG_WIFI_SSID                    "housewifi"
#define CONFIG_WIFI_PASSWORD                "richmond15"

static const char *TAG =                    "LORA_2_MQTT_DISPLAY";

static EventGroupHandle_t mqtt_event_group;
const static int CONNECTED_BIT = BIT0;

static esp_mqtt_client_handle_t mqtt_client = NULL;

static char *expected_data = "hello";
static char *actual_data = "hello";
static size_t expected_size = 5;
static size_t expected_published = 0;
static size_t actual_published = 0;
static int qos_test = 0;
static EventGroupHandle_t wifi_event_group;

static char *largeTankLevel = "0";
static char *smallTankLevel = "0";
static char *moistureLevel = "0";
static char *largeTankBattery = "0";
static char *smallTankBattery = "0";
static char *moistureBattery = "0";
static uint8_t largeTankLevelSz = 0;
static uint8_t smallTankLevelSz = 0;
static uint8_t moistureLevelSz = 0;
static uint8_t largeTankBatterySz = 0;
static uint8_t smallTankBatterySz = 0;
static uint8_t moistureBatterySz = 0;

uint8_t buf[10];

void SetupDemo( struct SSD1306_Device* DisplayHandle, const struct SSD1306_FontDef* Font );
void SayHello( struct SSD1306_Device* DisplayHandle, const char* HelloText );

bool DefaultBusInit( void ) {
    assert( SSD1306_I2CMasterInitDefault( ) == true );
    assert( SSD1306_I2CMasterAttachDisplayDefault( &I2CDisplay, I2CDisplayWidth, I2CDisplayHeight, I2CDisplayAddress, I2CResetPin ) == true );
    return true;
}

void printToLine1of3( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
    SSD1306_SetFont( DisplayHandle, &Font_droid_sans_mono_7x13 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_West, HelloText, SSD_COLOR_WHITE );
}

void printToLine2of3( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
   SSD1306_SetFont( DisplayHandle, &Font_droid_sans_mono_7x13 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_NorthWest, HelloText, SSD_COLOR_WHITE );
}

void printToLine3of3( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
                SSD1306_SetFont( DisplayHandle, &Font_droid_sans_mono_7x13 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_SouthWest, HelloText, SSD_COLOR_WHITE );
}

void printToLine2of2( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
   SSD1306_SetFont( DisplayHandle, &Font_droid_sans_fallback_15x17 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_NorthWest, HelloText, SSD_COLOR_WHITE );
}

void printToLine1of2( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
    SSD1306_SetFont( DisplayHandle, &Font_droid_sans_fallback_15x17 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_SouthWest, HelloText, SSD_COLOR_WHITE );
}

void printToLine2of2R( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
   SSD1306_SetFont( DisplayHandle, &Font_droid_sans_fallback_15x17 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_NorthEast, HelloText, SSD_COLOR_WHITE );
}

void printToLine1of2R( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
    SSD1306_SetFont( DisplayHandle, &Font_droid_sans_fallback_15x17 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_SouthEast, HelloText, SSD_COLOR_WHITE );
}

void printToLine1of1( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
    SSD1306_SetFont( DisplayHandle, &Font_droid_sans_mono_13x24 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_North, HelloText, SSD_COLOR_WHITE );
}
void printToLine2of1( struct SSD1306_Device* DisplayHandle, const char* HelloText ) {
    SSD1306_SetFont( DisplayHandle, &Font_droid_sans_fallback_15x17 );
    SSD1306_FontDrawAnchoredString( DisplayHandle, TextAnchor_South, HelloText, SSD_COLOR_WHITE );
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    static int msg_id = 0;
    static int actual_len = 0;
    // your_context_t *context = event->context;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(mqtt_event_group, CONNECTED_BIT);
        msg_id = esp_mqtt_client_subscribe(client, SUBSCIBE_TOPIC, qos_test);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_publish(mqtt_client, PUBLISH_TOPIC, expected_data, expected_size, qos_test, 0);
        // ESP_LOGI(TAG, "[%d] Publishing...", msg_id);

        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        printf("ID=%d, total_len=%d, data_len=%d, current_data_offset=%d\n", event->msg_id, event->total_data_len, event->data_len, event->current_data_offset);
        if (event->topic) {
            actual_len = event->data_len;
            msg_id = event->msg_id;
        } else {
            actual_len += event->data_len;
            // check consisency with msg_id across multiple data events for single msg
            if (msg_id != event->msg_id) {
                ESP_LOGI(TAG, "Wrong msg_id in chunked message %d != %d", msg_id, event->msg_id);
                abort();
            }
        }
        memcpy(actual_data + event->current_data_offset, event->data, event->data_len);
        if (actual_len == event->total_data_len) {
            if (0 == memcmp(actual_data, expected_data, expected_size)) {
                printf("OK!");
                memset(actual_data, 0, expected_size);
                actual_published ++;
                if (actual_published == expected_published) {
                    printf("Correct pattern received exactly x times\n");
                    ESP_LOGI(TAG, "Test finished correctly!");
                }
            } else {
                printf("FAILED!");
                abort();
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    mqtt_event_group = xEventGroupCreate();
    const esp_mqtt_client_config_t mqtt_cfg = {
        .username = "rowdy",
        .password = "richmond",
        .uri = "mqtt://192.168.1.19",
        .port = 1883,
        .event_handle = mqtt_event_handler,
        // .cert_pem = (const char *)mqtt_eclipse_org_pem_start,
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
}


int getBufferLength(uint8_t buf[]){
    int index = 0;
    if(buf[0] > 96 && buf[0] < 123){
        index++;
        while(buf[index] > 47 && buf[index] < 58){
            index++;
        }
    }
    printf("the length of the buffer is %d\n", index);
    return index;
}

int getRssiOrSnrLength(int type,int num){
    if(type == 0){
        if(num<-99){
            return 4;
        }else{
            return 3;
        }
    } else {
        if(num>9){
            return 2;
        } else {
            return 1;
        }
    }
}


void task_rx(void *p)
{
   int x;
   for(;;) {
      lora_receive();    // put into receive mode
      while(lora_received()) {
        x = lora_receive_packet(buf, sizeof(buf));  // x is equal to the length of the message 
        buf[x] = 0; 
        char *bufStore =malloc(sizeof(buf));
        memcpy(bufStore,(const char*)buf,sizeof(buf));
        printf("Received: %s\n", buf);

        int rssi   = lora_packet_rssi();
        int snr    = lora_packet_snr();
        int rssi_length = getRssiOrSnrLength(0,rssi);
        int snr_length  = getRssiOrSnrLength(1,snr);
        char f_rssi[rssi_length+1];
        char f_snr[snr_length+1];
        snprintf(f_rssi,rssi_length+1,"%d",rssi);
        snprintf(f_snr,snr_length+1,"%d",snr);

        switch (buf[0]){
            case 116:               // 116 is for t in ascii to identify the tank water level
                largeTankLevel = (char *) bufStore;
                largeTankLevelSz = x;
                esp_mqtt_client_publish(mqtt_client, TANK_PUBLISH_TOPIC, (const char *) buf, x, qos_test, 0);   
                esp_mqtt_client_publish(mqtt_client, TANK_RSSI_PUBLISH_TOPIC, f_rssi, rssi_length, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client, TANK_SNR_PUBLISH_TOPIC, f_snr, snr_length, qos_test, 0);            
                printf("The large tank level packet RSSI is %s and the snr is %s\n",f_rssi,f_snr);
                break;
            case 98:                // 98 is for b in ascii is for battery level of tank sensor 
                largeTankBattery = (char *) bufStore;
                largeTankBatterySz = x;
                esp_mqtt_client_publish(mqtt_client, TANK_BATTERY_PUBLISH_TOPIC, (const char *) buf, x, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client, TANK_RSSI_PUBLISH_TOPIC, f_rssi, rssi_length, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client, TANK_SNR_PUBLISH_TOPIC, f_snr, snr_length, qos_test, 0);            
                printf("The large tank battery packet RSSI is %s and the snr is %s\n",f_rssi,f_snr);
                break;
            case 109:               // 109 is equal to m for moisture in ascii
                moistureLevel = (char *) bufStore;
                moistureLevelSz = x;
                esp_mqtt_client_publish(mqtt_client,  MOISTURE_PUBLISH_TOPIC, (const char *) buf, x, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client,  MOISTURE_RSSI_PUBLISH_TOPIC, f_rssi, rssi_length, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client,  MOISTURE_SNR_PUBLISH_TOPIC, f_snr, snr_length, qos_test, 0);            
                printf("The moisture level packet RSSI is %s and the snr is %s\n",f_rssi,f_snr);
                break;
            case 110:                // 110 is equal to n and is for battery in the moisture sensor.
                moistureBattery = (char *) bufStore;
                moistureBatterySz = x;
                esp_mqtt_client_publish(mqtt_client,  MOISTURE_BATTERY_PUBLISH_TOPIC, (const char *) buf, x, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client,  MOISTURE_RSSI_PUBLISH_TOPIC, f_rssi, rssi_length, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client,  MOISTURE_SNR_PUBLISH_TOPIC, f_snr, snr_length, qos_test, 0);
                printf("The moisture battery packet RSSI is %s and the snr is %s\n",f_rssi,f_snr);
                break;
            case 101:               // 109 is equal to d for moisture
                smallTankLevel = (char *) bufStore;   
                smallTankLevelSz = x;  
                esp_mqtt_client_publish(mqtt_client, SMALL_TANK_PUBLISH_TOPIC, (const char *) buf, x, qos_test, 0);   
                esp_mqtt_client_publish(mqtt_client, SMALL_TANK_RSSI_PUBLISH_TOPIC, f_rssi, rssi_length, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client, SMALL_TANK_SNR_PUBLISH_TOPIC, f_snr, snr_length, qos_test, 0);            
                printf("The small tank level packet RSSI is %s and the snr is %s\n",f_rssi,f_snr);
                break;
            case 100:                // 110 is equal to e and is for battery in the moisture sensor.
                smallTankBattery = (char *) bufStore;
                smallTankBatterySz = x;
                esp_mqtt_client_publish(mqtt_client, SMALL_TANK_BATTERY_PUBLISH_TOPIC, (const char *) buf, x, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client, SMALL_TANK_RSSI_PUBLISH_TOPIC, f_rssi, rssi_length, qos_test, 0);
                esp_mqtt_client_publish(mqtt_client, SMALL_TANK_SNR_PUBLISH_TOPIC, f_snr, snr_length, qos_test, 0);            
                printf("The small tank battery packet RSSI is %s and the snr is %s\n",f_rssi,f_snr);
                break;
            default:
            // default statements
            break;
        }

        lora_receive();
      }
      vTaskDelay(1);
   }
}

void app_main()
{

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();
    mqtt_app_start();   

    lora_init();
    lora_set_frequency(917e6);
    lora_enable_crc();
    xTaskCreate(&task_rx, "task_rx", 2048, NULL, 5, NULL);     

    printf( "Lora Successfully set up...\n" );
    
    if ( DefaultBusInit( ) != true ) {
        printf( "SSD1306 I2C BUS Init didn't work. so will try again in 5 seconds...\n" );
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    printf( "SSD1306 I2C BUS Init lookin good...\n" );
    printf( "Ready to receive and draw.\n" );

    for(;;){  
    char *display_buffer;

    display_buffer = malloc(sizeof(smallTankLevel) + sizeof(smallTankBattery) + 1);
    SSD1306_Clear(&I2CDisplay, SSD_COLOR_BLACK);
    printToLine2of2(&I2CDisplay, (const char *) "Sml Tank");
    snprintf(display_buffer,smallTankLevel+1,"%s ",smallTankLevel);
    strcat(display_buffer,smallTankBattery);
    printToLine1of2(&I2CDisplay, (const char *) display_buffer);
    SSD1306_Update(&I2CDisplay);  
    free(display_buffer);   
    vTaskDelay(2000 / portTICK_PERIOD_MS);   

    display_buffer = malloc(sizeof(largeTankLevel) + sizeof(largeTankBattery) + 1);
    SSD1306_Clear(&I2CDisplay, SSD_COLOR_BLACK);
    printToLine2of2(&I2CDisplay, (const char *) "Lrg Tank");
    snprintf(display_buffer,largeTankLevel+1,"%s ",largeTankLevel);
    strcat(display_buffer,largeTankBattery);
    printToLine1of2(&I2CDisplay, (const char *) display_buffer);
    SSD1306_Update(&I2CDisplay);  
    free(display_buffer);   
    vTaskDelay(2000 / portTICK_PERIOD_MS);   

    display_buffer = malloc(sizeof(moistureLevel) + sizeof(moistureBattery) + 1);
    SSD1306_Clear(&I2CDisplay, SSD_COLOR_BLACK);
    printToLine2of2(&I2CDisplay, (const char *) "Moisture");
    snprintf(display_buffer,moistureLevel+1,"%s ",moistureLevel);
    strcat(display_buffer,moistureBattery);
    printToLine1of2(&I2CDisplay, (const char *) display_buffer);
    SSD1306_Update(&I2CDisplay);  
    free(display_buffer);   
    vTaskDelay(2000 / portTICK_PERIOD_MS);    

    // SSD1306_Clear( &I2CDisplay, SSD_COLOR_BLACK );
    // printToLine1of1( &I2CDisplay, "LoRa" );
    // printToLine2of1( &I2CDisplay, "gateway" );
    // SSD1306_Update( &I2CDisplay );         
    // vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

