#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "ssid";
const char* password = "pswd";
const int servoPin = 2;

WebServer server(80);
Servo myServo;

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Servo utility</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="utf-8">
    <style>
        body { font-family: sans-serif; text-align: center; margin-top: 50px; background-color: #f4f4f9; }
        input[type=range] { width: 80%; max-width: 400px; height: 25px; }
        h1 { color: #333; }
        .angle-display { font-size: 24px; font-weight: bold; color: #0066cc; }
    </style>
</head>
<body>
    <h1>Servo control</h1>
    <p>Angle: <span class="angle-display" id="angle_val">90</span>&deg;</p>
    <input type="range" min="0" max="180" value="90" id="servoSlider" oninput="updateServo(this.value)">
    
    <script>
        function updateServo(val) {
            document.getElementById("angle_val").innerText = val;
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
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void setup() {
  Serial.begin(115200);
  
  myServo.setPeriodHertz(50); 
  myServo.attach(servoPin, 500, 2400); 
  myServo.write(90);

  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("Wi-Fi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/set", handleSet);

  server.begin();
  Serial.println("server started");
}

void loop() {

  server.handleClient();
}