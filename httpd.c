 
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       httpd.c 
   
   Description:     Main file

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.07.2018     Peter Glen      Initial version.

   ======================================================================= */

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
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

#include "wifi.h"
#include "utils.h"

static const char *httpTAG = "WiFiClock";

#define BUFFSIZE (8 * 1024)

// Max number of shown AP in HTML Page. Update both html page and 
// WiFi scanner.

#define MAX_SHOWN 10

// This is how we would check ... does not work (wtf)
// So make sure that is the case in code
//ct_assert(MAX_SHOWN == MAX_CHANNELS)

// How many times to attempt to retry socket ops

#define RETRY_COUNT 10

//static time_t lasttime = 0;
static int toggle = 0;
static char *nsync   = "Not Synronized";
static char *mem_err = "Cannot allocate mem for request.";

// The following strings must agree with the HTML string on page
static char *syncstr = "dd/mm/yyyy hh:mm";          
static char *timestr = "00:00";

//static char *scan_err = "Cannot allocate mem for scan result.";
        
// Hold variables here
static char xap[36], xap2[36], xap3[36], xap4[36], xap5[36], xap6[36];

// We pair up the timezone names with the modifiers. 
// Modify only together with the html_page / C_str / C_val
// Note custom is 99 (invalid value) 
// will fetch it from custom field, assume default (EST) if missing

char *tmarr[] = 
    { "HST+10", "AKST+9", "PST+8", "MST+7", "CST+6",
      "EST+5", "AST+4", "WET-0", "CET-1", "EET-2", 
      "FET-3", "CUS" 
    };    

int tmvarr[] = 
    { -10, -9, -8, -7, -6, 
      -5, -4, +0,  +1, +2, 
      +3, +99
    };

//////////////////////////////////////////////////////////////////////////
// Import HTML page strings

#include "html_page1.h"
#include "html_page2.h"
#include "html_page3.h"
#include "html_page4.h"

//////////////////////////////////////////////////////////////////////////
// HTTP server context

typedef struct _httpd_server_context

{
    int             id;
    int             sockFd;
    int             lastret;
    int             serv_conn_num;
    int             terminate;
    TaskHandle_t    proc_handle;
}   httpd_server_context;

static httpd_server_context gl_context;

//////////////////////////////////////////////////////////////////////////
// HTTP connection context
 
typedef struct _httpd_context

{
    int resp_cnt;
    int sock;
    int ret;
    int retval; 
    char *buf;   
}   httpd_context;

// The following strings are dynamically constructed resonse templates:
// (note: some are presented as-is)

const static char ssl_header[] =   
    "HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/html\r\n" \
    "Content-Length: 98\r\n\r\n" \
    "<html>\r\n" \
    "<head>\r\n" \
    "<title>OpenSSL example</title></head><body>\r\n" \
    "OpenSSL server example!\r\n" \
    "</body>\r\n" \
    "</html>\r\n" \
    "\r\n";

