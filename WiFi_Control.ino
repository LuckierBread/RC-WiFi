#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <AsyncTCP.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_camera.h"
#include "esp_timer.h"

#define CAMERA_MODEL_ESP32S3_EYE

#include "camera_pins.h"


const char* ssid = "Your SSID";
const char* password = "Your password";


// Define a structure to hold motor control commands
struct MotorCommand {
    int motorId;
    char action;
    int speed;
};

// Create a queue to hold motor control commands
QueueHandle_t motorQueue;

// Cefine motor class
class Motor {
public:
  Motor(int forwardPin, int reversePin)
      : forwardPin(forwardPin), reversePin(reversePin) {
      pinMode(forwardPin, OUTPUT);
      pinMode(reversePin, OUTPUT);
  }

  void updateMotor(char forward, int speed) {
    if (forward == 'f') {
      analogWrite(forwardPin, speed);
      analogWrite(reversePin, 0);
    } else if (forward == 'b') {
      analogWrite(forwardPin, 0);
      analogWrite(reversePin, speed);
    }
  }

  void stop() {
    analogWrite(forwardPin, 0);
    analogWrite(reversePin, 0);
  }

  void hold() {
    analogWrite(forwardPin, 255);
    analogWrite(reversePin, 255);
  }

private:
  int forwardPin;
  int reversePin;
};

// Initialize motors
Motor mBucket(19, 20);
Motor mElbow(21, 47);
Motor mShoulder(45, 0);
Motor mTrunk(35, 36);
Motor mLeftTrack(41, 42);
Motor mRightTrack(46, 3);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
//Open Up Web Socket for faster responce
AsyncWebSocket ws("/ws");

// HTML code to be served
const char* html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Robot Control</title>
    <style>
        body {
            display: flex;
            flex-direction: column;
            align-items: center;
            font-family: Arial, sans-serif;
        }
        #video-container {
            margin-bottom: 20px;
        }
        .motor-controls {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            margin-bottom: 20px;
        }
        .motor-control {
            margin: 10px;
            text-align: center;
        }
        .motor-control label {
            display: block;
            margin-bottom: 5px;
        }
    </style>
