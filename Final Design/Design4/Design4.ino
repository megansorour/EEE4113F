//Author: Megan Sorour
//May 2024
//Sketch made for ESP32 for the end-user data monitoring and communications subsystem for weight monitoring fork-tailed Drongos.

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"

const char* logFileName = "/data.txt"; //filename of data log

//AP details
const char *ssid = "DrongoScale";
const char *password = "yourPassword7";

//states (for updating webpage)
int state = 0;
// if 0: start up
// if 1: bird just left
// if 2: idle

String currentWeight = "0"; //stores current weight from data processing subsystem

String timestamp = "0"; //stores most recent timestamp from client-side
bool newTimestamp = false; //flag to determine if new timestamp has been sent

int sim = 0; //controls simulation

//values for simulation
int bird_there_sim[12]  = {0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1};
//int bird_there_sim[12]  = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
bool birdThere = false; //flag showing whether a bird is detected on the scale or not

// AsyncWebServer object on TCP port 80
AsyncWebServer server(80);

// Event Source on /events
AsyncEventSource events("/events");

//functions to simulate data processing subsystem
bool isBird(){
  delay(100);
  if (bird_there_sim[sim]==0){ //go through values in array for simulation
    return false;
  }
  else {
    return true;
  }
}

String getWeight(){
  delay(2000);
  currentWeight = random(500); //generate random weight value
  return currentWeight;
}

//webpage html and JavaScript framework:
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
      color: #212121;
      background-color:#fdf7eb; 
    }
  .container {
      max-width: 320px;
      min-height: 120px;
      margin: 20px auto;
      text-align: center;
      padding: 14px;
      background-color: #fffefb;
      border-radius: 10px;
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
    }
  .button {
      background-color: #f2e7d1;
      min-width: 181px;
      border: none;
      color: #212121;
      padding: 10px 24px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 16px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 10px;
      transition: background-color 0.3s, color 0.3s, transform 0.3s, box-shadow 0.3s;
      }
    .button:active {
      background-color: #e0cba8;
      color: #fff;
      transform: translateY(1px);
      box-shadow: 0 2px 4px rgba(0, 0, 0, 0.2);
    }
      html {display: inline-block; text-align: center;}
      h1 {
          font-family: Georgia, serif; 
          color: #212121; 
          font-weight: normal;
          position: relative;
          display: inline-block;}
      h1::after {
        content: '';
        position: absolute;
        bottom: -10px; /* Adjust this value to move the line closer or further from the text */
        left: 50%; /* Position the line in the center */
        width: 200px; /* Set the width of the line */
        height: 3px; /* Set the height (thickness) of the line */
        background-color: #e78b68; /* Set the color of the line */
        transform: translateX(-50%); /* Center the line horizontally */
        border-radius: 5px;
    }
      h2 {font-size: 3.0rem; color: #212121;}
      body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    #weightDisplay {
        font-family: Arial, sans-serif;
        font: Arial;
        font-size:38px;
        font-weight: bold;
        color: #212121;
        text-align: center;
        display: flex;
        justify-content: center; 
        align-items: center; 
        height: 74px; /* container height */ 
    }
  </style>
</head>
<body>
  <h1>Bird Weight Monitoring</h1>
  <p id ="loading_indicator"> Loading... </p>
   <div class="container">
    <p id="weightDisplay"> No birds detected yet </p>
  </div>
  <p> Last update: <span id="datetime"></span> </p>
  <br>
  <br>
  <button class="button" id="downloadBtn" style="background-color: #e0d0ac;">Download data log</button>
  <br>
  <br>
  <br>
  <button class="button" id="clear_log_btn" style="background-color: #e24a4a6f;">Clear data log</button>
  <p id="feedback"></p>
  
<script>
var source;

const lastCleared = getTimestamp('lastCleared');
const lastDownloaded = getTimestamp('lastDownloaded');

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
  console.log('weight', e.data);
  if (e.data == "NewBird") {
    document.getElementById('weightDisplay').textContent = 'Bird detected';
    document.getElementById('weightDisplay').style.fontSize = '38px';
    document.getElementById('weightDisplay').style.textAlign = 'center';
  }
  else if (e.data !== "StartUp") {
    document.getElementById('weightDisplay').textContent = 'Weight: ' + e.data +' g';
    document.getElementById('weightDisplay').style.fontSize = '38px';
}
  // updating the time on webpage
  const now = new Date();
  const currentTimeSec = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  
  document.querySelector('#datetime').textContent = currentTimeSec;
  sendTime();
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
    saveTimestamp('lastDownloaded');
    setTimeout(() => {
      document.getElementById('feedback').innerText = '';
    }, 5000);
  });  
}

