#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Pin definitions
namespace Pin {
  // Motor control pins
  constexpr uint8_t FR_FWD = 27;  // Front Right Forward
  constexpr uint8_t FR_BCK = 26;  // Front Right Backward
  constexpr uint8_t FL_FWD = 12;  // Front Left Forward
  constexpr uint8_t FL_BCK = 14;  // Front Left Backward
  constexpr uint8_t BR_FWD = 25;  // Back Right Forward
  constexpr uint8_t BR_BCK = 33;  // Back Right Backward
  constexpr uint8_t BL_FWD = 32;  // Back Left Forward
  constexpr uint8_t BL_BCK = 15;  // Back Left Backward
  constexpr uint8_t LED = 2;     // Built-in LED

  // Array of all motor pins for easy initialization
  constexpr uint8_t MOTORS[] = {FR_FWD, FR_BCK, FL_FWD, FL_BCK, BR_FWD, BR_BCK, BL_FWD, BL_BCK};
}

// Access Point credentials
const char* ssid = "Newton_Car";
const char* password = "12345678";
const char* domain = "newton.ji";
const IPAddress local_ip(192,168,4,1);
const IPAddress gateway(192,168,4,1);
const IPAddress subnet(255,255,255,0);
const byte DNS_PORT = 53;

WebServer server(80);
DNSServer dnsServer;

// WiFi monitoring variables
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 2000; // Check WiFi every 2 seconds
boolean apActive = false;

// Function prototypes
void handleRoot();
void handleMotor(bool rf, bool rb, bool lf, bool lb);
void stopMotors();
void handleFigure8();
void handleSpin();
void handleZigzag();
void testMotors();
void handleNotFound();

void setup() {
  // Initialize LED pin
  pinMode(Pin::LED, OUTPUT);
  digitalWrite(Pin::LED, LOW); // Turn off initially

  // Initialize motor pins as output
  for(uint8_t pin : Pin::MOTORS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW); // Start with all motors stopped
  }

  // Stop all motors at the start
  stopMotors();

  // Start Serial for debugging
  Serial.begin(115200);
  Serial.println("\n\n--- Newton Car Starting ---");
  Serial.println("Initializing motors...");

  // Configure WiFi
  WiFi.mode(WIFI_AP);
  WiFi.disconnect();
  delay(100);
  
  // Configure static IP and start AP
  if(WiFi.softAPConfig(local_ip, gateway, subnet)) {
    Serial.println("Soft AP Config Success");
    if(WiFi.softAP(ssid, password)) {
      Serial.println("Soft AP Start Success");
      apActive = true;
    } else {
      Serial.println("Soft AP Start Failed");
    }
  } else {
    Serial.println("Soft AP Config Failed");
  }
  
  // Initialize DNS server
  dnsServer.start(DNS_PORT, domain, local_ip);
  
  Serial.println("Access Point started");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Domain: http://");
  Serial.println(domain);

  // Define web server routes
  server.on("/", handleRoot);
  server.on("/forward", [](){ 
    Serial.println("Forward command received via HTTP");
    handleMotor(true, false, true, false); 
    server.send(200, "text/plain", "OK");
  });
  server.on("/backward", [](){ 
    Serial.println("Backward command received via HTTP");
    handleMotor(false, true, false, true);
    server.send(200, "text/plain", "OK");
  });
  server.on("/left", [](){ 
    Serial.println("Left command received via HTTP");
    handleMotor(false, true, true, false);
    server.send(200, "text/plain", "OK");
  });
  server.on("/right", [](){ 
    Serial.println("Right command received via HTTP");
    handleMotor(true, false, false, true);
    server.send(200, "text/plain", "OK");
  });
  server.on("/stop", [](){ 
    Serial.println("Stop command received via HTTP");
    stopMotors();
    server.send(200, "text/plain", "OK");
  });
  server.on("/figure8", handleFigure8);
  server.on("/spin", handleSpin);
  server.on("/zigzag", handleZigzag);
  server.on("/test", testMotors);
  
  // Handle 404 Not Found
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Server started");
  
  // Blink LED to indicate successful initialization
  for (int i = 0; i < 3; i++) {
    digitalWrite(Pin::LED, HIGH);
    delay(100);
    digitalWrite(Pin::LED, LOW);
    delay(100);
  }

  // Test all motor pins to ensure they're working
  Serial.println("Testing motor pins for output capability...");
  // Front Right
  digitalWrite(Pin::FR_FWD, HIGH);
  delay(50);
  digitalWrite(Pin::FR_FWD, LOW);
  // Front Left
  digitalWrite(Pin::FL_FWD, HIGH);
  delay(50);
  digitalWrite(Pin::FL_FWD, LOW);
  // Back Right
  digitalWrite(Pin::BR_FWD, HIGH);
  delay(50);
  digitalWrite(Pin::BR_FWD, LOW);
  // Back Left
  digitalWrite(Pin::BL_FWD, HIGH);
  delay(50);
  digitalWrite(Pin::BL_FWD, LOW);
  
  Serial.println("Motor pin test completed");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Check WiFi status periodically
  if (currentMillis - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = currentMillis;
    
    if (!apActive || WiFi.softAPgetStationNum() == 0) {
      // Try to restart AP if it's down or no clients are connected
      if (!apActive) {
        Serial.println("Attempting to restart AP...");
        WiFi.mode(WIFI_AP);
        delay(100);
        if(WiFi.softAPConfig(local_ip, gateway, subnet) && WiFi.softAP(ssid, password)) {
          apActive = true;
          Serial.println("AP restarted successfully");
        }
      }
      
      // Blink LED to indicate no clients
      digitalWrite(Pin::LED, !digitalRead(Pin::LED));
    } else {
      // Solid LED when clients are connected
      digitalWrite(Pin::LED, HIGH);
    }
    
    // Print debug info
    Serial.print("AP Status: ");
    Serial.print(apActive ? "Active" : "Inactive");
    Serial.print(", Connected clients: ");
    Serial.println(WiFi.softAPgetStationNum());
  }
  
  // Process DNS and web requests
  if (apActive) {
    dnsServer.processNextRequest();
    server.handleClient();
  }
}