</head>
<body>
    <h1>ESP32 Robot Control</h1>
    <div id="video-container">
        <img id="videoStream" width="640" height="480" src="/stream" />
    </div>
    <div class="motor-controls">
        <div class="motor-control" id="motor1">
            <label for="motor1Speed">Bucket Speed</label>
            <input type="range" id="bucketSpeed" min="0" max="255" value="127">
        </div>
        <div class="motor-control" id="motor2">
            <label for="motor2Speed">Elbow Speed</label>
            <input type="range" id="elbowSpeed" min="0" max="255" value="127">
        </div>
        <div class="motor-control" id="motor3">
            <label for="motor3Speed">Shoulder Speed</label>
            <input type="range" id="shoulderSpeed" min="0" max="255" value="127">
        </div>
        <div class="motor-control" id="motor4">
            <label for="motor4Speed">Trunk Speed</label>
            <input type="range" id="trunkSpeed" min="0" max="255" value="127">
        </div>
        <div class="motor-control" id="motor5">
            <label for="motor5Speed">Left Track Speed</label>
            <input type="range" id="leftTrackSpeed" min="0" max="255" value="127">
        </div>
        <div class="motor-control" id="motor6">
            <label for="motor6Speed">Right Track Speed</label>
            <input type="range" id="rightTrackSpeed" min="0" max="255" value="127">
        </div>
    </div>
    <script>
        const ws = new WebSocket(`ws://${window.location.hostname}/ws`);

        function controlMotor(motorId, action, speed) {
            ws.send(`${motorId}|${action}|${speed}`);
        }

        document.addEventListener('keydown', (event) => {
            const BucketSpeed = document.getElementById('bucketSpeed').value;
            const ElbowSpeed = document.getElementById('elbowSpeed').value;
            const ShoulderSpeed = document.getElementById('shoulderSpeed').value;
            const TrunkSpeed = document.getElementById('trunkSpeed').value;
            const LeftTrackSpeed = document.getElementById('leftTrackSpeed').value;
            const RightTrackSpeed = document.getElementById('rightTrackSpeed').value;

            switch(event.key) {
                case 'w':
                    controlMotor(5, 'f', LeftTrackSpeed);
                    controlMotor(6, 'f', RightTrackSpeed);
                    break;
                case 's':
                    controlMotor(5, 'b', LeftTrackSpeed);
                    controlMotor(6, 'b', RightTrackSpeed);
                    break;
                case 'a':
                    controlMotor(5, 'b', LeftTrackSpeed);
                    controlMotor(6, 'f', RightTrackSpeed);
                    break;
                case 'd':
                    controlMotor(5, 'f', LeftTrackSpeed);
                    controlMotor(6, 'b', RightTrackSpeed);
                    break;
                case 'q':
                    controlMotor(4, 'f', TrunkSpeed);
                    break;
                case 'e':
                    controlMotor(4, 'b', TrunkSpeed);
                    break;
                case 'u':
                    controlMotor(1, 'f', BucketSpeed);
                    break;
                case 'j':
                    controlMotor(1, 'b', BucketSpeed);
                    break;
                case 'i':
                    controlMotor(2, 'f', ElbowSpeed);
                    break;
                case 'k':
                    controlMotor(2, 'b', ElbowSpeed);
                    break;
                case 'o':
                    controlMotor(3, 'f', ShoulderSpeed);
                    break;
                case 'l':
                    controlMotor(3, 'b', ShoulderSpeed);
                    break;
            }
        });

        document.addEventListener('keyup', (event) => {
            switch(event.key) {
                case 'w':
                    controlMotor(5, 's', 0); 
                    controlMotor(6, 's', 0);
                    break;
                case 's':
                    controlMotor(5, 's', 0);
                    controlMotor(6, 's', 0);
                    break;
                case 'a':
                    controlMotor(5, 's', 0); 
                    controlMotor(6, 's', 0);
                    break;
                case 'd':
                    controlMotor(5, 's', 0);
                    controlMotor(6, 's', 0);
                    break;
                case 'q':
                    controlMotor(4, 's', 0);
                    break;
                case 'e':
                    controlMotor(4, 's', 0);
                    break;
                case 'u':
                    controlMotor(1, 's', 0);
                    break;
                case 'j':
                    controlMotor(1, 's', 0);
                    break;
                case 'i':
                    controlMotor(2, 's', 0);
                    break;
                case 'k':
                    controlMotor(2, 's', 0);
                    break;
                case 'o':
                    controlMotor(3, 's', 0);
                    break;
                case 'l':
                    controlMotor(3, 's', 0);
                    break;
            }
        });

        ws.onmessage = (event) => {
            console.log('Message from server', event.data);
        };
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  int retry_count = 0;
  while (WiFi.status() != WL_CONNECTED && retry_count < 20) {
    delay(500);
    Serial.println("Connecting to WiFi...");
    retry_count++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi");
    return;
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start camera
  startCamera();
  Serial.println("Camera Started");

  // Start server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", html);
  });
  server.on("/bmp", HTTP_GET, sendBMP);
  server.on("/capture", HTTP_GET, sendJpg);
  server.on("/stream", HTTP_GET, streamJpg);
  server.on("/control", HTTP_GET, setCameraVar);
  server.on("/status", HTTP_GET, getCameraStatus);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.begin();
  Serial.println("Server started");

  //Create queue for second processor
  motorQueue = xQueueCreate(10, sizeof(MotorCommand));
  // Create a task to handle motor control on core 1
  xTaskCreatePinnedToCore(
    handleMotorControl,  // Task function
    "MotorControlTask",  // Name of the task
    10000,               // Stack size (in bytes)
    NULL,                // Parameter passed to the task
    1,                   // Priority of the task
    NULL,                // Task handle
    1                    // Core where the task should run
  );
}

void loop() {
  // Nothing needed here for AsyncWebServer
}