//clear data log button
document.getElementById("clear_log_btn").onclick = function() {
  let lastClearedString;
  let lastDownloadedString;
  let msg;
  if (getTimestamp('lastCleared')){
    lastClearedString = new Date(getTimestamp('lastCleared')).toLocaleString(undefined, { year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric' });
  }
  else {
    lastClearedString = '<error getting start date>';
  }
  if (getTimestamp('lastDownloaded')) {
    lastDownloadedString = new Date(getTimestamp('lastDownloaded')).toLocaleString(undefined, { year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric' });
  }
  else {
    lastDownloadedString = 'never';
  }
  if (!getTimestamp('lastCleared')) {
    msg = "Are you sure you want to clear the current data log and start a new one?";
  }
  else if ((!getTimestamp('lastDownloaded'))&(getTimestamp(lastCleared))) {
    msg = "Are you sure you want to clear the data log started on " + lastClearedString +"? It has not been downloaded yet."
  }
  else {
    msg = "Are you sure you want to clear the data log started on " + lastClearedString + " and start a new one? The last time a data log was downloaded was " + lastDownloadedString + ".";
  }
  if (confirm(msg)) {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/clear', true);
    xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
    xhr.send('clear=true');
    xhr.onload = function() {
    if (xhr.status == 200) {
      document.getElementById('feedback').innerText = 'Data log cleared.';
      saveTimestamp('lastCleared');
      setTimeout(() => {
        document.getElementById('feedback').innerText = '';
      }, 5000);
  }
  else {
        document.getElementById('feedback').innerText = 'Clearing data log failed.';
      }
  }
    xhr.onerror = function() {
      document.getElementById('feedback').innerText = 'Clearing data log failed.';
      setTimeout(() => {
        document.getElementById('feedback').innerText = '';
      }, 5000);
    }
}
}
// Send time to ESP32
function sendTime(){
  const now = new Date();
  const currentTime = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit'});
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

// Function to save the timestamp to localStorage
function saveTimestamp(key) {
  const now = new Date();
  localStorage.setItem(key, now.toString());
}

// Function to retrieve the timestamp from localStorage
function getTimestamp(key) {
  const timestamp = localStorage.getItem(key);
  if (timestamp) {
    return new Date(timestamp);
  }
  return null;
}

</script>
</body>
</html>
)rawliteral";

  // Error checking if webpage requested does not exist
  void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "The page not found");
  }

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  
    if (!WiFi.softAP(ssid, password)) {
      log_e("Soft AP creation failed.");
      while(1);
    }
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // Route for root '/' web page (serving html defined as index_html)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });


// Sending the data log to the server for user to download
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

  // Endpoint for client sending recent timestamp
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    // Get the timestamp value from body of request
    String currentTime;
    if (request->hasParam("plain", true)) {
      currentTime = request->getParam("plain", true)->value();
    
    // Store the timestamp in the global variable, set new timestamp to true
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

  //Endpoint for user requesting to clear data log
  server.on("/clear", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("clear", true) && request->getParam("clear", true)->value() == "true") {
      //Open data log in write mode to overwrite existing data
      File logFile = SPIFFS.open(logFileName, "w");
      if (logFile){
        //Write new column headers to file
        logFile.print("Time");
        logFile.print(", ");
        logFile.print("Weight");
        logFile.println();
      }
      else {
        Serial.println("Failed to open log file for writing.");
      }
      logFile.close();
      //Success message if complete
      request->send(200, "text/plain", "Log cleared");
    } else {
      //Otherwise error message
      request->send(400, "text/plain", "Failed to clear log");
    }
  });

  // Handling web server events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // and set reconnect delay to 0.5 second
    client->send("Server hello!", NULL, millis(), 5000);
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
        birdThere = isBird();
        if ((birdThere)&&state==2){
          currentWeight = "NewBird";
          //bird just landed on scale

          //notify client that bird detected
          events.send("ping",NULL,millis());
          events.send(String(currentWeight).c_str(),"weight",millis());

          //simulating data processing getWeight function
          currentWeight = getWeight(); 
          state = 1; //bird leaves scale
        }
          else if (state == 1){ //first loop after bird leaves perch, update webpage to final weight measurement
            // Send events to the web client with the weight measurement
            events.send(String(currentWeight).c_str(),"weight",millis());
          
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

            state = 2; //back into idle state
        }
        else if (state == 0){ //initial message to webpage after first start-up
          currentWeight = "Startup";

          //notify client once setup
          events.send("ping",NULL,millis());
          events.send(String(currentWeight).c_str(),"weight",millis());
          state = 2; //set state to idle, so it doesn't keep updating webpage with startup event
        }
        else {
          Serial.println("No bird");
          currentWeight = "0";
        }
  sim ++;
  //Optional delay, helpful for simulating process and analysing webpage
  delay(5000);
}

