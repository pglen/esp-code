
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       utils.h
   
   Description:     

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.1.2018     Peter Glen      Initial version.

   ======================================================================= */

// Misc defines for ESP32 projects

// Macro to initialize NVS

#define INIT_APP_NVS    {                               \
        esp_err_t retf = nvs_flash_init();              \
            if (retf == ESP_ERR_NVS_NO_FREE_PAGES) {    \
                ESP_ERROR_CHECK(nvs_flash_erase());     \
                retf = nvs_flash_init();                \
            }                                           \
            ESP_ERROR_CHECK( retf );  }                 \

// Feed watchdog
        
#define FEED_DOG vTaskDelay(10/portTICK_RATE_MS);

#ifndef TRUE
#define TRUE  (1==1)
#define FALSE (1==0)
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

// Variables exported

extern time_t lasttime;

// Misc routines for printing

int     print2log(const char *ptr, ...);
int     printauth(int auth);
int     printwifierr(int err);
int     printerr(int err);

// Parser helpers

char    *wcstrdup(const char *str);
int     parse_post(const char *buf, char *names[], char *mems[], int xlen);
char    *subst_str(char *orgx, const char *substx, const char *restr);
int     unescape_url(char *str, char *strout, int lims);

void    delayed_reboot(int wait_ms);

// Time related

time_t  internet_gettime();
int     is_ntp_inited();
char    *convday(int daynum);
char    *convmonth(int monthnum);



// EOF