void startCamera()
{
  camera_config_t config;
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
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  // for larger pre-allocated frame buffer.
  /*if(psramFound()){
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }*/

    // Camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 0); // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, 0); // lower the saturation
  s->set_awb_gain(s,1); //turn on white balance
  s->set_whitebal(s,4); //set white balance to indoor
}

typedef struct {
        camera_fb_t * fb;
        size_t index;
} camera_frame_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";

static const char * JPG_CONTENT_TYPE = "image/jpeg";
static const char * BMP_CONTENT_TYPE = "image/x-windows-bmp";

class AsyncBufferResponse: public AsyncAbstractResponse {
    private:
        uint8_t * _buf;
        size_t _len;
        size_t _index;
    public:
        AsyncBufferResponse(uint8_t * buf, size_t len, const char * contentType){
            _buf = buf;
            _len = len;
            _callback = nullptr;
            _code = 200;
            _contentLength = _len;
            _contentType = contentType;
            _index = 0;
        }
        ~AsyncBufferResponse(){
            if(_buf != nullptr){
                free(_buf);
            }
        }
        bool _sourceValid() const { return _buf != nullptr; }
        virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override{
            size_t ret = _content(buf, maxLen, _index);
            if(ret != RESPONSE_TRY_AGAIN){
                _index += ret;
            }
            return ret;
        }
        size_t _content(uint8_t *buffer, size_t maxLen, size_t index){
            memcpy(buffer, _buf+index, maxLen);
            if((index+maxLen) == _len){
                free(_buf);
                _buf = nullptr;
            }
            return maxLen;
        }
};

class AsyncFrameResponse: public AsyncAbstractResponse {
    private:
        camera_fb_t * fb;
        size_t _index;
    public:
        AsyncFrameResponse(camera_fb_t * frame, const char * contentType){
            _callback = nullptr;
            _code = 200;
            _contentLength = frame->len;
            _contentType = contentType;
            _index = 0;
            fb = frame;
        }
        ~AsyncFrameResponse(){
            if(fb != nullptr){
                esp_camera_fb_return(fb);
            }
        }
        bool _sourceValid() const { return fb != nullptr; }
        virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override{
            size_t ret = _content(buf, maxLen, _index);
            if(ret != RESPONSE_TRY_AGAIN){
                _index += ret;
            }
            return ret;
        }
        size_t _content(uint8_t *buffer, size_t maxLen, size_t index){
            memcpy(buffer, fb->buf+index, maxLen);
            if((index+maxLen) == fb->len){
                esp_camera_fb_return(fb);
                fb = nullptr;
            }
            return maxLen;
        }
};

