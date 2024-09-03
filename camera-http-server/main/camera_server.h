#if !defined(__CAMERA_SERVER__)
#define __CAMERA_SERVER__

#include <esp_event.h>

#define CAMERA_PIN_D0 9
#define CAMERA_PIN_D1 11
#define CAMERA_PIN_D2 12
#define CAMERA_PIN_D3 10
#define CAMERA_PIN_D4 33
#define CAMERA_PIN_D5 35
#define CAMERA_PIN_D6 36
#define CAMERA_PIN_D7 38
#define CAMERA_PIN_HREF 39
#define CAMERA_PIN_PCLK 34
#define CAMERA_PIN_PWDN 40
#define CAMERA_PIN_RESET 42
#define CAMERA_PIN_SCL 1
#define CAMERA_PIN_SDA 0
#define CAMERA_PIN_VSYNC 41
#define CAMERA_PIN_XCLK 37

esp_err_t camera_server_init();

esp_err_t camera_server_start();
void camera_server_stop();

void camera_server_destroy();


#endif // __CAMERA_SERVER__