const static char http_html_hdr[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-type: text/html\r\n"
    "Host: http://192.168.4.1\r\n"
    "\r\n";
    
const static char http_html_hdr2[] =
    "HTTP/1.1 204 OK\r\nContent-type: text/html\r\n"
    "Host: http://192.168.4.1\r\n"
    "\r\n";
    
const static char http_html_redir[] =
     "HTTP/1.1 302 OK\r\n"
     "Location: http://192.168.4.1/page_1.html\r\n\r\n"
     "<html>\n"
     "Redirect to: http://192.168.4.1/page_1.htm"
     "</html>\n";

const static char http_html_redir2[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-type: text/html\r\n"
    "Host: http://192.168.4.1\r\n"
    "\r\n"
    "<html> <head>"
    "<meta http-equiv=refresh " 
    "content=4;url=http://192.168.4.1/page_1.htm/>"
    "</head>"
    "<body> <h1>Waiting for reboot, redirecting in 5-15 seconds...</h1>"
    "</body></html>"
    "</html>\n";

const static char http_config_head[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-type: text/html\r\n"
    "Host: http://192.168.4.1\r\n"
    "\r\n";
    
const static char http_config_ok[] =
    "<html>"
    "<title>WiFiClock Config Saved</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock Config Saved</h1>\n"
    "<tr><td colspan=2 align=center>"
    "<a href=page_1.html>Return to Home Page."
    "</center>"
    "</body?>"
    "</html>";  

const static char http_config_ok2[] =
    "<html>"
    "<title>WiFiClock AP Config Saved</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock AP Config Saved.</h1>\n"
    "<a href=page_1.html>Return to Home Page."
    "</center>"
    "</body?>"
    "</html>";  

const static char http_config_ok3[] =
    "<html>"
    "<title>WiFiClock Station Config Saved</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock Station Config Saved.</h1>\n"
    "If station name changed, you might need to reconnect from your "
    "computer under the new WiFi settings.<p>"
    "Please write down new pasword, as without it "
    "you may not be able to log in. See user manual for recovery options.<p>"
    "<a href=page_1.html>Return to Home Page."
    "</center>"
    "</body?>"
    "</html>";  

const static char http_config_ok4[] =
    "<html>"
    "<title>WiFiClock Advanced Config Saved</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock Advanced Settings Saved.</h1>\n"
    "<a href=page_1.html>Return to Home Page."
    "</center>"
    "</body?>"
    "</html>";  

const static char http_config_ok5[] =
    "<html>"
    "<title>WiFiClock Time Refresh</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock Internet Time Refresh Started.</h1>\n"
    "Refresh usually takes about a couple of seconds, and should be "
    "visible from the home screen. <p>"
    "<a href=page_1.html>Return to Home Page."
    "</center>"
    "</body?>"
    "</html>";  

const static char http_config_err3[] =
    "<html>"
    "<title>WiFiClock Station Config Saved</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock Station Config NOT Saved.</h1>\n"
    "Station name must be at least 4 characters long and "
    "password must be 8 characters long or longer.<p>"
    " <a href=page_3.html>[ Back to Station Config Page ]</a> &nbsp; " 
    " <a href=page_1.html>[ Return to Home Page ]</a<p>"
    "</center>"
    "</body?>"
    "</html>";  

const static char http_config_err4[] =
    "<html>"
    "<title>WiFiClock Station Config Saved</title>\n"
    "</head>\n"
    "<body>\n"
    "<center>"
    "<h1>WiFiClock Station Config NOT Saved.</h1>\n"
    "The two typed passwords must match.<p>"
    " <a href=page_3.html>[ Back to Station Config Page ]</a> &nbsp; " 
    " <a href=page_1.html>[ Return to Home Page ]</a<p>"
    "</center>"
    "</body?>"
    "</html>";  

// This string is the template to replace in the html string
static char *ap_src_str = "Access_Point_String_";

char *apres[] =  {
    "AP-NUL",
    "AP-ONE",
    "AP-TWO",
    "AP-THR",
    "AP-FOU",
    "AP-FIV",
    "AP-SIX",
    "AP-SEV",
    "AP-EIG",
    "AP-NIN" };

//////////////////////////////////////////////////////////////////////////
// Fill time into web page 00:00 if no good
    
void fill_time(char *mem)

{
    time_t now; time(&now); 
    struct  tm nowinfo = { 0 }, timeinfo = { 0 };;
    localtime_r(&now, &nowinfo); localtime_r(&lasttime, &timeinfo);
        
    // Valid time?
    if(timeinfo.tm_year > (2016 - 1900))
        {
        snprintf(xap, sizeof(xap), 
                    "%02d:%02d",  nowinfo.tm_hour, nowinfo.tm_min);
        snprintf(xap2, sizeof(xap2), 
        "%02d/%02d/%04d %02d:%02d",  
                timeinfo.tm_mday, timeinfo.tm_mon + 1,
                    timeinfo.tm_year + 1900,
                        timeinfo.tm_hour, timeinfo.tm_min);
        }
    else
        {
        snprintf(xap, sizeof(xap), "%02d:%02d",  0, 0);
        snprintf(xap2, sizeof(xap2), "%16s", nsync);
        }
    subst_str(mem, timestr, xap);
    subst_str(mem, syncstr, xap2);
}

//////////////////////////////////////////////////////////////////////////
// Wait until data arrives, bail out if got end of HTML or closed socket
//    

static int peekrecv(int sock, char *buf, int buflen)
              
{              
    int ret2 = 0, ret3 = 0, cnt = 0;
    
    while(true)
        {
        int ret4 = 0, cnt2 = 0;
        
        //ESP_LOGI(httpTAG, "recv data loop iter");
           
        // Try for availablity
        while(true)
            {
            if(cnt2++ > 10) 
                {
                //ESP_LOGI(httpTAG, "breaking peek loop %d", cnt2);
                break;
                }
            ret4 = recv(sock, buf + ret2, buflen - ret2, MSG_PEEK | MSG_DONTWAIT);
            //ESP_LOGI(httpTAG, "recv peek %d", ret4);
    
            if(ret4 < 0)  { goto endrec; }
            if(ret4 > 0)  break;
            vTaskDelay(100/portTICK_RATE_MS);
            }
            
        // Would not block, get it
        if(ret4 > 0)
            { 
            // Possible race condition here, not a practical concern
               
            ESP_LOGI(httpTAG, "about to block on len=%d", ret4);
            ret3 = recv(sock, buf + ret2, buflen - ret2, 0);
            ESP_LOGI(httpTAG, "block done.");
            
            if(ret3 < 0)
                {
                ret2 = ret3;
                break;
                }
            ret2 += ret3; 
            //ESP_LOGI(httpTAG, "recv data %d '%s'", ret4, buf);
            }
        
        // Early out ... got the whole buffer      
        if(strstr(buf, "</html>"))
            break;
        
        if(ret2 >= buflen)
            break;
            
        if(cnt++ > 10)
            {
            //ESP_LOGI(httpTAG, "breaking recv loop %d", cnt2);
            break;
            }
        }
        
  endrec:
  
    //ESP_LOGI(httpTAG, "recv data loop end %d bytes\n", ret2);
    return ret2;
}        

//////////////////////////////////////////////////////////////////////////
// Generate response for home page submission
                  
char *fill_resp_home(const char *buf, int *mlen)

{
    char *mem = NULL; 
    
    mem = wcstrdup(http_config_ok);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on resp home page.");
        mem = mem_err;
        goto endd6;
        }
    char *nnn[] = {"tzone=", "dst=", "bright=", "color=", "ampm=",  NULL};
    char *mmm[] = {xap, xap2, xap3, xap4, xap5, NULL};
    parse_post(buf, nnn, mmm, sizeof(xap));

    ESP_LOGI(httpTAG, "tzone='%s' dst='%s' bright='%s' color='%s'  ampm='%s' ", 
                                xap, xap2, xap3, xap4, xap5);
                                
    unescape_url(xap, xap6, sizeof(xap6));
   
    ESP_LOGI(httpTAG, "tzone unescaped='%s' ", xap6);
    
   endd6: 
    FEED_DOG
    *mlen = strlen(mem);
    return mem; 
}

//////////////////////////////////////////////////////////////////////////
// Generate response for Advanced page submission
                  
char *fill_resp_adv(const char *buf, int *mlen)

{
    char *mem = NULL; 
    
    mem = wcstrdup(http_config_ok4);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on resp adv page.");
        mem = mem_err;
        goto endd6;
        }
    char *nnn[] = {"ntpsrv=", "ntpip=", "RR=", "GG=", "BB=", "tzcust=", NULL};
    char *mmm[] = {xap, xap2, xap3, xap4, xap5, xap6, NULL};
    parse_post(buf, nnn, mmm, sizeof(xap));

    ESP_LOGI(httpTAG, "ntpsrv='%s' ntpip='%s' RR='%s'  GG='%s'  RR='%s' tzcust='%s' ", 
                                xap, xap2, xap3, xap4, xap5, xap6);
                                
    // Check parameters, fill it to ...
    if(strlen(xap6))
        {
        ESP_LOGI(httpTAG, "Custom timezone %s", xap6);
        } 
        
    if(strlen(xap2))
        {
        ESP_LOGI(httpTAG, "Custom time server %s", xap2);
        } 
   
   endd6: 
    FEED_DOG
    *mlen = strlen(mem);
    return mem; 
}

//////////////////////////////////////////////////////////////////////////
// Generate response for STA submission
                  
char *fill_resp_sta(const char *buf, int *mlen)

{
    char *mem = NULL; 
    
    mem = wcstrdup(http_config_ok3);
    if(!mem)
        {
        *mlen = 43;
        return("Cannot allocate mem for scan result.");
        }
    char *nnn[] = {"stname=", "spass=", "spass2=", NULL};
    char *mmm[] = {xap, xap2, xap3, NULL};
    parse_post(buf, nnn, mmm, sizeof(xap));

    //ESP_LOGI(httpTAG, "stname='%s' spass='%s' spass2='%s'", 
    //                    xap, xap2, xap3);
                
    // Present error string, if any
    if(strlen(xap2) < 8 || strlen(xap) < 4)
        {
        if(mem != mem_err) free (mem);
        mem = wcstrdup(http_config_err3);
        if(!mem)
            {
            *mlen = 43;
            return("Cannot allocate mem for scan result.");
            }
        }
        
    // Present error string
    if(strcmp(xap2, xap3) != 0)
        {
        if(mem != mem_err) free (mem);
        mem = wcstrdup(http_config_err4);
        if(!mem)
            {
            ESP_LOGI(httpTAG, "Malloc error on resp sta page.");
            mem = mem_err;
            goto endd3;
            }
        }
        
    #if 1
    err_t err;
    nvs_handle my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(httpTAG, "Error (%d) opening NVS handle!", err);
        }
    else
        {
        err = nvs_set_str(my_handle, "stname", xap);
        if (err != ESP_OK) {
            ESP_LOGI(httpTAG, "Error (%d) wrting stname to NVS!", err);
            nvs_close(my_handle);   
            goto endd3;
            }
        err = nvs_set_str(my_handle, "spass", xap2);
        if (err != ESP_OK) {
            ESP_LOGI(httpTAG, "Error (%d) wrting spass to NVS", err);
            nvs_close(my_handle);   
            goto endd3;
            }
        err = nvs_commit(my_handle);
        if (err != ESP_OK) {
            ESP_LOGI(httpTAG, "Error (%d)commit to NVS!", err);
            nvs_close(my_handle);   
            goto endd3;
            }
        // Close
        nvs_close(my_handle);   
        
        // All is written, reconfig station name, re-Init Wifi Subsystem
        read_wifi_config();
        ESP_LOGI(httpTAG, "'%s' '%s' '%s' '%s' ",
                                apstr, appass, ststr, stpass);
        wcwifi_init(apstr, appass, ststr, stpass);
        }
    #endif
   
  endd3: 
    FEED_DOG
    *mlen = strlen(mem);
    return mem;
}