void handleRoot() {
  const char* html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Newton Car Control</title>
<style>
body{background:#000;color:#fff;font-family:system-ui;margin:0;padding:10px;text-align:center;touch-action:manipulation;-webkit-user-select:none;user-select:none}
.grid{display:grid;grid-template-columns:repeat(3,minmax(70px,80px));grid-template-rows:repeat(3,minmax(70px,80px));gap:8px;margin:20px auto}
.btn{background:none;border:2px solid #0f0;color:#0f0;border-radius:12px;font-size:24px;font-weight:bold;display:flex;align-items:center;justify-content:center}
.btn:active{background:#0f02}
.stop{border-color:#f00;color:#f00}
.stunts{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin:15px auto;max-width:300px}
.stunt-btn{background:none;border:2px solid #56f;color:#56f;padding:12px 5px;border-radius:12px;font-size:18px;font-weight:bold}
.ind{position:fixed;top:10px;right:10px;width:10px;height:10px;border-radius:50%;background:#333}
.connected{background:#0f0}
.error{background:#f00}
.title{margin:10px 0;font-size:24px}
.section{margin:10px 0;font-size:18px;color:#0f0}
.hint{margin:8px 0;font-size:14px;color:#56f}
</style>
</head>
<body>
<div id="indicator" class="ind"></div>
<h1 class="title">Newton Car Control</h1>

<div class="section">Direction Control</div>
<div class="grid">
<div></div><button class="btn" id="forward-btn">&#9650;</button><div></div>
<button class="btn" id="left-btn">&#9668;</button><button class="btn stop" id="stop-btn">■</button><button class="btn" id="right-btn">&#9658;</button>
<div></div><button class="btn" id="backward-btn">&#9660;</button><div></div>
</div>

<div class="section">Special Moves</div>
<div class="stunts">
<button class="stunt-btn" id="figure8-btn">Figure 8</button>
<button class="stunt-btn" id="spin-btn">Spin</button>
<button class="stunt-btn" id="zigzag-btn">Zigzag</button>
<button class="stunt-btn" id="test-btn">Test Motors</button>
</div>

<div class="hint">Use W/A/S/D keys on desktop</div>

<script>
'use strict';
const controlButtons = {
  'forward-btn': 'forward',
  'backward-btn': 'backward',
  'left-btn': 'left',
  'right-btn': 'right',
  'stop-btn': 'stop',
  'figure8-btn': 'figure8',
  'spin-btn': 'spin',
  'zigzag-btn': 'zigzag',
  'test-btn': 'test'
};

let activeCommand = null;
let lastCommandTime = 0;

// Prevent zoom/scroll on mobile
document.addEventListener('touchmove', e => e.preventDefault(), {passive: false});
document.addEventListener('touchstart', e => {
  if (e.target.tagName === 'BUTTON') e.preventDefault();
}, {passive: false});

function vibrate(duration) {
  if ('vibrate' in navigator) navigator.vibrate(duration);
}

function sendCommand(command) {
  const now = Date.now();
  if (now - lastCommandTime < 80) return;
  lastCommandTime = now;
  
  const indicator = document.getElementById('indicator');
  indicator.className = 'ind';
  
  fetch('/' + command)
    .then(response => {
      indicator.className = response.ok ? 'ind connected' : 'ind error';
      setTimeout(() => indicator.className = 'ind', 500);
    })
    .catch(() => {
      indicator.className = 'ind error';
      setTimeout(() => indicator.className = 'ind', 500);
    });
}

// Setup control buttons with touch/mouse events
for (const [btnId, command] of Object.entries(controlButtons)) {
  const btn = document.getElementById(btnId);
  if (!btn) continue;
  
  // For movement controls (excluding special moves), handle press/release
  if (['forward', 'backward', 'left', 'right'].includes(command)) {
    btn.addEventListener('touchstart', () => {
      vibrate(20);
      activeCommand = command;
      sendCommand(command);
    });
    
    btn.addEventListener('touchend', () => {
      if (activeCommand === command) {
        activeCommand = null;
        sendCommand('stop');
      }
    });
    
    btn.addEventListener('mousedown', () => {
      activeCommand = command;
      sendCommand(command);
    });
    
    btn.addEventListener('mouseup', () => {
      if (activeCommand === command) {
        activeCommand = null;
        sendCommand('stop');
      }
    });
    
    btn.addEventListener('mouseleave', () => {
      if (activeCommand === command) {
        activeCommand = null;
        sendCommand('stop');
      }
    });
  } else {
    // For special moves and stop, just handle click
    btn.addEventListener('click', () => {
      vibrate(20);
      sendCommand(command);
      activeCommand = null;
    });
  }
}

// Keyboard controls for desktop
const keyMap = {
  'w': 'forward',
  's': 'backward',
  'a': 'left',
  'd': 'right',
  ' ': 'stop'
};

document.addEventListener('keydown', e => {
  const command = keyMap[e.key.toLowerCase()];
  if (command && activeCommand !== command) {
    activeCommand = command;
    sendCommand(command);
  }
});

document.addEventListener('keyup', e => {
  const command = keyMap[e.key.toLowerCase()];
  if (command && activeCommand === command) {
    activeCommand = null;
    sendCommand('stop');
  }
});

// Handle page visibility change
document.addEventListener('visibilitychange', () => {
  if (document.hidden && activeCommand) {
    sendCommand('stop');
    activeCommand = null;
  }
});
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleMotor(bool rf, bool rb, bool lf, bool lb) {
  // Update all motors in one go
  const uint8_t pins[] = {Pin::FR_FWD, Pin::FR_BCK, Pin::FL_FWD, Pin::FL_BCK};
  const bool states[] = {rf, rb, lf, lb};
  
  for(uint8_t i = 0; i < 4; i++) {
    digitalWrite(pins[i], states[i]);
    digitalWrite(pins[i] + 8, states[i]);  // Ensure back motors are controlled correctly
  }
  
  // Visual feedback
  digitalWrite(Pin::LED, rf || rb || lf || lb);
}

void stopMotors() {
  // Stop all motors at once
  for(uint8_t pin : Pin::MOTORS) {
    digitalWrite(pin, LOW);
  }
  digitalWrite(Pin::LED, LOW);
}

// Stunt movement functions
void handleFigure8() {
  // Perform figure 8 movement
  for(int i = 0; i < 2; i++) {
    handleMotor(true, false, false, true);  // Right turn
    delay(1000);
    handleMotor(false, true, true, false);  // Left turn
    delay(1000);
  }
  stopMotors();
  server.send(200, "text/plain", "Figure 8 completed");
}

void handleSpin() {
  // Perform 360-degree spin
  handleMotor(true, false, false, true);  // Right spin
  delay(800);
  stopMotors();
  server.send(200, "text/plain", "Spin completed");
}

void handleZigzag() {
  // Perform zigzag movement
  for(int i = 0; i < 3; i++) {
    handleMotor(true, false, true, false);  // Forward
    delay(400);
    handleMotor(true, false, false, true);  // Right
    delay(250);
    handleMotor(true, false, true, false);  // Forward
    delay(400);
    handleMotor(false, true, true, false);  // Left
    delay(250);
  }
  stopMotors();
  server.send(200, "text/plain", "Zigzag completed");
}

// Test function to check if motors are working
void testMotors() {
  Serial.println("Starting motor test sequence...");
  
  // Blink LED quickly to indicate test mode
  for (int i = 0; i < 5; i++) {
    digitalWrite(Pin::LED, HIGH);
    delay(50);
    digitalWrite(Pin::LED, LOW);
    delay(50);
  }
  
  // Test front right motor
  Serial.println("Testing front right motor (forward)");
  digitalWrite(Pin::FR_FWD, HIGH);
  digitalWrite(Pin::FR_BCK, LOW);
  delay(1000);
  digitalWrite(Pin::FR_FWD, LOW);
  delay(500);  // Pause between tests
  
  // Test front left motor
  Serial.println("Testing front left motor (forward)");
  digitalWrite(Pin::FL_FWD, HIGH);
  digitalWrite(Pin::FL_BCK, LOW);
  delay(1000);
  digitalWrite(Pin::FL_FWD, LOW);
  delay(500);  // Pause between tests
  
  // Test back right motor
  Serial.println("Testing back right motor (forward)");
  digitalWrite(Pin::BR_FWD, HIGH);
  digitalWrite(Pin::BR_BCK, LOW);
  delay(1000);
  digitalWrite(Pin::BR_FWD, LOW);
  delay(500);  // Pause between tests
  
  // Test back left motor
  Serial.println("Testing back left motor (forward)");
  digitalWrite(Pin::BL_FWD, HIGH);
  digitalWrite(Pin::BL_BCK, LOW);
  delay(1000);
  digitalWrite(Pin::BL_FWD, LOW);
  delay(500);  // Pause between tests
  
  // Test front right motor backward
  Serial.println("Testing front right motor (backward)");
  digitalWrite(Pin::FR_BCK, HIGH);
  delay(1000);
  digitalWrite(Pin::FR_BCK, LOW);
  delay(500);  // Pause between tests
  
  // Test front left motor backward
  Serial.println("Testing front left motor (backward)");
  digitalWrite(Pin::FL_BCK, HIGH);
  delay(1000);
  digitalWrite(Pin::FL_BCK, LOW);
  delay(500);  // Pause between tests
  
  // Test all motors together
  Serial.println("Testing all motors (forward)");
  digitalWrite(Pin::FR_FWD, HIGH);
  digitalWrite(Pin::FL_FWD, HIGH);
  digitalWrite(Pin::BR_FWD, HIGH);
  digitalWrite(Pin::BL_FWD, HIGH);
  delay(2000);
  
  // Stop all motors
  Serial.println("Test complete, stopping all motors");
  stopMotors();
  
  server.send(200, "text/plain", "Motor test completed");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
