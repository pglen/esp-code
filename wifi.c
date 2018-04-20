 
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       wifi.c 
   
   Description:     Wifi Scan Related support modules

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.16.2018     Peter Glen      Initial version.
      0.00  mar.18.2018     Peter Glen      Remove max_scan

   ======================================================================= */

// Notes:
// 1.   Wifi firmware version: c202b34 - hangs tcp/ip for 30 sec erratically
//      ESP-IDF v3.1-dev-562-g84788230 2nd stage bootloader
//
// 2.    Wifi firmware version: ebd3e5d - same error
//       ESP-IDF v3.1-dev-402-g672f8b05 2nd stage bootloader


#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

// Networking
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

#include "utils.h"
#include "wifi.h"

// -----------------------------------------------------------------------
// Exposed variables

static int wifi_connected = false;

// Exported values
   
const int WIFI_START_BIT    = BIT0;
const int CONNECTED_BIT     = BIT1;
const int SCANNED_BIT       = BIT2;
const int AP_START_BIT      = BIT3;

EventGroupHandle_t wifi_event_group;

uint16_t num_chs = 0;
wifi_ap_record_t ap_recs[MAX_CHANNELS + 1];

char *apstr = NULL, *appass = NULL, *ststr = NULL, *stpass = NULL;
 
// Private to this module

static const char *TAG = "WiFiSubsys";
static  int inited = 0;

#if CONFIG_WIFI_ALL_CHANNEL_SCAN
#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#elif CONFIG_WIFI_FAST_SCAN
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#else
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#endif /*CONFIG_SCAN_METHOD*/

#if CONFIG_WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_WIFI_CONNECT_AP_BY_SECURITY
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#else
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#endif /*CONFIG_SORT_METHOD*/

#if CONFIG_FAST_SCAN_THRESHOLD
#define DEFAULT_RSSI CONFIG_FAST_SCAN_MINIMUM_SIGNAL
#if CONFIG_EXAMPLE_OPEN
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#elif CONFIG_EXAMPLE_WEP
#define DEFAULT_AUTHMODE WIFI_AUTH_WEP
#elif CONFIG_EXAMPLE_WPA
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA_PSK
#elif CONFIG_EXAMPLE_WPA2
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#else
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif
#else
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif /*CONFIG_FAST_SCAN_THRESHOLD*/

wifi_config_t wifi_config = {
        .sta = {
            //.ssid = DEFAULT_SSID,
            //.password = DEFAULT_PWD,
            // Porting to new location (remember com port)
            //.ssid="PG_THREE",
            .ssid="akostar",
            .password="12345678901",
            //.ssid=ssid_str,
            //.password=pass_str,
            .bssid_set=false,
            .scan_method = DEFAULT_SCAN_METHOD,
            .sort_method = DEFAULT_SORT_METHOD,
            .threshold.rssi = DEFAULT_RSSI,
            .threshold.authmode = DEFAULT_AUTHMODE,
            },
    };
    
    wifi_config_t ap_wifi_config = {
        .ap = {
                .ssid="WiFiClock",
                .password="admin1234",
                .ssid_len = 0,
                .channel = 0,
                .authmode= WIFI_AUTH_WPA_WPA2_PSK,
                //.authmode= WIFI_AUTH_OPEN,
                .ssid_hidden = 0,
                .max_connection = 4, 
                .beacon_interval = 100,
                }
    };
    
  //////////////////////////////////////////////////////////////////////////
//
   
static esp_err_t event_handler(void *ctx, system_event_t *event)