//////////////////////////////////////////////////////////////////////////
// Generate response for AP submission
                  
char *fill_resp_ap(const char *buf, int *mlen)
                                 
{
    char *mem = NULL;
    err_t err;
    
    mem = wcstrdup(http_config_ok2);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on resp ap page.");
        *mlen = 38;
        mem = mem_err;
        goto endd4;
        }
    char *ptr = NULL;
    char *fff = strstr(buf, "apname=");
    if(fff)
        {
        //ESP_LOGI(httpTAG, "enum='%.*s'", 6, fff + 7);
        
        for(int loopc = 0; loopc < num_chs; loopc++)
            {
            //ESP_LOGI(httpTAG, "search='%.*s'", 6, apres[loopc]);
                                            
            if(strncmp(fff + 7, apres[loopc], 6) == 0)
                {
                //ESP_LOGI(httpTAG, "found=%d '%s'", loopc, apres[loopc]);
                ptr = (char*)ap_recs[loopc].ssid;
                break;
                }
            }
        }
        
    if(ptr)
        {
        //ESP_LOGI(httpTAG, "found AP='%s'", ptr);

        char *nnn[] = {"cpass=", NULL};
        char *mmm[] = {xap, NULL};
    
        parse_post(buf, nnn, mmm, sizeof(xap));

        if(mmm[0] != '\0')
            {
            //ESP_LOGI(httpTAG, "pass='%s'", mmm[0]);
            
            nvs_handle my_handle;
            err = nvs_open("storage", NVS_READWRITE, &my_handle);
            if (err != ESP_OK) {
                ESP_LOGI(httpTAG, "Error (%d) opening NVS handle", err);
                nvs_close(my_handle);   
                goto endd4;
                }
            err = nvs_set_str(my_handle, "apname", ptr);
            if (err != ESP_OK) {
                ESP_LOGI(httpTAG, "Error (%d) writing apname to NVS", err);
                nvs_close(my_handle);   
                goto endd4;
                }
            err = nvs_set_str(my_handle, "cpass", mmm[0]);
            if (err != ESP_OK) {
                ESP_LOGI(httpTAG, "Error (%d) writing cpass to NVS", err);
                nvs_close(my_handle);   
                goto endd4;
                }
            err = nvs_commit(my_handle);
            if (err != ESP_OK) {
                ESP_LOGI(httpTAG, "Error (%d) commit to NVS", err);
                nvs_close(my_handle);   
                goto endd4;
                }
            // Close
            nvs_close(my_handle);   
            
            // All is written, reconfig, re-Init Wifi Subsystem
            read_wifi_config();
            //wcwifi_init(apstr, appass, ststr, stpass);
            }
        }
   endd4:
    *mlen = strlen(mem);
    return mem;
}
    