class AsyncJpegStreamResponse: public AsyncAbstractResponse {
    private:
        camera_frame_t _frame;
        size_t _index;
        size_t _jpg_buf_len;
        uint8_t * _jpg_buf;
        uint64_t lastAsyncRequest;
    public:
        AsyncJpegStreamResponse(){
            _callback = nullptr;
            _code = 200;
            _contentLength = 0;
            _contentType = STREAM_CONTENT_TYPE;
            _sendContentLength = false;
            _chunked = true;
            _index = 0;
            _jpg_buf_len = 0;
            _jpg_buf = NULL;
            lastAsyncRequest = 0;
            memset(&_frame, 0, sizeof(camera_frame_t));
        }
        ~AsyncJpegStreamResponse(){
            if(_frame.fb){
                if(_frame.fb->format != PIXFORMAT_JPEG){
                    free(_jpg_buf);
                }
                esp_camera_fb_return(_frame.fb);
            }
        }
        bool _sourceValid() const {
            return true;
        }
        virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
            size_t ret = _content(buf, maxLen, _index);
            if(ret != RESPONSE_TRY_AGAIN){
                _index += ret;
            }
            return ret;
        }
        size_t _content(uint8_t *buffer, size_t maxLen, size_t index){
            if(!_frame.fb || _frame.index == _jpg_buf_len){
                if(index && _frame.fb){
                    uint64_t end = (uint64_t)micros();
                    int fp = (end - lastAsyncRequest) / 1000;
                    log_printf("Size: %uKB, Time: %ums (%.1ffps)\n", _jpg_buf_len/1024, fp);
                    lastAsyncRequest = end;
                    if(_frame.fb->format != PIXFORMAT_JPEG){
                        free(_jpg_buf);
                    }
                    esp_camera_fb_return(_frame.fb);
                    _frame.fb = NULL;
                    _jpg_buf_len = 0;
                    _jpg_buf = NULL;
                }
                if(maxLen < (strlen(STREAM_BOUNDARY) + strlen(STREAM_PART) + strlen(JPG_CONTENT_TYPE) + 8)){
                    //log_w("Not enough space for headers");
                    return RESPONSE_TRY_AGAIN;
                }
                //get frame
                _frame.index = 0;

                _frame.fb = esp_camera_fb_get();
                if (_frame.fb == NULL) {
                    log_e("Camera frame failed");
                    return 0;
                }

                if(_frame.fb->format != PIXFORMAT_JPEG){
                    unsigned long st = millis();
                    bool jpeg_converted = frame2jpg(_frame.fb, 80, &_jpg_buf, &_jpg_buf_len);
                    if(!jpeg_converted){
                        log_e("JPEG compression failed");
                        esp_camera_fb_return(_frame.fb);
                        _frame.fb = NULL;
                        _jpg_buf_len = 0;
                        _jpg_buf = NULL;
                        return 0;
                    }
                    log_i("JPEG: %lums, %uB", millis() - st, _jpg_buf_len);
                } else {
                    _jpg_buf_len = _frame.fb->len;
                    _jpg_buf = _frame.fb->buf;
                }

                //send boundary
                size_t blen = 0;
                if(index){
                    blen = strlen(STREAM_BOUNDARY);
                    memcpy(buffer, STREAM_BOUNDARY, blen);
                    buffer += blen;
                }
                //send header
                size_t hlen = sprintf((char *)buffer, STREAM_PART, JPG_CONTENT_TYPE, _jpg_buf_len);
                buffer += hlen;
                //send frame
                hlen = maxLen - hlen - blen;
                if(hlen > _jpg_buf_len){
                    maxLen -= hlen - _jpg_buf_len;
                    hlen = _jpg_buf_len;
                }
                memcpy(buffer, _jpg_buf, hlen);
                _frame.index += hlen;
                return maxLen;
            }

            size_t available = _jpg_buf_len - _frame.index;
            if(maxLen > available){
                maxLen = available;
            }
            memcpy(buffer, _jpg_buf+_frame.index, maxLen);
            _frame.index += maxLen;

            return maxLen;
        }
};

void sendBMP(AsyncWebServerRequest *request){
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb == NULL) {
        log_e("Camera frame failed");
        request->send(501);
        return;
    }

    uint8_t * buf = NULL;
    size_t buf_len = 0;
    unsigned long st = millis();
    bool converted = frame2bmp(fb, &buf, &buf_len);
    log_i("BMP: %lums, %uB", millis() - st, buf_len);
    esp_camera_fb_return(fb);
    if(!converted){
        request->send(501);
        return;
    }

    AsyncBufferResponse * response = new AsyncBufferResponse(buf, buf_len, BMP_CONTENT_TYPE);
    if (response == NULL) {
        log_e("Response alloc failed");
        request->send(501);
        return;
    }
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}



void sendJpg(AsyncWebServerRequest *request){
    camera_fb_t * fb = esp_camera_fb_get();
    if (fb == NULL) {
        log_e("Camera frame failed");
        request->send(501);
        return;
    }

    if(fb->format == PIXFORMAT_JPEG){
        AsyncFrameResponse * response = new AsyncFrameResponse(fb, JPG_CONTENT_TYPE);
        if (response == NULL) {
            log_e("Response alloc failed");
            request->send(501);
            return;
        }
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
        return;
    }

    size_t jpg_buf_len = 0;
    uint8_t * jpg_buf = NULL;
    unsigned long st = millis();
    bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
    esp_camera_fb_return(fb);
    if(!jpeg_converted){
        log_e("JPEG compression failed: %lu", millis());
        request->send(501);
        return;
    }
    log_i("JPEG: %lums, %uB", millis() - st, jpg_buf_len);

    AsyncBufferResponse * response = new AsyncBufferResponse(jpg_buf, jpg_buf_len, JPG_CONTENT_TYPE);
    if (response == NULL) {
        log_e("Response alloc failed");
        request->send(501);
        return;
    }
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void streamJpg(AsyncWebServerRequest *request){
    AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse();
    if(!response){
        request->send(501);
        return;
    }
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void getCameraStatus(AsyncWebServerRequest *request){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    if(s == NULL){
        request->send(501);
        return;
    }
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", s->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"denoise\":%u,", s->status.denoise);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"vflip\":%u,", s->status.vflip);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u", s->status.colorbar);
    *p++ = '}';
    *p++ = 0;

    AsyncWebServerResponse * response = request->beginResponse(200, "application/json", json_response);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}

