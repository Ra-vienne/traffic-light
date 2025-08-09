// Pin definitions red yellow green
const int lights[5][3] = {
  {A2, A1, A0},    // 0: Center
  {2, 3, 4},       // 1: North
  {5, 6, 7},       // 2: East
  {8, 9, 10},      // 3: South
  {11, 12, 13}     // 4: West
};

// System state
int lightOrder[5] = {0, 1, 2, 3, 4};
unsigned long lightDelays[5][3] = {
  {5000, 1000, 5000},  // Center
  {5000, 1000, 5000},  // North
  {5000, 1000, 5000},  // East
  {5000, 1000, 5000},  // South
  {5000, 1000, 5000}   // West
};

// Traffic cycle state machine
enum CycleState {
  ALL_RED,
  GREEN_ACTIVE,
  YELLOW_GREEN_TO_RED,
  YELLOW_RED_TO_GREEN
};

struct SystemState {
  bool isPaused = false;
  unsigned long pauseStartTime = 0;
  unsigned long phaseStartTime = 0;
  int currentLight = 0;
  int nextLight = 1;
  CycleState currentPhase = ALL_RED;
} systemState;

// Update direction names to match: "NORTH", "SW", "SE", "NW", "NE"
const char* lightNames[5] = {"NORTH", "SW", "SE", "NW", "NE"};

void setup() {
  Serial.begin(115200);
  Serial.println("\nTraffic Light Controller v2.2 (Interruptible)");
  Serial.println("Available commands:");
  Serial.println("!order 0,1,2,3,4 - Set light sequence");
  Serial.println("!delay 5000,2000,5000,... - Set all timings (15 values)");
  Serial.println("!pause - Freeze current state");
  Serial.println("!resume - Continue operation");
  Serial.println("!status - Show current settings");
  
  // Initialize pins
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 3; j++) {
      pinMode(lights[i][j], OUTPUT);
      digitalWrite(lights[i][j], LOW);
    }
  }
  turnAllRed();
  systemState.phaseStartTime = millis();
}

void loop() {
  if (!systemState.isPaused) {
    runTrafficCycle();
  }
  checkSerial();
}

void runTrafficCycle() {
  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - systemState.phaseStartTime;
  
  int current = lightOrder[systemState.currentLight];
  int next = lightOrder[systemState.nextLight];
  
  switch (systemState.currentPhase) {
    case ALL_RED:
      // Start with all red, then immediately move to GREEN_ACTIVE
      turnAllRed();
      systemState.currentPhase = GREEN_ACTIVE;
      systemState.phaseStartTime = currentTime;
      sendLightStates();
      break;  

    case GREEN_ACTIVE:
      // Ensure all other lights are RED
      turnAllRedExcept(current);
      digitalWrite(lights[current][2], HIGH); // Turn on current GREEN
      
      if (elapsed >= lightDelays[current][2]) {
        // GREEN duration complete
        digitalWrite(lights[current][2], LOW); // Turn off GREEN
        systemState.currentPhase = YELLOW_GREEN_TO_RED;
        systemState.phaseStartTime = currentTime;
        sendLightStates();
      }
      break;
      
    case YELLOW_GREEN_TO_RED:
      digitalWrite(lights[current][1], HIGH); // Turn on YELLOW
      
      if (elapsed >= lightDelays[current][1]) {
        // YELLOW duration complete
        digitalWrite(lights[current][1], LOW);  // Turn off YELLOW
        digitalWrite(lights[current][0], HIGH);  // Turn on RED (should already be on)
        systemState.currentPhase = YELLOW_RED_TO_GREEN;
        systemState.phaseStartTime = currentTime;
        sendLightStates();
      }
      break;
      
    case YELLOW_RED_TO_GREEN:
      // For next light: RED+YELLOW (prepare to go GREEN)
      digitalWrite(lights[next][1], HIGH); // Turn on YELLOW (with RED)
      
      if (elapsed >= lightDelays[next][1]) {
        // YELLOW duration complete
        digitalWrite(lights[next][0], LOW);  // Turn off RED
        digitalWrite(lights[next][1], LOW);  // Turn off YELLOW
        digitalWrite(lights[next][2], HIGH); // Turn on GREEN
        
        // Move to next light in sequence
        systemState.currentLight = (systemState.currentLight + 1) % 5;
        systemState.nextLight = (systemState.currentLight + 1) % 5;
        systemState.currentPhase = GREEN_ACTIVE;
        systemState.phaseStartTime = currentTime;
        sendLightStates();
      }
      break;
  }
}

void turnAllRed() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(lights[i][0], HIGH); // RED on
    digitalWrite(lights[i][1], LOW);  // YELLOW off
    digitalWrite(lights[i][2], LOW);  // GREEN off
  }
  sendLightStates();
}

void turnAllRedExcept(int exception) {
  for (int i = 0; i < 5; i++) {
    if (i == exception) continue;
    digitalWrite(lights[i][0], HIGH); // RED on
    digitalWrite(lights[i][1], LOW);  // YELLOW off
    digitalWrite(lights[i][2], LOW);  // GREEN off
  }
  sendLightStates();
}