//////////////////////////////////////////////////////////////////////////
// Substitute HOME page from stock string

char *fill_request_home(const char *buf, int *mlen)

{
    char *mem = NULL;
    mem = wcstrdup(index_html);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on home page.");
        mem = mem_err;
        goto endd9;
        }
        
    // Substitute sels
    char  *sel_str = "selected";
    //int   sel_len = strlen(sel_str);
    
    // Fill in the currently selected time zone:
    char  *oldenv = getenv("TZ");
    if(oldenv)
        {
        char *env = wcstrdup(oldenv);
        //ESP_LOGI(httpTAG, "env='%s'", env);
        
        for (int loop = 0; loop < sizeof(tmarr) / sizeof(char*); loop++)
            {
            //ESP_LOGI(httpTAG, "tmarr='%s' env='%s'", tmarr[loop], env);
            
            if(strstr(env, tmarr[loop]))
                {
                //ESP_LOGI(httpTAG, "found='%s'", tmarr[loop]);
                char *fff = strstr(mem, tmarr[loop]);
                if(fff)
                    {
                    // Look for next selected:
                    char *fff2 = strstr(fff, sel_str);
                    if(fff2)
                        {
                        //ESP_LOGI(httpTAG, "found3='%s'", fff2);
                        // NOOP 
                        }
                    }
                }
            else
                {
                // Replace any other selected with spaces
                char *eee = strstr(mem, tmarr[loop]);
                if(eee)
                    {
                    // Look for next selected:
                    char *eee2 = strstr(eee, sel_str);
                    if(eee2)
                        {
                        // Make sure this string is the same len as 'selected'.
                        snprintf(xap, sizeof(xap), "%s", "        ");      
                        memcpy(eee2, xap, strlen(xap));
                        }
                    } 
                }
            }
        free(env);
        }
    //tzset();
    fill_time(mem);

   endd9:
    FEED_DOG
    *mlen = strlen(mem);
    return mem;
}

