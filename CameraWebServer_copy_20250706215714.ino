#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE  // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_CAMS3_UNIT  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

#define In1 4
#define In2 2
#define In3 14
#define In4 15
#define TrigPin 13
#define EchoPin 16
#define servoPin 12

// ===========================
// Enter your WiFi credentials
// ===========================
//const char *ssid = "**********";
//const char *password = "**********";
const char* ssid = "OnePlus Nord CE 3 Lite 5G";
const char* password = "Ishi2007";

WebServer server(80);
Servo myservo;

void startCameraServer();
void setupLedFlash(int pin);
long getDistance();


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.frame_size = FRAMESIZE_QVGA;
  //config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_RGB565) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_QVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

  startCameraServer();
  server.on("/", handleRoot);
  server.on("/forward", []() { Forward(); server.send(200, "text/plain", "Forward"); });
  server.on("/backward", []() { Backward(); server.send(200, "text/plain", "Backward"); });
  server.on("/left", []() { TurnLeft(); server.send(200, "text/plain", "Left"); });
  server.on("/right", []() { TurnRight(); server.send(200, "text/plain", "Right"); });
  server.on("/stop", []() { moveStop(); server.send(200, "text/plain", "Stopped"); });
  server.on("/distance", []() { server.send(200, "text/plain", String(getDistance())); });

  server.begin();
  Serial.println("Web server started");
}

void loop() {
  // Do nothing. Everything is done in another task by the web server
  //delay(10000);
  server.handleClient();
}

long getDistance() {
  long duration;
  digitalWrite(TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW);
  duration = pulseIn(EchoPin, HIGH, 30000);
  return (duration == 0) ? 999 : (duration / 2) / 29.1;
}

void handleRoot() {
  String page = R"rawliteral(
    <html>
    <head>
      <script>
        function sendCommand(cmd) {
          var xhttp = new XMLHttpRequest();
          xhttp.open("GET", cmd, true);
          xhttp.send();
        }
        function updateDistance() {
          var xhttp = new XMLHttpRequest();
          xhttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
              document.getElementById("distance").innerHTML = this.responseText + " cm";
            }
          };
          xhttp.open("GET", "/distance", true);
          xhttp.send();
          setTimeout(updateDistance, 1000);
        }
      </script>
    </head>
    <body onload="updateDistance()">
      <h2>ESP32 Camera & Robot Control</h2>
      <h3>Distance: <span id="distance">Loading...</span></h3>
      <img src="/stream" width="320" height="240">
      <br>
      <button onclick="sendCommand('/forward')">Forward</button>
      <button onclick="sendCommand('/left')">Left</button>
      <button onclick="sendCommand('/stop')">Stop</button>
      <button onclick="sendCommand('/right')">Right</button>
      <button onclick="sendCommand('/backward')">Backward</button>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

void moveStop() { digitalWrite(In1, LOW); digitalWrite(In2, LOW); digitalWrite(In3, LOW); digitalWrite(In4, LOW); }
void Forward() { digitalWrite(In1, LOW); digitalWrite(In2, HIGH); digitalWrite(In3, LOW); digitalWrite(In4, HIGH); }
void Backward() { digitalWrite(In1, HIGH); digitalWrite(In2, LOW); digitalWrite(In3, HIGH); digitalWrite(In4, LOW); }
void TurnRight() { digitalWrite(In1, LOW); digitalWrite(In2, HIGH); digitalWrite(In3, HIGH); digitalWrite(In4, LOW); }
void TurnLeft() { digitalWrite(In1, HIGH); digitalWrite(In2, LOW); digitalWrite(In3, LOW); digitalWrite(In4, HIGH); }

