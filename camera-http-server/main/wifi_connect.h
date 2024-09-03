#if !defined(__WIFI_CONNECT__)
#define __WIFI_CONNECT__

#include "esp_err.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#define WIFI_SSID "zuoyuan"
#define WIFI_PASSWORD "13811409809"
#define WIFI_MIN_RSSI -100
#define WIFI_CONNECT_RETRY_COUNT 5
#define WIFI_NETIF_DESC "test"

void wifi_start(void);
void wifi_stop(void);
esp_err_t wifi_sta_do_connect(wifi_config_t wifi_config, bool wait);
esp_err_t wifi_sta_do_disconnect(void);
void wifi_shutdown(void);
esp_err_t wifi_connect(void);


#endif // __WIFI_CONNECT__
