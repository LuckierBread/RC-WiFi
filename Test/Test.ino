#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "Arduino.h"
#include "soc/soc.h"          // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
#include "esp_http_server.h"

// Replace with your network credentials
const char* ssid = "TELUSWiFi6813";
const char* password = "3Uexp9YUNQ";

#define PART_BOUNDARY "123456789000000000000987654321"
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t server = NULL;

camera_config_t config;
uint32_t frame_count = 0;
unsigned long last_fps_time = 0;

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];

    // Set response type for MJPEG stream
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    while (true) {
        // Capture frame
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            continue; // Skip to next frame
        }

        // Use JPEG buffer directly
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        // Send frame headers
        size_t hlen = snprintf((char *)part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
        res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        if (res != ESP_OK) break;

        // Send frame data
        res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        if (res != ESP_OK) break;

        // Send boundary
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK) break;

        // Return frame buffer
        esp_camera_fb_return(fb);
        fb = NULL;

        // Frame rate counter
        frame_count++;
        unsigned long current_time = millis();
        if (current_time - last_fps_time >= 1000) {
            Serial.printf("FPS: %u\n", frame_count);
            frame_count = 0;
            last_fps_time = current_time;
        }
    }

    // Cleanup on error
    if (fb) {
        esp_camera_fb_return(fb);
    }
    return res;
}

static esp_err_t root_page_handler(httpd_req_t *req) {
    const char* html_page = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <title>ESP32 Camera Control</title>
        <script>
            async function updateSettings(event) {
                event.preventDefault();
                const formData = new FormData(event.target);
                const params = new URLSearchParams(formData).toString();
                const response = await fetch(`/settings?${params}`);
                const result = await response.text();
                alert(result); // Display feedback to the user
            }
        </script>
    </head>
    <body>
        <h1>ESP32 Camera Stream</h1>
        <img src="/stream" style="width:640px; height:480px;">

        <h2>Camera Controls</h2>
        <form onsubmit="updateSettings(event)">
            <label for="brightness">Brightness:</label>
            <input type="range" id="brightness" name="brightness" min="-2" max="2" value="0"><br><br>

            <label for="contrast">Contrast:</label>
            <input type="range" id="contrast" name="contrast" min="-2" max="2" value="0"><br><br>

            <label for="saturation">Saturation:</label>
            <input type="range" id="saturation" name="saturation" min="-2" max="2" value="0"><br><br>

            <label for="effect">Effect:</label>
            <select id="effect" name="effect">
                <option value="0">Normal</option>
                <option value="1">Negative</option>
                <option value="2">Grayscale</option>
                <option value="3">Red Tint</option>
                <option value="4">Green Tint</option>
                <option value="5">Blue Tint</option>
                <option value="6">Sepia</option>
            </select><br><br>

            <input type="submit" value="Update Settings">
        </form>
    </body>
    </html>
    )rawliteral";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html_page, strlen(html_page));
}


static esp_err_t settings_handler(httpd_req_t *req) {
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[32];

        // Adjust brightness
        if (httpd_query_key_value(buf, "brightness", param, sizeof(param)) == ESP_OK) {
            int brightness = atoi(param);
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                s->set_brightness(s, brightness);
                Serial.printf("Brightness set to: %d\n", brightness);
            }
        }

        // Adjust contrast
        if (httpd_query_key_value(buf, "contrast", param, sizeof(param)) == ESP_OK) {
            int contrast = atoi(param);
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                s->set_contrast(s, contrast);
                Serial.printf("Contrast set to: %d\n", contrast);
            }
        }

        // Adjust saturation
        if (httpd_query_key_value(buf, "saturation", param, sizeof(param)) == ESP_OK) {
            int saturation = atoi(param);
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                s->set_saturation(s, saturation);
                Serial.printf("Saturation set to: %d\n", saturation);
            }
        }

        // Adjust special effect
        if (httpd_query_key_value(buf, "effect", param, sizeof(param)) == ESP_OK) {
            int effect = atoi(param);
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                s->set_special_effect(s, effect);
                Serial.printf("Effect set to: %d\n", effect);
            }
        }
    }

    httpd_resp_sendstr(req, "Settings updated");
    return ESP_OK;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    // Define the root page endpoint
    httpd_uri_t root_page_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_page_handler,
        .user_ctx  = NULL
    };

    // Define the stream endpoint
    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    // Define the settings endpoint
    httpd_uri_t settings_uri = {
        .uri       = "/settings",
        .method    = HTTP_GET,
        .handler   = settings_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&server, &config) == ESP_OK) {
        // Register handlers for endpoints
        httpd_register_uri_handler(server, &root_page_uri);
        httpd_register_uri_handler(server, &stream_uri);
        httpd_register_uri_handler(server, &settings_uri);
        Serial.println("\nHTTP server started with /, /stream, and /settings");
    } else {
        Serial.println("\nFailed to start HTTP server");
    }
}

void setup() {
    Serial.begin(115200);

    // Initial Camera configuration
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count = 1;

    // Initialize camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connected");
    Serial.printf("Stream ready at: http://%s\n", WiFi.localIP().toString().c_str());

    // Start server
    startCameraServer();
}

void loop() {
    delay(10); // Prevent watchdog reset
}

