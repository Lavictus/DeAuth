/* Scan Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
    This example shows how to use the All Channel Scan or Fast Scan to connect
    to a Wi-Fi network.

    In the Fast Scan mode, the scan will stop as soon as the first network matching
    the SSID is found. In this mode, an application can set threshold for the
    authentication mode and the Signal strength. Networks that do not meet the
    threshold requirements will be ignored.

    In the All Channel Scan mode, the scan will end only after all the channels
    are scanned, and connection will start with the best network. The networks
    can be sorted based on Authentication Mode or Signal Strength. The priority
    for the Authentication mode is:  WPA2 > WPA > WEP > Open
*/
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_wifi_internal.h"
#include "esp_system.h"
#include <string.h>



#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 22
#define SEQNUM_OFFSET 22


static const char *TAG = "scan";

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);

uint8_t beacon_frame_raw[] = 
{
	0x80, 0x00,														// 0-1: Frame Control
	0x00, 0x00,														// 2-3: Duration
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,								// 4-9: Destination address (broadcast)
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,								// 10-15: Source address
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,								// 16-21: BSSID
	0x00, 0x00,														// 22-23: Sequence / fragment number
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,					// 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
	0x64, 0x00,														// 32-33: Beacon interval
	0x31, 0x04,														// 34-35: Capability info
	0x00, 0x00, /* FILL CONTENT HERE */								// 36-38: SSID parameter set, 0x00:length:content
	0x01, 0x08, 0x82, 0x84,	0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,		// 39-48: Supported rates
	0x03, 0x01, 0x01,												// 49-51: DS Parameter set, current channel 1 (= 0x01),
	0x05, 0x04, 0x01, 0x02, 0x00, 0x00,								// 52-57: Traffic Indication Map

};


void false_id_task(void *pvParameter)
{
	
	for (;;)
	{
		vTaskDelay(100 / portTICK_PERIOD_MS);
		char* name = "Mark_is_awesome";
		long int name_length = strlen(name);

		uint8_t beacon[200];
		memcpy(beacon, beacon_frame_raw, BEACON_SSID_OFFSET - 1);
		beacon[BEACON_SSID_OFFSET - 1] = name_length;
		memcpy(&beacon[BEACON_SSID_OFFSET], name, name_length);
		memcpy(&beacon[BEACON_SSID_OFFSET + name_length], &beacon_frame_raw[BEACON_SSID_OFFSET], sizeof(beacon_frame_raw) - BEACON_SSID_OFFSET);

		esp_wifi_80211_tx(WIFI_IF_AP, beacon, sizeof(beacon_frame_raw) + name_length, false);

	}
}


static void test_wifi_scan_all()
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_list;
    uint8_t i;
    char *authmode;

    esp_wifi_scan_get_ap_num(&ap_count);
    printf("--------scan count of AP is %d-------\n", ap_count);
    if (ap_count <= 0)
        return;

    ap_list = (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

    printf("======================================================================\n");
    printf("             SSID             |    RSSI    |           AUTH           \n");
    printf("======================================================================\n");
    for (i = 0; i < ap_count; i++)
    {
        switch (ap_list[i].authmode)
        {
        case WIFI_AUTH_OPEN:
            authmode = "WIFI_AUTH_OPEN";
            break;
        case WIFI_AUTH_WEP:
            authmode = "WIFI_AUTH_WEP";
            break;
        case WIFI_AUTH_WPA_PSK:
            authmode = "WIFI_AUTH_WPA_PSK";
            break;
        case WIFI_AUTH_WPA2_PSK:
            authmode = "WIFI_AUTH_WPA2_PSK";
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            authmode = "WIFI_AUTH_WPA_WPA2_PSK";
            break;
        default:
            authmode = "Unknown";
            break;
        }
        printf("%26.26s    |    % 4d    |    %22.22s\n", ap_list[i].ssid, ap_list[i].rssi, authmode);
    }
    free(ap_list);
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
        //ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        //ESP_LOGI(TAG, "Got IP: %s\n",
        //         ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
        //ESP_ERROR_CHECK(esp_wifi_connect());
        break;
    case SYSTEM_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");
        test_wifi_scan_all();
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,	/* 0--all channel scan */
        .show_hidden = 1,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 150,
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    while (1)
    {
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	tcpip_adapter_init();

	
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));


	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	wifi_config_t ap_config =
	{
		.ap =
		{
			.ssid = "esp32-beacon",
			.ssid_len = 0,
			.password = "markisgreat",
			.channel = 1,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.ssid_hidden = 1,
			.max_connection = 4,
			.beacon_interval = 60000
		}
	};

	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	xTaskCreate(&false_id_task, "false_id_task", 2048, NULL, 5, NULL);

    //wifi_scan();
}
