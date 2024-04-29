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

//states (for updating webpage)
int state = 0;
// if 0: initial setup
// if 1: bird just landed
// if 2: idle

String currentWeight = "0";
String lastDownload; //timestamp of the last time user downloaded the data log
String lastStart; //timestamp of time user started new data log

String timestamp = "0";
bool newTimestamp = false;

int sim = 0;

// Timer variables
unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;

//values for simulation
int bird_there_sim[12]  = {0, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1};
//int bird_there_sim[12]  = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
bool birdThere = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

String processor(const String& var){
  if(var == "WEIGHT"){
    return String(currentWeight);
  }
  return String();
}

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
  <p id ="loading_indicator"> Loading... </p>
  <div class="container">
    <h2 style="font-size:40px;"> Weight: <span id="weight">%WEIGHT%</span> g</h2>
  </div>
  <p> Last update: <span id="datetime"></span> </p>
  <button class="button" id="downloadBtn">Download data log</button>
  <br>
  <br>
  <button class="button" id="clear_log_btn">Clear data log</button>
  <p id="feedback"></p>
  
<script>
var source;
document.getElementById("weight").innerText = '0';
if (!!window.EventSource) {
  source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);

 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('weight', function(e) {
  console.log("weight", e.data);
  document.getElementById("weight").innerText = e.data;

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
    document.getElementById('loading_indicator').style.visibility = "hidden";
    }, false);
}

// download log button
document.getElementById("downloadBtn").onclick = function() {
const now = new Date();
const day = String(now.getDate()).padStart(2, '0');
const month = String(now.getMonth() + 1).padStart(2, '0');
fetch('/download')
  .then(response => response.blob())
  .then(blob => {
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.style.display = 'none';
    a.href = url;
    //a.download = 'data_.csv';
    a.download = 'data_log_' + day + '-' + month + '.csv';
    document.body.appendChild(a);
    a.click();
    window.URL.revokeObjectURL(url);
    // Update feedback message to tell user download is complete
    document.getElementById('feedback').innerText = 'Download complete!';
  })
  .catch(error => {
    console.error('Error downloading file:', error);
    document.getElementById('feedback').innerText = 'Download failed.';
  })
  .finally(() => {
    //remove download feedback after 5 seconds
    setTimeout(() => {
      document.getElementById('feedback').innerText = '';
    }, 5000);
  });  
}

//clear data log button
document.getElementById("clear_log_btn").onclick = function() {
  if (confirm("Are you sure you want to clear this data log and start a new one? This action cannot be undone.")) {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/clear', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('clear=true');
    document.getElementById('feedback').innerText = 'Data log cleared.';
    setTimeout(() => {
      document.getElementById('feedback').innerText = '';
    }, 5000);
  }
}
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

  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("clear", true) && request->getParam("clear", true)->value() == "true") {
      File logFile = SPIFFS.open(logFileName, "w");
      if (logFile){
        //write new headers to file, overwrite old data
        logFile.print("Time");
        logFile.print(", ");
        logFile.print("Weight");
        logFile.println();
      }
      else {
        Serial.println("Failed to open log file for writing.");
      }
      logFile.close();
      request->send(200, "text/plain", "Log cleared");
    } else {
      request->send(400, "text/plain", "Failed to clear log");
    }
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 0.5 second
    client->send("hello!", NULL, millis(), 5000);
  });
  server.addHandler(&events);
  server.onNotFound(notFound);

  // setting up SPIFFS
  if (SPIFFS.begin(true)) {
    Serial.println("SPIFFS mounted successfully.");
  } else {
    Serial.println("SPIFFS mount failed. Check your filesystem.");
  }

  // Start server
  server.begin();
 }

void loop() {
  //simulating some kind of data monitoring function which produces a weight value
        //delay(5000);
        birdThere = isBird();
        if (birdThere){
          currentWeight = getWeight();
          
          // Send Events to the Web Client with the Sensor Readings
          //events.send("ping",NULL,millis());
          events.send(String(currentWeight).c_str(),"weight",millis());
          
          lastTime = millis();
          if (newTimestamp){ //only write to log if new timestamp, ie if client connected. This prevents erroneous data being recorded
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
          state = 1;
        }
        else {
          Serial.println("No bird");
          currentWeight = "0";
          if (state == 1 || state == 0){ //first loop after bird leaves perch, update webpage to 0g
            events.send("ping",NULL,millis());
            events.send(String(currentWeight).c_str(),"weight",millis());
            lastTime = millis();

            state = 2; //back into idle state
          }
        }
  sim ++;
  delay(5000);
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
