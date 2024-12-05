#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HardwareSerial.h>
#include <AsyncWebSocket.h>



const char* ssid = "";
const char* password = "";

const int status_led_Pin = 2;
HardwareSerial mySerial(1); 
AsyncWebSocket ws("/ws");

/*************************************************************************************************************************************** */
AsyncWebServer server(80);
String weightValue = "0.000"; 
bool isReceivingWeight = false; 
String globalWeightValue = "0.000";

/***************************************************************************************************************************************** */

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.println("WebSocket client connected.");
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.println("WebSocket client disconnected.");
    }
}

void sendWeightToWebSocket(const String& weight) {
    ws.textAll(weight); 
}
/******************************************************************************************************************************************** */
/******************************************************************************************************************************************* */
void sendCommandToPondera(const String& command) {
    Serial.println("Sending command to Pondera: " + command);
    mySerial.print(command + "\r\n"); 

    unsigned long timeout = millis() + 2000; 
    while (millis() < timeout) {
        if (mySerial.available()) {
            String response = mySerial.readStringUntil('\n'); 
            response = manualTrim(response); 
            Serial.println("Response from Pondera: " + response);

            if (response == "ACK") {
                Serial.println("ACK received. Command executed successfully.");
                isReceivingWeight = true; 
                return;
            }
        }
        delay(10);
    }

    Serial.println("No ACK received. Command failed.");
}
/**************************************************************************************************************************************** */
void receiveWeightData() {
    if (!isReceivingWeight) return; 

    if (mySerial.available()) {
        String response = mySerial.readStringUntil('\n'); 
        response = manualTrim(response); 
        Serial.println("Weight data from Pondera: " + response);

        int weightStart = response.indexOf("PERSIAN:");
        if (weightStart != -1) {
            weightStart += 8; 
            if (response[weightStart] == ' ') {
                weightStart++;
            }

            int weightEnd = response.indexOf("grams", weightStart);
            if (weightEnd == -1) {
                weightEnd = response.indexOf("kg", weightStart);
            }
            if (weightEnd == -1) {
                weightEnd = response.length();
            }

            if (weightStart <= weightEnd && weightEnd <= response.length()) {
                globalWeightValue = manualTrim(response.substring(weightStart, weightEnd));
              //  Serial.println("Extracted weight: " + globalWeightValue);
                sendWeightToWebSocket(globalWeightValue); 
            }
        }
    }
}

