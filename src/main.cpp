#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "DLS";
const char* password = "future2809";
const int servoPin = 2;

WebServer server(80);
Servo myServo;

unsigned long detachTime = 0;
bool isServoMoving = false;

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Servo utility</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="utf-8">
    <style>
        body { font-family: sans-serif; text-align: center; margin-top: 20px; background-color: #f4f4f9; }
        .section { margin: 20px auto; padding: 20px; width: 90%; max-width: 400px; background: #fff; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        input[type=number] { width: 100px; padding: 10px; font-size: 18px; margin: 10px; text-align: center; }
        button { font-size: 18px; padding: 15px 20px; margin: 10px; border: none; border-radius: 5px; cursor: pointer; color: white; width: 80%; background-color: #4CAF50; transition: 0.2s; }
        button:active { transform: scale(0.95); }
        h1, h2 { color: #333; }
    </style>
</head>
<body>
    <h1>Servo Control</h1>
    
    <div class="section">
        <h2>Set Target Angle</h2>
        <input type="number" id="targetAngle" min="0" max="180" value="90"><br>
        <button onclick="sendAction(document.getElementById('targetAngle').value)">Set Angle</button>
    </div>
    
    <script>
        function sendAction(val) {
            fetch('/set?angle=' + val); 
        }
    </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

void handleSet() {
  if (server.hasArg("angle")) {
    int angle = server.arg("angle").toInt();
    if (angle >= 0 && angle <= 180) {
      myServo.write(angle);
      if (!myServo.attached()) {
        myServo.attach(servoPin, 500, 2400);
      }
      detachTime = millis() + 1000;
      isServoMoving = true;
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void setup() {
  Serial.begin(115200);
  
  myServo.setPeriodHertz(50); 

  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  server.on("/", handleRoot);
  server.on("/set", handleSet);

  server.begin();
}

void loop() {
  server.handleClient();

  if (isServoMoving && millis() >= detachTime) {
    myServo.detach();
    isServoMoving = false;
  }
}