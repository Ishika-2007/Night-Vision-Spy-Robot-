#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Wi-Fi credentials
const char *ssid = "OnePlus Nord CE 3 Lite 5G";
const char *password = "Ishi2007";

// Motor driver pins
#define IN1 D0
#define IN2 D1
#define IN3 D2
#define IN4 D3
#define ENA D7
#define ENB D8

// Ultrasonic sensor pins
#define TRIG_FRONT D5
#define ECHO_FRONT D6
#define TRIG_BACK D4  // NEW ultrasonic sensor (BACK)
#define ECHO_BACK 3  // You can adjust pins according to your connections

int motorSpeed = 800;
bool isStopped = false; // Flag to know if robot stopped due to obstacle

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);

  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);
  pinMode(TRIG_BACK, OUTPUT);
  pinMode(ECHO_BACK, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/distance", handleDistance);
  server.on("/speed", handleSpeed);

  server.on("/forward", []() {
    isStopped = false;
    forward();
    server.send(204);
  });
  server.on("/backward", []() {
    isStopped = false;
    backward();
    server.send(204);
  });
  server.on("/left", []() {
    isStopped = false;
    left();
    server.send(204);
  });
  server.on("/right", []() {
    isStopped = false;
    right();
    server.send(204);
  });
  server.on("/stop", []() {
    stopMotors();
    isStopped = true;
    server.send(204);
  });

  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();

  if (!isStopped) {  // Only check if robot is moving
    long frontDist = getDistance(TRIG_FRONT, ECHO_FRONT);
    long backDist = getDistance(TRIG_BACK, ECHO_BACK);

    if ((frontDist > 0 && frontDist < 15) || (backDist > 0 && backDist < 15)) {
      Serial.println("Obstacle detected! Auto STOP!");
      stopMotors();
      isStopped = true; // Set stopped flag
    }
  }
}

// ==========================
// Distance functions
// ==========================
long getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1;

  long distance = duration * 0.034 / 2;
  return distance;
}

void handleDistance() {
  long front = getDistance(TRIG_FRONT, ECHO_FRONT);
  long back = getDistance(TRIG_BACK, ECHO_BACK);

  String distInfo = "Front: " + String(front) + " cm | Back: " + String(back) + " cm";
  server.send(200, "text/plain", distInfo);
}

void handleSpeed() {
  if (server.hasArg("value")) {
    motorSpeed = server.arg("value").toInt();
    analogWrite(ENA, motorSpeed);
    analogWrite(ENB, motorSpeed);
    Serial.println("Speed set to: " + String(motorSpeed));
    server.send(200, "text/plain", "Speed updated");
  } else {
    server.send(400, "text/plain", "Missing value");
  }
}

// ==========================
// Motor functions
// ==========================
void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void right() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void left() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ==========================
// Web Page
// ==========================
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <title>Spy Robot Controller</title>
    <style>
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: radial-gradient(circle, #f2f2f2 0%, #d0e7ff 100%);
        color: #333;
        text-align: center;
        padding: 30px;
      }
      h1 {
        font-size: 2.5rem;
        margin-bottom: 20px;
      }
      #distance {
        font-size: 1.4rem;
        margin-bottom: 10px;
        color: #0077cc;
      }
      #alert {
        font-size: 1.2rem;
        color: red;
        margin-bottom: 15px;
      }
      button {
        padding: 12px 24px;
        margin: 10px;
        font-size: 1.1rem;
        border: none;
        border-radius: 10px;
        background-color: #28a745;
        color: white;
        cursor: pointer;
        transition: 0.2s;
      }
      button:hover {
        background-color: #218838;
        transform: scale(1.05);
      }
      input[type="range"] {
        width: 60%;
        margin-top: 20px;
      }
    </style>
    <script>
      function updateDistance() {
        fetch("/distance")
          .then(res => res.text())
          .then(data => {
            document.getElementById("distance").innerText = data;
            const distances = data.match(/\d+/g).map(Number);
            if (distances[0] < 5 || distances[1] < 5) {
              document.getElementById("alert").innerText = "⚠️ Obstacle detected!";
            } else {
              document.getElementById("alert").innerText = "";
            }
          });
      }

      function changeSpeed(val) {
        fetch("/speed?value=" + val)
          .then(res => res.text())
          .then(msg => console.log(msg));
      }

      function move(direction) {
        fetch("/" + direction);
      }

      setInterval(updateDistance, 1000);
      window.onload = updateDistance;
    </script>
  </head>
  <body>
    <h1>Night Vision Spy Robot</h1>
    <div id="distance">Loading...</div>
    <div id="alert"></div>

    <div>
      <button onclick="move('forward')">Forward</button><br>
      <button onclick="move('left')">Left</button>
      <button onclick="move('stop')">Stop</button>
      <button onclick="move('right')">Right</button><br>
      <button onclick="move('backward')">Backward</button>
    </div>

    <h2>Adjust Motor Speed</h2>
    <input type="range" min="0" max="1023" value="800" oninput="changeSpeed(this.value)">
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