//////////////////////////////////////////////////////////////////////////

char *fill_request_adv(const char *buf, int *mlen)

{
    char *mem = NULL;
    mem = wcstrdup(advanced_html);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on req adv page.");
        mem = mem_err; 
        goto endd8;
        }
          
    fill_time(mem);
    FEED_DOG
    
   endd8:
    *mlen = strlen(mem);
    return mem;
}

//////////////////////////////////////////////////////////////////////////
// Generate response for STA submission
                  
char *fill_request_sta(const char *buf, int *mlen)

{
    char *mem = NULL;

    mem = wcstrdup(station_html);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on req sta page.");
        mem = mem_err; 
        goto endd7;
        }
    subst_str(mem, "station_name_here", (char *)ap_wifi_config.ap.ssid);
    fill_time(mem);
        
    endd7:   
    *mlen = strlen(mem);
    return mem;
}

//////////////////////////////////////////////////////////////////////////
// Substitute AP page 

char *fill_request_ap(const char *buf, int *mlen)

{
    char *mem = NULL;
    
    mem = wcstrdup(scan_html);
    if(!mem)
        {
        ESP_LOGI(httpTAG, "Malloc error on req ap page.");
        mem = mem_err;
        goto endd5;
        }
    
    // Actual scan, exposed vars for results
    wcwifi_scan();
    
    // Output scan data
    for(int loopc = 0; loopc < num_chs; loopc++)
        {
        FEED_DOG
        snprintf(xap, sizeof(xap), "%s%d", ap_src_str, loopc);
        //ESP_LOGI(httpTAG, "apstr='%s'", xap);
        int aplen = strlen(xap);
        char *fff = strstr(mem, xap);
        if(fff)
            {
            char *ptr = (char*)ap_recs[loopc].ssid;
            //ESP_LOGI(httpTAG, "Found:'%s' ", ptr);
            int splen = strlen(ptr);
            if(splen)
                {
                snprintf(xap2, sizeof(xap2), "%*.*s", aplen, splen, ptr);
                }     
            else
                {
                snprintf(xap2, sizeof(xap2), "%*.*s", aplen, aplen, "(Hidden)");
                } 
                
            // Push it back to the active html string.
            // Use memcpy so it will not terminate target prematurely
            memcpy(fff, xap2, strlen(xap2));
            }
        else
            {
            ESP_LOGI(httpTAG, "not found apstr='%s'", xap);
            }
        }
    
    fill_time(mem);
    
  endd5:  
    FEED_DOG
    *mlen = strlen(mem);
    return mem;
}

//////////////////////////////////////////////////////////////////////////
//
// Process get request. Search for a specific mark to see if it is 
// a command. We circumvent the giant standard of URL rocessing as 
// in this embedded application does not need all the options of 
// the HTTP standard.
//

static int httpd_get(httpd_context *pctx, const char *buf)

{
    char *head_end = strchr(buf, '\r');
    if(head_end)
        *head_end = '\0';
    
    ESP_LOGI(httpTAG, "Got head: '%s' ", buf);
    
    char *found = NULL;
    
    if( (found = strstr(buf, "page_4.html")) )
        {
        int mlen = 0;
        char *mem = NULL;
        mem = fill_request_adv(buf, &mlen);
        
        send(pctx->sock, http_html_hdr, sizeof(http_html_hdr)-1, 0);
        send(pctx->sock, mem, mlen, 0);
        //send(pctx->sock, scan_html, sizeof(scan_html)-1, 0);
        if(mem != mem_err) free (mem);
        }
    else if( (found = strstr(buf, "page_3.html")) )
        {
        int mlen = 0;
        char *mem = NULL;
        mem = fill_request_sta(buf, &mlen);
        
        send(pctx->sock, http_html_hdr, sizeof(http_html_hdr)-1, 0);
        send(pctx->sock, mem, mlen, 0);
        //send(pctx->sock, scan_html, sizeof(scan_html)-1, 0);
        if(mem != mem_err) free (mem);        
        }
    else if( (found = strstr(buf, "page_2.html")) )
        {
        int mlen = 0;
        char *mem = NULL;
        mem = fill_request_ap(buf, &mlen);
        
        send(pctx->sock, http_html_hdr, sizeof(http_html_hdr)-1, 0);
        send(pctx->sock, mem, mlen, 0);
        //send(pctx->sock, scan_html, sizeof(scan_html)-1, 0);
        if(mem != mem_err) free (mem);        
        }
    else if( (found = strstr(buf, "page_1.html")) )
        {
        char *ggg = strstr(buf, "reboot=true");
        if(ggg)
            {
            send(pctx->sock, http_html_redir2, sizeof(http_html_redir2)-1, 0);
            delayed_reboot(1000);
            return 0;
            }
        
        char *hhh = strstr(buf, "refresh=true");
        if(hhh)
            {
            lasttime = internet_gettime();
            send(pctx->sock, http_html_hdr, sizeof(http_html_hdr)-1, 0);
            send(pctx->sock, http_config_ok5, sizeof(http_config_ok5)-1, 0);
            }
        else
            {
            int mlen = 0;
            char *mem = NULL;
            mem = fill_request_home(buf, &mlen);
            
            send(pctx->sock, http_html_hdr, sizeof(http_html_hdr)-1, 0);
            send(pctx->sock, mem, mlen, 0);
            if(mem != mem_err) free (mem);        
            }
        }
  else 
        {
        // Default: present redirect page
        send(pctx->sock, http_html_redir, sizeof(http_html_redir)-1, 0);
        }
    return 0;
}                

