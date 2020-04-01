

/*

DeAuth

This project started of as a deathentication project, hence the name, that proved more difficult then I expected.
Instead I chose to make a Wifi "spammer", which uses the ESP to generate fake acces points.
This shows how easy it is to make something that looks like an acces point, without it actually being one.
Real life uses for this could be to let someone connect to you instead of the acces point they think they are connecting too,
to steal information.

~Mark

*/

// This project was made by Mark Baaij for Avans Hogeschool 
// It uses a ESP32 with built in wifi chip to send out fake Acces Point beacon frames. 
// This project started with the scan class in the ESP folder, but since that one did not work,
// it got replaced by other code.



#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_wifi_internal.h"
#include "esp_system.h"
#include <string.h>

#define AMOUNT_OF_AP 10

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 22




static const char *TAG = "scan";

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer, int len, bool en_sys_seq);


// Raw template of a beacon frame
uint8_t beacon_frame_raw[] = 
{
	0x80, 0x00,														// 0-1: Frame Control
	0x00, 0x00,														// 2-3: Duration
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,								// 4-9: Destination address (broadcast)
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,								// 10-15: Source address
	0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,								// 16-21: BSSID
	0x00, 0x00,														// 22-23: Sequence / fragment number
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,					// 24-31: Timestamp (GETS OVERWRITTEN TO 0 BY HARDWARE)
	0x64, 0x00,														// 32-33: Beacon interval
	0x31, 0x04,														// 34-35: Capability info
	0x00, 0x00,														// 36-38: SSID parameter set, 0x00:length:content
	0x01, 0x08, 0x82, 0x84,	0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24,		// 39-48: Supported rates
	0x03, 0x01, 0x01,												// 49-51: DS Parameter set, current channel 1 (= 0x01),
	0x05, 0x04, 0x01, 0x02, 0x00, 0x00,								// 52-57: Traffic Indication Map

};



// Function that creates fake Acces Point beacon frames and sends these over wifi
void false_ap_task(void *pvParameter)
{
	
	int total_count = AMOUNT_OF_AP;				//Amount of fake AP's
	char* myname = "Mark_is_awesome";			//Name of the fake AP's
	uint8_t count = 0;							
	int i = 0;
	long int name_length = strlen(myname);
	
	// Continue sending beacon frames indefinitely 
	while (true) 
	{
		do {
			
			vTaskDelay(100 / total_count / portTICK_PERIOD_MS);			
			
			// Allocate memory to add spaces to the name
			char *name = malloc(name_length + count + 1);			
			strcpy(name, myname);									
			

			// a ' ' needs to be added to make seperate acces points with the same name
			uint8_t spacecount = 0;
			for (i = name_length; spacecount <= count; i++) {		
					
					name[i] = ' ';
					spacecount++;
			}

			name[name_length + count] = '\0';						
			
			long int fullnamelength = strlen(name);
			

			uint8_t beacon[200];
			memcpy(beacon, beacon_frame_raw, BEACON_SSID_OFFSET - 1);
			beacon[BEACON_SSID_OFFSET - 1] = fullnamelength;
			memcpy(&beacon[BEACON_SSID_OFFSET], name, fullnamelength);
			memcpy(&beacon[BEACON_SSID_OFFSET + fullnamelength], &beacon_frame_raw[BEACON_SSID_OFFSET], sizeof(beacon_frame_raw) - BEACON_SSID_OFFSET);

			beacon[SRCADDR_OFFSET + 5] = count;
			beacon[BSSID_OFFSET + 5] = count;


		
			//Send frame over wifi
			esp_wifi_80211_tx(WIFI_IF_AP, beacon, sizeof(beacon_frame_raw) + fullnamelength, false);	

			count++;
			// Free previously allocated space
			free(name);		
		} while (count < total_count);	// Loop to reach targeted amount of AP's
		
		count = 0; 
	}

}

//Scan all channels for available Acces Points
static void test_wifi_scan_all()
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_list;
    uint8_t i;
    char *authmode;

	//Scan for AP's
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
		//Check AP's Authorization Mode
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
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
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

// Initialize Wi-Fi as sta and set scan method 
static void wifi_scan(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	//Configure WiFi Scan type
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

	// Initialize WiFi 
	tcpip_adapter_init();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));


	// Set the esp wifi chip to behave as a wifi Acces Point
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

	// Configure Wifi
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	// Launch false_ap_task
	xTaskCreate(&false_ap_task, "false_ap_task", 2048, NULL, 5, NULL);

	// Scan Wifi 
	//wifi_scan();
}