void setCameraVar(AsyncWebServerRequest *request){
    if(!request->hasArg("var") || !request->hasArg("val")){
        request->send(404);
        return;
    }
    String var = request->arg("var");
    const char * variable = var.c_str();
    int val = atoi(request->arg("val").c_str());

    sensor_t * s = esp_camera_sensor_get();
    if(s == NULL){
        request->send(501);
        return;
    }


    int res = 0;
    if(!strcmp(variable, "framesize")) res = s->set_framesize(s, (framesize_t)val);
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "sharpness")) res = s->set_sharpness(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "denoise")) res = s->set_denoise(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);

    else {
        log_e("unknown setting %s", var.c_str());
        request->send(404);
        return;
    }
    log_d("Got setting %s with value %d. Res: %d", var.c_str(), val, res);

    AsyncWebServerResponse * response = request->beginResponse(200);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
}


// This function handles the WebSocket events
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT) {
            data[len] = 0;
            String msg = (char *)data;
            Serial.println("Received message: " + msg);

            // Parse the custom protocol message
            char* token = strtok((char*)data, "|");
            int motorId = atoi(token);
            token = strtok(NULL, "|");
            char action = token[0];
            token = strtok(NULL, "|");
            int speed = atoi(token);

            // Create a MotorCommand and send it to the queue
            MotorCommand command = {motorId, action, speed};
            xQueueSend(motorQueue, &command, portMAX_DELAY);

            client->text("{\"status\":\"success\"}");
        }
    }
}

void handleMotorControl(void *parameter) {
    MotorCommand command;
    while (true) {
        if (xQueueReceive(motorQueue, &command, portMAX_DELAY)) {
            switch (command.motorId) {
                case 1:
                    if (command.action == 'f' || command.action == 'b') {
                        mBucket.updateMotor(command.action, command.speed);
                    } else {
                        mBucket.stop();
                    }
                    Serial.println("Bucket motor updated");
                    break;
                case 2:
                    if (command.action == 'f' || command.action == 'b') {
                        mElbow.updateMotor(command.action, command.speed);
                    } else {
                        mElbow.stop();
                    }
                    Serial.println("Elbow motor updated");
                    break;
                case 3:
                    if (command.action == 'f' || command.action == 'b') {
                        mShoulder.updateMotor(command.action, command.speed);
                    } else {
                        mShoulder.stop();
                    }
                    Serial.println("Shoulder motor updated");
                    break;
                case 4:
                    if (command.action == 'f' || command.action == 'b') {
                        mTrunk.updateMotor(command.action, command.speed);
                    } else {
                        mTrunk.stop();
                    }
                    Serial.println("Trunk motor updated");
                    break;
                case 5:
                    if (command.action == 'f' || command.action == 'b') {
                        mLeftTrack.updateMotor(command.action, command.speed);
                    } else {
                        mLeftTrack.stop();
                    }
                    Serial.println("Left track motor updated");
                    break;
                case 6:
                    if (command.action == 'f' || command.action == 'b') {
                        mRightTrack.updateMotor(command.action, command.speed);
                    } else {
                        mRightTrack.stop();
                    }
                    Serial.println("Right track motor updated");
                    break;
                default:
                    Serial.println("Invalid motor ID");
            }
        }
    }
}

