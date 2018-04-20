 
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       wcrdate.c 
   
   Description:     RDATE Server

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  apr.11.2018     Peter Glen      Initial version.

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

static const char *rdateTAG = "Rdate";

#define RETRY_COUNT 10

//////////////////////////////////////////////////////////////////////////
// rdate server context

typedef struct _rdate_server_context

{
    int             id;
    int             sockFd;
    int             lastret;
    int             serv_conn_num;
    int             terminate;
    TaskHandle_t    proc_handle;
}   rdate_server_context;

static rdate_server_context gl_rdate_context;

//////////////////////////////////////////////////////////////////////////
// As simple as that rfc 868 time

static time_t rfc868time()

{
    time_t now; time(&now);
	return now + 2208988800U;
}
    
//////////////////////////////////////////////////////////////////////////////////
// The rdate server
// Will loop until terminate variable is set.

static void rdate_server(void *pvParameters)

{
    int cnt;  err_t err;
    struct sockaddr_in server_addr,  peer_addr;
    //static int sockFd = NULL;
    rdate_server_context *sc = (rdate_server_context*)pvParameters;
 
    //--------- label -----------
  begin_1:
    
    ESP_LOGI(rdateTAG, "begin rdate server loop.");
    
    cnt = 0;
    while(true)
        {  
        if(gl_rdate_context.terminate == sc->id) goto end_1;
  
        sc->sockFd = socket(AF_INET, SOCK_STREAM, 0);
		if (sc->sockFd != -1) 
            break;
            
        ESP_LOGI(rdateTAG, "error on rdate server create socket.");
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
	server_addr.sin_port = htons(37);
	server_addr.sin_len = sizeof(server_addr);
	   
    while(true)
        {
        if(gl_rdate_context.terminate == sc->id) goto end_1;
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
            
        ESP_LOGI(rdateTAG, "error on rdate server bind. %d %d", cnt, errno);
        vTaskDelay(1000/portTICK_RATE_MS);
        }
    cnt = 0;   
    while(true)
        {
        if(gl_rdate_context.terminate == sc->id) goto end_1;
        err =  listen(sc->sockFd, 4);
        if(err !=  -1)
            {
            break;
            }
        ESP_LOGI(rdateTAG, "error on rdate server listen. %d %d", cnt, errno);
        vTaskDelay(1000/portTICK_RATE_MS);
        
        if(cnt++ > RETRY_COUNT)
            {
            if(sc->sockFd != 0)
                close(sc->sockFd);
                
            tcpip_adapter_init();
    
            goto begin_1;
            }
        }
        
    unsigned int peer_addr_size = sizeof(struct sockaddr_in);
    cnt = 0;
    
    while (true)
        {
        if(gl_rdate_context.terminate == sc->id) goto end_1;
            
        int rdate_sock = accept(sc->sockFd, (struct sockaddr *) &peer_addr,
                                    &peer_addr_size);
        if (rdate_sock != -1) 
            {
            ESP_LOGI(rdateTAG, "Got RDATE conection.");
            
            const unsigned int rtime = htonl( rfc868time() );
            if (send(rdate_sock, (const char *)&rtime, 4, 0) != 4 )
                {
                ESP_LOGI(rdateTAG, "error: could not send rdate data (4 bytes)");
                }
	        close(rdate_sock);

            ESP_LOGI(rdateTAG, "rdate server memory %d.", xPortGetFreeHeapSize());
            }
        else
            {
            ESP_LOGI(rdateTAG, "error on rdate server accept %d %d.",
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
        
    //ESP_LOGI(rdateTAG, "rdated loop ended, restarting");
    close(sc->sockFd);
    goto begin_1;
  
    //--------- label -----------
   end_1:
    close(sc->sockFd);
    
    // Termination point for the server      
    //ESP_LOGI(rdateTAG, "rdated loop terminated.");
    vTaskDelete(NULL);
}

////////////////////////////////////////////////////////////////////////
// Remember id in passed structure

void init_rdated(int id)

{
    memset(&gl_rdate_context, '\0', sizeof(gl_rdate_context));
    gl_rdate_context.id = id;
    gl_rdate_context.terminate = -1;
    xTaskCreate(&rdate_server, "rdate_server", 2048, 
                                &gl_rdate_context, 5, &gl_rdate_context.proc_handle);
                                
    //vTaskDelay(100/portTICK_RATE_MS);
}            

//////////////////////////////////////////////////////////////////////////
// Terminate with the event ID

void deinit_rdated(int id)

{
    gl_rdate_context.terminate = id;
    vTaskDelay(100/portTICK_RATE_MS);
}





