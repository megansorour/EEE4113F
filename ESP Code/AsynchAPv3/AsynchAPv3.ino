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
const char* PARAM_MESSAGE = "message";

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
      .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
      .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
      input:checked+.slider {background-color: #b30000}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
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
  %BUTTONPLACEHOLDER%
  
<script>

function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}
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
const currentTime = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
document.querySelector('#datetime').textContent = currentTime;

// V1 Send time to ESP32
//   fetch('/upload', {
//     method: 'POST',
//     headers: {
//       'Content-Type': 'text/plain'
//     },
//     body: currentTime
//   })
//   .then(response => response.text())
//   .then(data => console.log(data))
//   .catch(error => console.error('Error sending time:', error));

// //   // Update time on the webpage
// //   document.querySelector('#time').textContent = currentTime;
     

// //V2
//   // Create a new XMLHttpRequest object
//   var xhr = new XMLHttpRequest();

//   // Configure the request
//   xhr.open("POST", "/upload", true);
//   xhr.setRequestHeader("Content-Type", "text/plain");

//   // Send the timestamp
//   xhr.send(currentTime);

//   // Handle the response
//   xhr.onreadystatechange = function () {
//     if (xhr.readyState === 4) {
//       if (xhr.status === 200) {
//         console.log("Timestamp sent successfully");
//       } else {
//         console.error("Failed to send timestamp");
//       }
//     }
//   };

//V3
// Send time to ESP32
console.log('Sending time:', currentTime); // Add this line
  fetch('/upload', {
    method: 'POST',
    headers: {
      'Content-Type': 'text/plain'
    },
    body: currentTime
  })
  .then(response => response.text())
  .then(data => console.log(data))
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
String processor(const String& var){
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons = "";
    buttons += "<h4>Output - Blue LED</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"2\" " + outputState(2) + "><span class=\"slider\"></span></label>";
    return buttons;
  }
  return String();
}

String outputState(int output){
  if(digitalRead(output)){
    return "checked";
  }
  else {
    return "";
  }
}

  // If webpage requested does not exist
  void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "The page not found");
  }


void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW);
  
    // You can remove the password parameter if you want the AP to be open.
    // a valid password must have more than 7 characters
    if (!WiFi.softAP(ssid, password)) {
      log_e("Soft AP creation failed.");
      while(1);
    }
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);
    server.begin();

    // Route for root / web page (navigating to html thing defined as index_html) (home page)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // LED: Send a GET request to <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage1;
    String inputMessage2;
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
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


//V1receiving the current timestamp from the server, manually parsing JSON
// server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
//   // Check if the request has a "plain" parameter
//   if (!request->hasParam("plain", true)) {
//     request->send(400, "text/plain", "Missing timestamp");
//     return;
//   }

//   // Get the value of the "plain" parameter (the timestamp)
//   AsyncWebParameter* plainParam = request->getParam("plain", true);
//   if (!plainParam->isPost()) {
//     request->send(400, "text/plain", "Invalid request");
//     return;
//   }

//   // Get the timestamp value as a string
//   timestamp = plainParam->value();

//   // Handle the timestamp (e.g., store it with the weights)
//   // Here, we simply respond with a success message
//   request->send(200, "text/plain", "Timestamp received: " + timestamp);
//   Serial.println(timestamp);
// });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    // Get the timestamp value from the request body
    String currentTime;
    if (request->hasParam("plain", true)) {
      //AsyncWebParameter* plainParam = request->getParam("plain", true);
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
