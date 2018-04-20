 
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       wifi.h 
   
   Description:     Wifi Related support modules

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.16.2018     Peter Glen      Initial version.

   ======================================================================= */

#define MAX_CHANNELS 10
#define PASS_SIZE 48

typedef const char cch_t;

// Expose scan specific variables and functions

extern uint16_t num_chs;
extern wifi_ap_record_t ap_recs[];

extern wifi_config_t ap_wifi_config;
extern char *apstr, *appass, *ststr, *stpass;
        
// Expose functions

extern void wifi_preinit();
extern int is_wifi_connected();
extern void read_wifi_config();
extern void wcwifi_init(char *ssid_str, char *pass_str, char *sta_str, char *sta_pass);
extern void wcwifi_scan(void);

// EOF