{                      
    ESP_LOGI(TAG, "Got event: ctx -> %p event_id: %d", ctx, event->event_id);
        
    switch (event->event_id) {
    
        case SYSTEM_EVENT_STA_START:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            ESP_ERROR_CHECK(esp_wifi_connect());
            xEventGroupSetBits(wifi_event_group, WIFI_START_BIT);
            break;
            
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(TAG, "Got IP: %s",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            wifi_connected = true;
            break;
            
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        
            // Reconnect to wifi if disconnected
            //ESP_ERROR_CHECK(esp_wifi_connect());
            break;
            
        case SYSTEM_EVENT_SCAN_DONE:
            ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");
            xEventGroupSetBits(wifi_event_group, SCANNED_BIT);
            break;
        
        case SYSTEM_EVENT_AP_START:
            xEventGroupSetBits(wifi_event_group, AP_START_BIT);
            break;
                                
        case SYSTEM_EVENT_AP_STOP:
            xEventGroupClearBits(wifi_event_group, AP_START_BIT);
            break;
        
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STACONNECTED");
            
            #if 0
            // This got obsolete, TCP connection maintained across 
            // reboots and disconnects
            struct station_info stations;
            ESP_ERROR_CHECK(esp_wifi_get_station_list(&stations));
            struct station_list infoList;
            ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(stations, &infoList));
            //struct station_list *head = infoList;      
            while(infoList != NULL) {
                printf("mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x " IPSTR " %d\n",
                   infoList->mac[0],infoList->mac[1],infoList->mac[2],
                   infoList->mac[3],infoList->mac[4],infoList->mac[5],
                   IP2STR(&(infoList->ip)),
                   (uint32_t)(infoList->ip.addr));
                infoList = STAILQ_NEXT(infoList, next);
                }
            //ESP_ERROR_CHECK(esp_adapter_free_sta_list(head));
            //ESP_ERROR_CHECK(esp_wifi_free_station_list());     
            
            // Make sure that the system settles
            //vTaskDelay(100 / portTICK_PERIOD_MS);
            
            // Start DNS Server
            captdnsInit();
            ESP_LOGI(TAG, "Started DNS server."); 
            
            // Start HTTP Server
            init_httpd(event->event_info.sta_connected.aid);
            ESP_LOGI(TAG, "Started HTTP server on %d.", event->event_info.sta_connected.aid);
            #endif
            
            break;
        
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STADISCONNECTED");
            
            #if 0
            // Terminate HTTP Server and DNS server
            deinit_httpd(event->event_info.sta_disconnected.aid);
            ESP_LOGI(TAG, "Terminated HTTP server.");
            
            captdnsdeInit();
            ESP_LOGI(TAG, "Terminated DNS server on %d.", event->event_info.sta_disconnected.aid);
            #endif
            break;
                
        default:
            break;
    }
    return ESP_OK;
}
  
/* 
 * Initialize Wi-Fi as access point and station
 */

void wcwifi_init(char *ssid_str, char *pass_str, char *sta_str, char *sta_pass)

