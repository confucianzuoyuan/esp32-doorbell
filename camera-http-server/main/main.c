#include "camera_server.h"
#include "wifi_connect.h"

void app_main(void)
{
    // 连接wifi
    wifi_connect();
    // 注册一个wifi关闭时调用的回调函数
    esp_register_shutdown_handler(&wifi_shutdown);
    // 初始化摄像头服务器
    camera_server_init();
    // 启动摄像头服务器
    camera_server_start();
}