///////////////////////////////////////////////////////////////////////////
// Serve the post request:

static int httpd_post(httpd_context *pctx, const char *buf)

{
    char *found = NULL;
    
    ESP_LOGI(httpTAG, "Got POST '%s' ", buf);
    
    if( (found = strstr(buf, "color=")) ) 
        {
        //ESP_LOGI(httpTAG, "Found config command '%s' ", found);
        int mlen = 0;
        char *mem = NULL;
        mem = fill_resp_home(buf, &mlen);
        send(pctx->sock, mem, mlen, 0);
        if(mem != mem_err) free (mem);        
        }
    else if( (found = strstr(buf, "stname=")) )
        {
        //ESP_LOGI(httpTAG, "Found station config command '%s' ", found);
        int mlen = 0;
        char *mem = NULL;
        mem = fill_resp_sta(buf, &mlen);
        send(pctx->sock, mem, mlen, 0);
        if(mem != mem_err) free (mem);        
        }
    else if( (found = strstr(buf, "apname=")) )
        {
        //ESP_LOGI(httpTAG, "Found apconfig command '%s' ", found);
        int mlen = 0;
        char *mem = NULL;
        mem = fill_resp_ap(buf, &mlen);
        //send(pctx->sock, http_config_ok2, sizeof(http_config_ok2)-1, 0);
        send(pctx->sock, mem, mlen, 0);
        //send(pctx->sock, scan_html, sizeof(scan_html)-1, 0);
        if(mem != mem_err) free (mem);        
        }
    else if( (found = strstr(buf, "ntpsrv=")) )
        {
        //ESP_LOGI(httpTAG, "Found advanced config command '%s' ", found);
        int mlen = 0;
        char *mem = NULL;
        mem = fill_resp_adv(buf, &mlen);
        //send(pctx->sock, http_config_ok2, sizeof(http_config_ok2)-1, 0);
        send(pctx->sock, mem, mlen, 0);
        //send(pctx->sock, scan_html, sizeof(scan_html)-1, 0);
        if(mem != mem_err) free (mem);        
        }
    else
        {
        char *inv = 
        "Invalid config response. No matching submission string found.";
        send(pctx->sock, http_html_hdr, sizeof(http_html_hdr)-1, 0);
        send(pctx->sock, inv, strlen(inv), 0);
        }
        
    wifi_sta_list_t stalist;   
    ESP_ERROR_CHECK(esp_wifi_ap_get_sta_list(&stalist));
         
    ESP_LOGI(httpTAG, "stationCount = %d", stalist.num);
    for(int loop3 = 0; loop3 <  stalist.num; loop3++)
        {
        ESP_LOGI(httpTAG, "Station MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
                stalist.sta[loop3].mac[0],stalist.sta[loop3].mac[1], 
                        stalist.sta[loop3].mac[2], stalist.sta[loop3].mac[3],
                            stalist.sta[loop3].mac[4], stalist.sta[loop3].mac[5]);

        //ESP_LOGI(httpTAG, "Station IP: %d.%d.%d.%d", IP2STR(&(stationInfo->ip)));
        }
   return 0; 
}

//////////////////////////////////////////////////////////////////////////
// Server level 2

static void http_server_serve2(void *parm)

