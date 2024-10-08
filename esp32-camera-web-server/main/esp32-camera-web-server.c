#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_system.h>
#include <nvs_flash.h>

#include "camera_pins.h"
#include "connect_wifi.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"

static const char *TAG = "esp32-cam Webserver";

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

#define CONFIG_XCLK_FREQ 20000000

static esp_err_t init_camera(void) {
  camera_config_t camera_config = {
      .pin_pwdn = CAM_PIN_PWDN,
      .pin_reset = CAM_PIN_RESET,
      .pin_xclk = CAM_PIN_XCLK,
      .pin_sccb_sda = CAM_PIN_SIOD,
      .pin_sccb_scl = CAM_PIN_SIOC,

      .pin_d7 = CAM_PIN_D7,
      .pin_d6 = CAM_PIN_D6,
      .pin_d5 = CAM_PIN_D5,
      .pin_d4 = CAM_PIN_D4,
      .pin_d3 = CAM_PIN_D3,
      .pin_d2 = CAM_PIN_D2,
      .pin_d1 = CAM_PIN_D1,
      .pin_d0 = CAM_PIN_D0,
      .pin_vsync = CAM_PIN_VSYNC,
      .pin_href = CAM_PIN_HREF,
      .pin_pclk = CAM_PIN_PCLK,

      .xclk_freq_hz = 10000000,
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,

      .pixel_format = PIXFORMAT_RGB565,
      .frame_size = FRAMESIZE_240X240,

      .jpeg_quality = 12,
      .fb_count = 1,
      .fb_location = CAMERA_FB_IN_PSRAM,
      .grab_mode = CAMERA_GRAB_WHEN_EMPTY}; // CAMERA_GRAB_LATEST. Sets when
                                            // buffers should be filled
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    return err;
  }
  return ESP_OK;
}

esp_err_t jpg_stream_httpd_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len;
  uint8_t *_jpg_buf;
  char *part_buf[64];
  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  // 设置响应内容为流媒体
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    // 获取图片缓冲区
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
      break;
    }
    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      if (!jpeg_converted) {
        esp_camera_fb_return(fb);
        res = ESP_FAIL;
      }
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    // 分段将图片数据返回给浏览器
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY,
                                  strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb->format != PIXFORMAT_JPEG) {
      free(_jpg_buf);
    }
    esp_camera_fb_return(fb);
    if (res != ESP_OK) {
      break;
    }
    int64_t fr_end = esp_timer_get_time();
    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;
    frame_time /= 1000;
  }

  last_frame = 0;
  return res;
}

httpd_uri_t uri_get = {.uri = "/",
                       .method = HTTP_GET,
                       .handler = jpg_stream_httpd_handler,
                       .user_ctx = NULL};
httpd_handle_t setup_server(void) {
  // 生成http server的默认配置
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t stream_httpd = NULL;

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    // 只注册一个uri的处理逻辑，就是"172.20.10.6:80/"的处理逻辑
    httpd_register_uri_handler(stream_httpd, &uri_get);
  }

  return stream_httpd;
}

void app_main() {
  esp_err_t err;

  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  connect_wifi();

  if (wifi_connect_status) {
    // 初始化摄像头传感器
    err = init_camera();
    if (err != ESP_OK) {
      printf("err: %s\n", esp_err_to_name(err));
      return;
    }
    // 启动web server
    setup_server();
  } else {

  }
}