void checkSerial() {
  while (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input == "!pause") {
      systemState.isPaused = true;
      systemState.pauseStartTime = millis();
      Serial.println("[System PAUSED]");
      printCurrentState();
    }
    else if (input == "!resume") {
      systemState.isPaused = false;
      // Adjust phase start time to account for pause duration
      systemState.phaseStartTime += (millis() - systemState.pauseStartTime);
      Serial.print("[System RESUMED] Paused for ");
      Serial.print((millis() - systemState.pauseStartTime) / 1000.0, 1);
      Serial.println(" seconds");
      printCurrentState();
    }
    else if (input.startsWith("!order ")) {
      setLightOrder(input.substring(7));
      // Reset cycle when order changes
      systemState.currentLight = 0;
      systemState.nextLight = 1;
      systemState.currentPhase = ALL_RED;
      systemState.phaseStartTime = millis();
      turnAllRed();
    }
    else if (input.startsWith("!delay ")) {
      setLightDelays(input.substring(7));
    }
    else if (input == "!status") {
      printStatus();
    }
    else {
      Serial.println("Unknown command. Try: !order !delay !pause !resume !status");
    }
  }
}

void setLightOrder(const String &data) {
  int newOrder[5] = {-1, -1, -1, -1, -1};
  int count = 0;
  
  int start = 0;
  int end = data.indexOf(',');
  
  while (end != -1 && count < 5) {
    newOrder[count++] = data.substring(start, end).toInt();
    start = end + 1;
    end = data.indexOf(',', start);
  }
  
  if (count < 5 && start < data.length()) {
    newOrder[count++] = data.substring(start).toInt();
  }

  // Validate
  bool valid = (count == 5);
  bool used[5] = {false};
  for (int i = 0; i < 5; i++) {
    if (newOrder[i] < 0 || newOrder[i] > 4 || used[newOrder[i]]) {
      valid = false;
      break;
    }
    used[newOrder[i]] = true;
  }

  if (valid) {
    memcpy(lightOrder, newOrder, sizeof(lightOrder));
    Serial.println("Light order updated:");
    printCurrentOrder();
  } else {
    Serial.println("Error: Need 5 unique numbers 0-4 separated by commas");
    Serial.println("Example: !order 1,2,3,4,0");
  }
}

void setLightDelays(const String &data) {
  int values[15];
  int count = 0;
  
  int start = 0;
  int end = data.indexOf(',');
  
  while (end != -1 && count < 15) {
    values[count++] = data.substring(start, end).toInt();
    start = end + 1;
    end = data.indexOf(',', start);
  }
  
  if (count < 15 && start < data.length()) {
    values[count++] = data.substring(start).toInt();
  }

  if (count == 15) {
    for (int i = 0; i < 5; i++) {
      lightDelays[i][0] = max(values[i*3], 100);     // Min 100ms for Red
      lightDelays[i][1] = max(values[i*3+1], 100);   // Min 100ms for Yellow
      lightDelays[i][2] = max(values[i*3+2], 100);   // Min 100ms for Green
    }
    Serial.println("Delays updated:");
    printCurrentDelays();
  } else {
    Serial.println("Error: Need 15 delay values (R,Y,G for each light)");
    Serial.println("Example: !delay 5000,2000,5000,5000,2000,5000,...");
  }
}

void printStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.print("State: ");
  Serial.println(systemState.isPaused ? "PAUSED" : "RUNNING");
  printCurrentState();
  printCurrentOrder();
  printCurrentDelays();
}

void printCurrentState() {
  Serial.print("Current light: ");
  Serial.println(lightNames[lightOrder[systemState.currentLight]]);
  Serial.print("Next light: ");
  Serial.println(lightNames[systemState.nextLight]);
  Serial.print("Phase: ");
  switch(systemState.currentPhase) {
    case ALL_RED: Serial.println("All RED"); break;
    case GREEN_ACTIVE: Serial.println("GREEN active"); break;
    case YELLOW_GREEN_TO_RED: Serial.println("YELLOW transition (GREEN to RED)"); break;
    case YELLOW_RED_TO_GREEN: Serial.println("YELLOW transition (RED to GREEN)"); break;
  }
}

void printCurrentOrder() {
  Serial.print("Sequence: ");
  for (int i = 0; i < 5; i++) {
    Serial.print(lightNames[lightOrder[i]]);
    if (i < 4) Serial.print(" -> ");
  }
  Serial.println();
}

void printCurrentDelays() {
  Serial.println("Timings (ms):");
  for (int i = 0; i < 5; i++) {
    Serial.print("  ");
    Serial.print(lightNames[i]);
    Serial.print(": R=");
    Serial.print(lightDelays[i][0]);
    Serial.print(" Y=");
    Serial.print(lightDelays[i][1]);
    Serial.print(" G=");
    Serial.println(lightDelays[i][2]);
  }
}

void sendLightStates() {
  Serial.print("STATE:");
  for (int i = 0; i < 5; i++) {
    Serial.print(lightNames[i]);
    Serial.print(",");
    Serial.print(digitalRead(lights[i][0]) ? "1" : "0"); // RED
    Serial.print(",");
    Serial.print(digitalRead(lights[i][1]) ? "1" : "0"); // YELLOW
    Serial.print(",");
    Serial.print(digitalRead(lights[i][2]) ? "1" : "0"); // GREEN
    if (i < 4) Serial.print(",");
  }
  Serial.println();
}
