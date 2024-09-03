/* BSD Socket API Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "driver/i2s_common.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <string.h>
#include <sys/param.h>

// tcpip协议栈在嵌入式中的替代品lwip
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include <stdio.h>
#include <string.h>
// 通过idf.py menuconfig配置的宏定义在sdkconfig.h中
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_check.h"
#include "esp_system.h"
#include "example_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "sdkconfig.h"

#define PORT CONFIG_EXAMPLE_PORT
#define KEEPALIVE_IDLE CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT CONFIG_EXAMPLE_KEEPALIVE_COUNT

static const char *TAG = "example";

// i2s将声音数据发送给喇叭
static i2s_chan_handle_t tx_handle = NULL;
// i2s从麦克风采集声音数据
static i2s_chan_handle_t rx_handle = NULL;

// es8311的配置
static esp_err_t es8311_codec_init(void) {
  /* Initialize I2C peripheral */
  const i2c_config_t es_i2c_cfg = {
      .sda_io_num = I2C_SDA_IO,
      .scl_io_num = I2C_SCL_IO,
      .mode = I2C_MODE_MASTER,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 100000,
  };
  ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM, &es_i2c_cfg), TAG,
                      "config i2c failed");
  ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0),
                      TAG, "install i2c driver failed");

  /* Initialize es8311 codec */
  es8311_handle_t es_handle = es8311_create(I2C_NUM, ES8311_ADDRRES_0);
  ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, TAG, "es8311 create failed");
  const es8311_clock_config_t es_clk = {.mclk_inverted = false,
                                        .sclk_inverted = false,
                                        .mclk_from_mclk_pin = true,
                                        .mclk_frequency = EXAMPLE_MCLK_FREQ_HZ,
                                        // 设置es8311音频编码器的采样率
                                        .sample_frequency =
                                            EXAMPLE_SAMPLE_RATE};

  ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16,
                              ES8311_RESOLUTION_16));
  ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es_handle,
                                                     EXAMPLE_SAMPLE_RATE *
                                                         EXAMPLE_MCLK_MULTIPLE,
                                                     EXAMPLE_SAMPLE_RATE),
                      TAG, "set es8311 sample frequency failed");
  ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, 80, NULL), TAG,
                      "set es8311 volume failed");
  ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), TAG,
                      "set es8311 microphone failed");
  return ESP_OK;
}

// i2s的初始化
static esp_err_t i2s_driver_init(void) {
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
  // 创建i2s手法数据的通道
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                      I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = I2S_MCK_IO,
              .bclk = I2S_BCK_IO,
              .ws = I2S_WS_IO,
              .dout = I2S_DO_IO,
              .din = I2S_DI_IO,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };
  std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
  return ESP_OK;
}

static void tcp_server_task(void *pvParameters) {
  char addr_str[128];
  int addr_family = (int)pvParameters;
  int ip_protocol = 0;
  int keepAlive = 1;
  int keepIdle = KEEPALIVE_IDLE;
  int keepInterval = KEEPALIVE_INTERVAL;
  int keepCount = KEEPALIVE_COUNT;
  struct sockaddr_storage dest_addr;

#ifdef CONFIG_EXAMPLE_IPV4
  if (addr_family == AF_INET) {
    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(PORT);
    ip_protocol = IPPROTO_IP;
  }
#endif

  int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  ESP_LOGI(TAG, "Socket created");

  int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  ESP_LOGI(TAG, "Socket bound, port %d", PORT);

  err = listen(listen_sock, 1);

  char rx_buffer[4096];
  size_t bytes_written = 0;

  ESP_LOGI(TAG, "Socket listening");

  struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
  socklen_t addr_len = sizeof(source_addr);
  int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);

  // Set tcp keepalive option
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
  setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
  setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
  setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
  // Convert ip address to string
#ifdef CONFIG_EXAMPLE_IPV4
  if (source_addr.ss_family == PF_INET) {
    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str,
                sizeof(addr_str) - 1);
  }
#endif
  ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

  while (1) {
    // 清空接收缓冲区
    memset(rx_buffer, 0, 4096);
    // 将从tcp client（esp32c3）发送过来的数据存储到rx_buffer中
    recv(sock, rx_buffer, 4096, 0);
    // 将rx_buffer中的数据发送到喇叭进行播放
    i2s_channel_write(tx_handle, rx_buffer, 4096, &bytes_written, 1000);
  }

}

void app_main(void) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << 46);
  gpio_config(&io_conf);
  gpio_set_level(46, 1);

  printf("i2s es8311 codec example start\n-----------------------------\n");
  /* Initialize i2s peripheral */
  if (i2s_driver_init() != ESP_OK) {
    ESP_LOGE(TAG, "i2s driver init failed");
    abort();
  } else {
    ESP_LOGI(TAG, "i2s driver init success");
  }
  /* Initialize i2c peripheral and config es8311 codec by i2c */
  if (es8311_codec_init() != ESP_OK) {
    ESP_LOGE(TAG, "es8311 codec init failed");
    abort();
  } else {
    ESP_LOGI(TAG, "es8311 codec init success");
  }

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  /* This helper function configures Wi-Fi or Ethernet, as selected in
   * menuconfig. Read "Establishing Wi-Fi or Ethernet Connection" section in
   * examples/protocols/README.md for more information about this function.
   */
  ESP_ERROR_CHECK(example_connect());

  xTaskCreate(tcp_server_task, "tcp_server", 8192, (void *)AF_INET, 5, NULL);
}