/************************************************************************************************************************************* */
void setup() {
    pinMode(status_led_Pin, OUTPUT);
    mySerial.begin(115200, SERIAL_8N1, 16, 17); // RX=GPIO16, TX=GPIO17
    Serial.begin(115200);
    Serial.println("ESP32 UART2 initialized!");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi connection failed!");
        return;
    }

    Serial.println("Connected to WiFi.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());




server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head>";
    html += "<style>"
            "button {"
            "   font-size: 30px;"
            "   padding: 20px 50px;"
            "   color: white;"
            "   background: linear-gradient(145deg, #3e8e41, #2a662c);"
            "   border: none;"
            "   border-radius: 15px;"
            "   box-shadow: 5px 5px 15px #1f3e1f, -5px -5px 15px #4eff5c;"
            "   cursor: pointer;"
            "   transition: transform 0.2s, background-color 0.3s;"
            "   margin: 10px;"
            "} "
            "button:hover {"
            "   transform: translateY(-5px);"
            "   box-shadow: 10px 10px 20px #1f3e1f, -10px -10px 20px #4eff5c;"
            "} "
            "#stopButton {"
            "   font-size: 35px;" 
            "   padding: 30px 60px;" 
            "   color: white;"
            "   background: linear-gradient(145deg, #3e8e41, #2a662c);"
            "   border: none;"
            "   border-radius: 15px;"
            "   box-shadow: 5px 5px 15px #1f3e1f, -5px -5px 15px #4eff5c;"
            "   cursor: pointer;"
            "   transition: transform 0.2s, background-color 0.3s;"
            "   margin: 10px;"
            "} "
            "#stopButton.active {"
            "   background: linear-gradient(145deg, #e63939, #b00000);"
            "   box-shadow: 5px 5px 15px #700000, -5px -5px 15px #ff4040;"
            "} "
            ".weight-container {"
            "   display: flex;"
            "   justify-content: center;"
            "   align-items: center;"
            "   margin-top: 20px;"
            "   font-family: Arial, sans-serif;"
            "} "
            ".weight-label {"
            "   font-size: 40px;"
            "   font-weight: bold;"
            "   margin-right: 10px;"
            "   color: #2a662c;"
            "} "
            "input[type='text'] {"
            "   font-size: 60px;"
            "   font-weight: bold;"
            "   padding: 20px;"
            "   width: 300px;"
            "   height: 80px;"
            "   text-align: center;"
            "   border: 2px solid #3e8e1f;"
            "   border-radius: 10px;"
            "   box-shadow: 3px 3px 10px #888;"
            "   color: #2a662c;"
            "   margin-right: 10px;"
            "} "
            ".unit {"
            "   font-size: 40px;"
            "   font-weight: bold;"
            "   color: #2a662c;"
            "   display: inline-block;"
            "} "
            ".stop-container {"
            "   margin-top: 30px;"
            "} "
            "h1 { font-family: Arial, sans-serif; font-size: 50px; } "
            "body { font-family: Arial, sans-serif; text-align: center; padding-top: 50px; } "
            "</style></head><body>";
    html += "<h1>Pondera Control Panel</h1>";
    html += "<button onclick=\"sendCommand('AT+RUN')\">Run (AT+RUN)</button><br>";
    html += "<div class='weight-container'>"
            "<span class='weight-label'>Weight:</span>"
            "<input type='text' id='weightValue' value='0.000' readonly>"
            "<span class='unit' id='unit'>grams</span>"
            "</div>";
    html += "<div class='stop-container'>"
            "<button id='stopButton' onclick=\"sendStopCommand()\">Stop</button>"
            "</div>";
    html += "<script>"


            "const ws = new WebSocket('ws://' + window.location.host + '/ws');"
            "ws.onmessage = function(event) {"
            "   const weight = parseFloat(event.data);"
            "   document.getElementById('weightValue').value = weight.toFixed(3);"
            "   document.getElementById('unit').innerText = weight >= 1000 ? 'Kg' : 'grams';"
            "};"


            "ws.onopen = function() { console.log('WebSocket connection established.'); };"
            "ws.onclose = function() { console.log('WebSocket connection closed.'); };"
            "function sendCommand(cmd) {"
            "   fetch('/sendCommand?cmd=' + encodeURIComponent(cmd)).then(response => response.text()).then(data => {"
            "       console.log('Command Response: ' + data);"
            "   });"
            "} "
            "function sendStopCommand() {"
            "   const stopButton = document.getElementById('stopButton');"
            "   if (stopButton) {"
            "       fetch('/sendCommand?cmd=' + encodeURIComponent('#')).then(response => response.text()).then(data => {"
            "           console.log('Command Response: ' + data);"
            "           stopButton.classList.add('active');"
            "           setTimeout(() => {"
            "               stopButton.classList.remove('active');"
            "           }, 1000);"
            "       }).catch(error => console.error('Error:', error));"
            "   } else {"
            "       console.error('Stop button not found!');"
            "   }"
            "}"
            "</script>";
    html += "</body></html>";
    request->send(200, "text/html", html);
});


server.on("/sendCommand", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("cmd")) {
        String cmd = request->getParam("cmd")->value();
        
        
        if (cmd == "AT+RUN") {
            sendCommandToPondera(cmd + "\r\n"); 
            request->send(200, "text/plain", "Weight tracking started...");
        }
        
        else if (cmd == "#") {
            sendCommandToPondera("#\r\n"); 
            request->send(200, "text/plain", "Stop command sent...");
        }
    
        else {
            request->send(400, "text/plain", "Invalid command");
        }
    } else {
        request->send(400, "text/plain", "No command provided");
    }
});


    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not Found");
    });

    
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.begin();
}
/********************************************************************************************************************************** */
void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(status_led_Pin, HIGH); 
    } else {
        digitalWrite(status_led_Pin, LOW); 
    }

    receiveWeightData();
    
    delay(10); 
}
/******************************************************************************************************************************** */