{
    httpd_context *pctx = parm; int cnt = 0, ret2;
    
    pctx->resp_cnt++;               // Count the number of requests:
        
    //  ESP_LOGI(httpTAG, "http_server_serve2 sock=%d", pctx->sock);
    
  again:
  
    ret2 = peekrecv(pctx->sock, pctx->buf, BUFFSIZE);
        
    if (ret2 == -1) 
        {
        ESP_LOGI(httpTAG, "could not recv data %d", errno);
        goto end3;
        }
        
    // Some clients will connect with a nul packet sent first (like wget)
    if (ret2 == 0) 
        {
        ESP_LOGI(httpTAG, "no data recieved %d", errno);
        if(cnt++ < 10)
            goto again;
        else
            goto end3;
        }
     
    // The rest of the code assumes zero termination   
    if(ret2 <= BUFFSIZE)
        pctx->buf[ret2] = '\0';    
     
    ESP_LOGI(httpTAG, "got incoming = '%.*s' ", 128, pctx->buf);

    if(strncmp(pctx->buf, "GET ", 4) == 0)
        {
        httpd_get(pctx, pctx->buf);
        }
    else if(strncmp(pctx->buf, "POST ", 5) == 0)
        {
        httpd_post(pctx, pctx->buf);
        }
    else
        {
        ESP_LOGI(httpTAG, "Unknown http command '%s' ", pctx->buf);
        }
  end3:
    pctx->ret = ret2;
    //ESP_LOGI(httpTAG, "ended worker task");
}

//////////////////////////////////////////////////////////////////////////

static void http_server_serve(void *parm)

{
    //ESP_LOGI(httpTAG, "Started worker task ...\n");
    
    httpd_context *pctx = parm; 
    
    #if 1  
    int val2 = 1;
    //setsockopt(pctx->sock, SOL_SOCKET, SO_REUSEADDR, &val2,
    //       sizeof(val2)); 
    setsockopt(pctx->sock, SOL_SOCKET, SO_KEEPALIVE, &val2,
           sizeof(val2)); 
    #endif
            
    pctx->buf = malloc(BUFFSIZE + 1);
    if(!pctx->buf)
        {
        ESP_LOGI(httpTAG, "error: cannot alloc connection memory");
        //return ret2;
        goto endd4;
        }
    while(true)
        {
        http_server_serve2(pctx);
        
        // Test if socket is still there
        char testbuf[4];
        if(pctx->sock < 0)
            {
            //ESP_LOGI(httpTAG, "Socket not there.");
            break;
            }
        int ret4 = recv(pctx->sock, testbuf, sizeof(testbuf)-1, MSG_PEEK | MSG_DONTWAIT);
        if (ret4 < 0)
            {
            //ESP_LOGI(httpTAG, "Socket closed.");
            break;
            }
        }
        
    // This lets the peer (browser) know we are done
    close(pctx->sock);
    //int cnt = 10;
   
   endd4:
   
   #if 0
   // This section is not strictly needed, we are waiting for the socket to 
   // exit from TIME_WAIT status (or from FIN_WAIT)
   while(cnt--)
        {
        //int error = 0; socklen_t len = sizeof (error);
        //int retval = getsockopt (pctx->sock, SOL_SOCKET, SO_ERROR, &error, &len);
        
        //if (retval != 0) {
            //ESP_LOGI(httpTAG, "error getting socket error code: %s\n", strerror(retval));
            //break;
        //    }
        //if (error != 0) {
            //ESP_LOGI(httpTAG, "socket error: %s\n", strerror(error));
            //break;
        //    }
            
        ESP_LOGI(httpTAG, "Hanging on .. %p", parm);
        vTaskDelay(1000/portTICK_RATE_MS);
        }
    #endif
        
    free(pctx->buf);
    
    ESP_LOGI(httpTAG, "Ended worker thread.");
    
    // It was allocated, just for this task, now it is free
    // Is the OS freeing it?
    free(pctx);
    //vTaskDelay(300/portTICK_RATE_MS);
    
    vTaskDelete( NULL );
 }            

//////////////////////////////////////////////////////////////////////////////////
// The HTTPD server
// Will loop until terminate variable is set.

static void http_server(void *pvParameters)

