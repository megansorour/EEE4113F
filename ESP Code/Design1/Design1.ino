#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <FS.h>
//#include <sys/stat.h> //for accessing file info and file-related operations

const char* logFileName = "/data.txt";

const char *ssid = "DrongoScale";
const char *password = "yourPassword7";

const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";
const char* PARAM_INPUT_3 = "weight";

String currentWeight = "0";
String timestamp = "0";
bool newTimestamp = false;

int sim = 0;

//values for simulation
int bird_there_sim[12]  = {0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1};
bool birdThere = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Bird Weight Monitoring</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
  body {
      font-family: Arial, sans-serif;
      text-align: center;
      margin: 0;
      padding: 0;
    }
  .container {
      max-width: 400px;
      min-height: 50px;
      margin: 20px auto;
      padding: 20px;
      background-color: #f4f4f4;
      border-radius: 10px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
    }
  .button {
      background-color: #c8c8c8;
      border: none;
      color: #000000;
      padding: 10px 24px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 10px;
      }
      html {font-family: Arial; display: inline-block; text-align: center;}
      h2 {font-size: 3.0rem;}
      body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
  </style>
</head>
<body>
  <h1>Bird Weight Monitoring</h1>
  <div class="container">
  <h2 style="font-size:40px;"> Weight: <span id="weight"></span>g</h2>
</div>
<p> Last update: <span id="datetime"></span> </p>
    <!-- <button type="button" class="button"
     onclick="logWeight()">Log current weight</button> 
    <br>
    <br> -->
    <button class="button" id="downloadBtn">Download data log</button>
  
<script>

  function logWeight() {
    // logging the measurement to the data array
    dataLog.push(document.getElementById('datetime').innerText);
    dataLog.push(document.getElementById('weight').innerText);
  }

function updateWeight() {
  fetch('/weight')
  .then(response => response.json())
  .then(data => {
    document.getElementById('weight').innerText = data.weight;
  })
  .catch(error => {
    console.error('Error fetching weight:', error);
  });

  // updating the time on webpage
const now = new Date();
const currentTimeSec = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
const currentTime = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit'});
document.querySelector('#datetime').textContent = currentTimeSec;

  // Send time to ESP32
  console.log('Sending time:', currentTime);
  fetch('/upload', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded'
    },
    body: 'plain=' + encodeURIComponent(currentTime)
  })
  .then(response => response.text())
  .then(data => console.log('Response:', data))
  .catch(error => console.error('Error sending time:', error));
  }

// Update weight and send time to ESP32 every 5 seconds
setInterval(updateWeight, 5000);

// download log button
    document.getElementById("downloadBtn").onclick = function() {
    fetch('/download')
      .then(response => response.blob())
      .then(blob => {
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.style.display = 'none';
        a.href = url;
        a.download = 'data.txt';
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
      })
      .catch(error => console.error('Error downloading file:', error));
}

// Update weight every 5 seconds
setInterval(updateWeight, 5000);

</script>
</body>
</html>
)rawliteral";

// Replaces placeholder with button section in your web page

  // If webpage requested does not exist
  void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "The page not found");
  }


void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  
    // You can remove the password parameter if you want the AP to be open.
    // a valid password must have more than 7 characters
    if (!WiFi.softAP(ssid, password)) {
      log_e("Soft AP creation failed.");
      while(1);
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.begin();

    // Route for root / web page (navigating to html thing defined as index_html) (home page)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Set up the endpoint to return the current weight value
  server.on("/weight", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", String("{\"weight\":") + currentWeight + "}");
  });

//sending the data log to the server for user to download
server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("Download request received");
    
    File file = SPIFFS.open("/data.txt", "r");
    if (!file) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(SPIFFS, "/data.txt", "text/plain", false);
    file.close();
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    // Get the timestamp value from the request body
    String currentTime;
    if (request->hasParam("plain", true)) {
      currentTime = request->getParam("plain", true)->value();
    
    // Store the timestamp in the global variable
    timestamp = currentTime;
    newTimestamp = true;
    }
    else {
      currentTime = "No time sent";
    }
    // Respond with a success message
    request->send(200, "text/plain", "Timestamp received: " + currentTime);
    Serial.println(timestamp);
  });

  server.onNotFound(notFound);

  // Start server
  server.begin();

  // setting up SPIFFS
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS mounted successfully.");
  } else {
    Serial.println("SPIFFS mount failed. Check your filesystem.");
  }
}

void loop() {
  //simulating some kind of data monitoring function which produces a weight value
        delay(5000);
        birdThere = isBird();
        if (birdThere){
          currentWeight = getWeight();
          if (newTimestamp){
            //opening the log file in append mode
            File logFile = SPIFFS.open(logFileName, "a");

            if (logFile){
              //append weight and time to log file
              logFile.print(timestamp);
              logFile.print(", ");
              logFile.print(currentWeight);
              logFile.println();
            }
            else {
              Serial.println("Failed to open log file for writing.");
            }
            logFile.close();
            newTimestamp = false; // Reset the flag
          }
        }
        else {
          currentWeight = "0";
        }
  sim ++;
  delay(1000);

}

//functions to simulate data processing subsystem
bool isBird(){
  delay(100);
  if (bird_there_sim[sim]==0){
    return false;
  }
  else {
    return true;
  }
}

String getWeight(){
  delay(1000);
  currentWeight = random(500); //random weight value
  return currentWeight;
}