{
    if(inited == 0)
        {
        ESP_LOGI(TAG, "Please call wifi_preinit first.\n")
        return;
        }
        
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    //esp_wifi_set_ps(WIFI_PS_NONE);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    
    if(ssid_str[0] != '\0')
        {
        strncpy((char*)wifi_config.sta.ssid,  ssid_str, 
                                    sizeof(wifi_config.sta.ssid)-1);
        strncpy((char*)wifi_config.sta.password, pass_str,
                                    sizeof(wifi_config.sta.password)-1);
        }
                                        
    // Copy in AP params use mac address to make it unique
    uint8_t mac[6] = { 0 };
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    ESP_LOGI(TAG,"sta mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    // Decorate current default with mac addr[5]
    char tmp[5];
    snprintf(tmp, sizeof(tmp), "_%02x", mac[5] & 0xff);
    strcat((char*)ap_wifi_config.ap.ssid, tmp);
    ESP_LOGI(TAG,"Decorated sta ssid %s", ap_wifi_config.ap.ssid);
    
    if(sta_str[0] != '\0' && strlen(sta_pass) >= 8)
        {
        strncpy((char*)ap_wifi_config.ap.ssid,  sta_str, 
                                    sizeof(ap_wifi_config.ap.ssid)-1);
        strncpy((char*)ap_wifi_config.ap.password, sta_pass,
                                    sizeof(ap_wifi_config.ap.password)-1);
        }
    else
        {
        }
        
    //ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    
    xEventGroupWaitBits(wifi_event_group, WIFI_START_BIT,
                        false, true, portMAX_DELAY);

    inited = 2;                        
}

/* 
 * Set scan method and scan. 
 */
    
void wcwifi_scan(void)

{
    static wifi_scan_config_t scfg; 

    if(inited < 2)
        {
        ESP_LOGI(TAG, "Please call wifi_preinit and wifi_init first.\n")
        return;
        }
    
    // Prevent re-scan if the data less than 30 sec. old
    // (disabled it)
    //int ttt = time(NULL);    
    //if(ttt - old_scan < 30)
    //    return;
    //old_scan = ttt;
                    
    ESP_LOGI(TAG, "Started WiFi scan.\n");
    memset(&scfg, 0, sizeof(scfg));
    scfg.show_hidden = TRUE;
    
    xEventGroupClearBits(wifi_event_group, SCANNED_BIT);
        
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scfg, false));
    
    xEventGroupWaitBits(wifi_event_group, SCANNED_BIT,
                        false, true, portMAX_DELAY);
    
    esp_wifi_scan_get_ap_num(&num_chs);
    
    //ESP_LOGI(TAG, "Got %d channels\n", num_chs);
    memset(ap_recs, 0, sizeof(ap_recs));
    num_chs = MAX(MAX_CHANNELS, num_chs);
    ESP_ERROR_CHECK(
        esp_wifi_scan_get_ap_records(&num_chs, ap_recs));
        
    for(int loopc = 0; loopc < num_chs; loopc++)
        {
        //ESP_LOGI(TAG, "Got AP '%s' (%s) on channel %d auth: %d signal: %d ", 
        //                        ap_recs[loopc].ssid,    
        //                            ap_recs[loopc].bssid,    
        //                                ap_recs[loopc].primary,
        //                                    ap_recs[loopc].authmode,
        //                                        ap_recs[loopc].rssi
        //                                );
        //printauth(ap_recs[loopc].authmode);
        
        ESP_LOGI(TAG, "AP '%s' ch: %d ", 
                                ap_recs[loopc].ssid,    
                                        ap_recs[loopc].primary
                                        );
        // Feed watchdog
        FEED_DOG
        }
    //ESP_LOGI(TAG, "Done scan.\n");
}    

//////////////////////////////////////////////////////////////////////////    
// Read to pre-destined mallocs

void read_wifi_config()

{        
    nvs_handle my_handle;
    err_t err;
    
    if(inited == 0)
        {
        ESP_LOGI(TAG, "Warining call wifi_preinit first.\n")
        wifi_preinit();
        }
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) opening NVS handle!\n", err);
        return;
        }
    if(apstr)
        {
        apstr[0] = 0;
        unsigned int slen = PASS_SIZE;
        err = nvs_get_str(my_handle, "apname", apstr, &slen);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%d) reading apname from NVS!\n", err);
            }
        ESP_LOGI(TAG, "Config AP = '%s'\n", apstr);
        }
    if(appass)
        {    
        appass[0] = 0;
        unsigned int plen = PASS_SIZE;
        err = nvs_get_str(my_handle, "cpass", appass, &plen);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%d) reading cpass from NVS!\n", err);
            }
        ESP_LOGI(TAG, "Config PASS = '%s'\n", appass);
        }
    // ----------------------------------------------------
   if(ststr)
        {
        ststr[0] = 0;
        unsigned int stlen = PASS_SIZE;
        err = nvs_get_str(my_handle, "stname", ststr, &stlen);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%d) reading stname from NVS!\n", err);
            }
        ESP_LOGI(TAG, "Config ST = '%s'\n", ststr);
        }
   if(stpass)
        {
        stpass[0] = 0;
        unsigned int pslen = PASS_SIZE;
        err = nvs_get_str(my_handle, "spass", stpass, &pslen);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%d) reading spass from NVS!\n", err);
            }
        ESP_LOGI(TAG, "Config ST PASS = '%s'\n", stpass);
        }
    // Close
    nvs_close(my_handle);   
}

// -------------------------------------------------------------------
// Allocate stuctures etc ... call this before eny other

void wifi_preinit()

{
    // Allocate WIFI params, if NULL, routine will not use it
    apstr = malloc(PASS_SIZE + 1);  appass = malloc(PASS_SIZE + 1); 
    ststr = malloc(PASS_SIZE + 1);  stpass = malloc(PASS_SIZE + 1); 
    
    wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    
    inited = 1;
}

int is_wifi_connected()

{
    return(wifi_connected);
}
 
   
// EOF