{
    int cnt;  err_t err;
    struct sockaddr_in server_addr,  peer_addr;
    //static int sockFd = NULL;
    httpd_server_context *sc = (httpd_server_context*)pvParameters;
 
    //--------- label -----------
  begin_1:
    
    //ESP_LOGI(httpTAG, "begin httpd server loop.");
    
    cnt = 0;
    while(true)
        {  
        if(gl_context.terminate == sc->id) goto end_1;
  
        sc->sockFd = socket(AF_INET, SOCK_STREAM, 0);
		if (sc->sockFd != -1) 
            break;
            
        ESP_LOGI(httpTAG, "error on http server create socket.");
        vTaskDelay(1000/portTICK_RATE_MS);
        
        if(cnt++ > RETRY_COUNT)
            {    
            goto begin_1;
            // This will terminate server
            //return;
            }
        }
        
    cnt = 0;
    int val = 1;
    setsockopt(sc->sockFd, SOL_SOCKET, SO_REUSEADDR, &val,
                   sizeof(val)); 
    val = 0;
    setsockopt(sc->sockFd, SOL_SOCKET, SO_LINGER, &val,
                   sizeof(val)); 
  
    memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;	   
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(80);
	server_addr.sin_len = sizeof(server_addr);
	   
    while(true)
        {
        if(gl_context.terminate == sc->id) goto end_1;
        err = bind(sc->sockFd, (struct sockaddr *)&server_addr, 
                                            sizeof(server_addr));
        if(err !=  -1)
            break;
            
        if(cnt++ > RETRY_COUNT)
            {
            if(sc->sockFd != 0)
                close(sc->sockFd);
            goto begin_1;
            }
            
        ESP_LOGI(httpTAG, "error on http server bind. %d %d", cnt, errno);
        vTaskDelay(1000/portTICK_RATE_MS);
        }
    cnt = 0;   
    while(true)
        {
        if(gl_context.terminate == sc->id) goto end_1;
        err =  listen(sc->sockFd, 4);
        if(err !=  -1)
            {
            break;
            }
        ESP_LOGI(httpTAG, "error on http server listen. %d %d", cnt, errno);
        vTaskDelay(1000/portTICK_RATE_MS);
        
        if(cnt++ > RETRY_COUNT)
            {
            if(sc->sockFd != 0)
                close(sc->sockFd);
                
            tcpip_adapter_init();
    
            goto begin_1;
            }
        }
        
    httpd_context ctx; memset(&ctx, '\0', sizeof(ctx));
    unsigned int peer_addr_size = sizeof(struct sockaddr_in);
    cnt = 0;
    
    while (true)
        {
        if(gl_context.terminate == sc->id) goto end_1;
            
        ctx.sock = accept(sc->sockFd, (struct sockaddr *) &peer_addr,
                                    &peer_addr_size);
        if (ctx.sock != -1) 
            {
            //ESP_LOGI(httpTAG, "http server accepted connection.");
            // Attempt to compansate for listen / accept race (not needed)
            //vTaskDelay(30/portTICK_RATE_MS);
        
            //struct sockaddr addr;
            //plen = sizeof(addr);
            //getpeername(int sockfd, &addr, &plen);
            //ESP_LOGI(TAG, "From IP: %s\n", 
            //    ip4addr_ntoa((ip4_addr*)&peer_addr.));
            
            // Make copy of context
            httpd_context *mypar = malloc(sizeof(httpd_context) + 2);
            if(mypar == NULL)
                {
                ESP_LOGI(httpTAG, "Cannot allocate mem (ctx).");
                close(sc->sockFd);
                goto begin_1;
                }
            memcpy(mypar, &ctx, sizeof(ctx));
            
            //ESP_ERROR_CHECK(
            //            gpio_set_level(LED_BUILTIN, toggle));
            toggle = !toggle;
            
            //ESP_LOGI(httpTAG, "sending sock %d\n", mypar->sock);
            
            xTaskCreate(&http_server_serve, 
                        "http_worker", 4096, mypar, 5, NULL);
            
            //gpio_set_level(LED_BUILTIN, 1);
            //ESP_LOGI(httpTAG, "http server closed connection.");
            ESP_LOGI(httpTAG, "http server memory %d.", xPortGetFreeHeapSize());
            }
        else
            {
            ESP_LOGI(httpTAG, "error on http server accept %d %d.",
                                                             cnt, errno);
            vTaskDelay(1000/portTICK_RATE_MS);
            if(cnt++ > RETRY_COUNT)
                {
                if(sc->sockFd != 0)
                    close(sc->sockFd);
                goto begin_1;
                }
            }
        } 
        
    //ESP_LOGI(httpTAG, "httpd loop ended, restarting");
    close(sc->sockFd);
    goto begin_1;
  
    //--------- label -----------
   end_1:
    close(sc->sockFd);
    
    // Termination point for the server      
    //ESP_LOGI(httpTAG, "httpd loop terminated.");
    vTaskDelete(NULL);
}

////////////////////////////////////////////////////////////////////////
// Remember id in passed structure

void init_httpd(int id)

{
    #if 0
    gpio_config_t gpioConfig;
    //gpioConfig.pin_bit_mask = (1 << 16) | (1 << 17);
    gpioConfig.pin_bit_mask = (1 << LED_BUILTIN);
    gpioConfig.mode = GPIO_MODE_OUTPUT;
    gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpioConfig.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&gpioConfig));
    #endif
    
    memset(&gl_context, '\0', sizeof(gl_context));
    gl_context.id = id;
    gl_context.terminate = -1;
    xTaskCreate(&http_server, "http_server", 2048, 
                                &gl_context, 5, &gl_context.proc_handle);
                                
    //vTaskDelay(100/portTICK_RATE_MS);
}            

//////////////////////////////////////////////////////////////////////////
// Terminate with the event ID

void deinit_httpd(int id)

{
    gl_context.terminate = id;
    vTaskDelay(100/portTICK_RATE_MS);
}

// EOF




