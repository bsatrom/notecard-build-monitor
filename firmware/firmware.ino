
// Include the Arduino library for the Notecard
#include <Notecard.h>
#include <Wire.h>

#define BUTTON_PIN          21
#define buttonPressedState  LOW

#define serialDebugOut Serial

#define myProductID "com.blues.bsatrom:build_monitor"
Notecard notecard;

// Light Stack Pin Mappings
#define RED_LIGHT    13
#define BLUE_LIGHT   27
#define GREEN_LIGHT  A1
#define ORANGE_LIGHT A5
#define WHITE_LIGHT 32

// Button handling 
#define BUTTON_IDLE         0
#define BUTTON_PRESS        1
#define BUTTON_DOUBLEPRESS  2

#define ATTN_INPUT_PIN 14

#define INBOUND_QUEUE_NOTEFILE      "build_results.qi"
#define INBOUND_QUEUE_COMMAND_FIELD "result"

static bool attnInterruptOccurred;
String buildStatus = "success";
static bool statusChanged = false;

// Forward Declarations
int getButtonPress(void);
void attnISR(void);
void attnArm(void);
void updateBuildLight(void);
void cycleLights(void);
void allLightsOff(void);
void checkBuildStatus(void);

void setup() {
  pinMode(RED_LIGHT, OUTPUT);
  pinMode(BLUE_LIGHT, OUTPUT);
  pinMode(GREEN_LIGHT, OUTPUT);
  pinMode(ORANGE_LIGHT, OUTPUT);
  pinMode(WHITE_LIGHT, OUTPUT);

  allLightsOff();
  
  pinMode(BUTTON_PIN, buttonPressedState == LOW ? INPUT_PULLUP : INPUT);
  
  serialDebugOut.begin(115200);
  notecard.setDebugOutputStream(serialDebugOut);

  Wire.begin();
  notecard.begin();

  J *req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "product", myProductID);
  JAddStringToObject(req, "mode", "continuous");
  JAddBoolToObject(req, "sync", true);
  JAddNumberToObject(req, "outbound", 60);
  JAddNumberToObject(req, "inbound", 240);
  notecard.sendRequest(req);

  // Disarm ATTN To clear any previous state before rearming
  req = notecard.newRequest("card.attn");
  JAddStringToObject(req, "mode", "disarm,-files");
  notecard.sendRequest(req);

  // Configure ATTN to wait for a specific list of files
  req = notecard.newRequest("card.attn");
  const char *filesToWatch[] = {INBOUND_QUEUE_NOTEFILE};
  int numFilesToWatch = sizeof(filesToWatch) / sizeof(const char *);
  J *filesArray = JCreateStringArray(filesToWatch, numFilesToWatch);
  JAddItemToObject(req, "files", filesArray);
  JAddStringToObject(req, "mode", "files");
  notecard.sendRequest(req);

  // Attach an interrupt pin
  pinMode(ATTN_INPUT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ATTN_INPUT_PIN), attnISR, RISING);

  // Check to see if we have any incoming notes after a reset
  checkBuildStatus();

  // Set build light
  updateBuildLight();
  
  // Arm the interrupt, so that we are notified whenever ATTN rises
  attnArm();
}

void loop() {
  int buttonState = getButtonPress();

  switch (buttonState) {
    case BUTTON_PRESS:
      // Test all lights
      notecard.logDebug("Testing lights...\n\n");
      cycleLights();
      updateBuildLight();
      return;

    case BUTTON_DOUBLEPRESS:
      // Perform an on-demand sync
      notecard.logDebug("Performing a Sync...\n\n");
      J *req = notecard.newRequest("hub.sync");
      notecard.sendRequest(req);
      return;
  }

  // If the interrupt hasn't occurred, exit
  if (!attnInterruptOccurred)
    return;

  // Re-arm the interrupt
  attnArm();

  // Process all pending inbound requests
  checkBuildStatus();    
}

void checkBuildStatus() {
  while (true) {
    J *req = notecard.newRequest("note.get");
    JAddStringToObject(req, "file", INBOUND_QUEUE_NOTEFILE);
    JAddBoolToObject(req, "delete", true);
    J *rsp = notecard.requestAndResponse(req);
    if (rsp != NULL) {
  
      if (notecard.responseError(rsp)) {
        notecard.deleteResponse(rsp);
        break;
      }
  
      J *body = JGetObject(rsp, "body");
      if (body != NULL) {
  
        char *incomingStatus = JGetString(body, INBOUND_QUEUE_COMMAND_FIELD);
        notecard.logDebugf("INBOUND STATUS: %s\n\n", incomingStatus);
  
        // Determine if the status has changed and update accordingly
        if (strcmp(incomingStatus, buildStatus.c_str()) != 0) {
          notecard.logDebugf("Status changed to: %s\n\n", incomingStatus);
          
          buildStatus = String(incomingStatus);
          statusChanged = true;
         
          updateBuildLight();
        }
      }
  
    }
    notecard.deleteResponse(rsp);
  }
}

void allLightsOff() {
  digitalWrite(RED_LIGHT, HIGH);
  digitalWrite(ORANGE_LIGHT, HIGH);
  digitalWrite(GREEN_LIGHT, HIGH);
  digitalWrite(BLUE_LIGHT, HIGH);
  digitalWrite(WHITE_LIGHT, HIGH);
}

void updateBuildLight() {
  allLightsOff();
 
  if (buildStatus == "building") {
    digitalWrite(BLUE_LIGHT, LOW);
    statusChanged = false;  
  } else if (buildStatus == "running_tests") {
    digitalWrite(WHITE_LIGHT, LOW);
    statusChanged = false;  
  } else if (buildStatus == "success") {
    digitalWrite(GREEN_LIGHT, LOW);
    statusChanged = false;  
  } else if (buildStatus == "upload_failed") {
    digitalWrite(ORANGE_LIGHT, LOW);
    statusChanged = false;  
  } else if (buildStatus == "tests_failed") {
    digitalWrite(RED_LIGHT, LOW);
    statusChanged = false;  
  }
}

void cycleLights() {
  digitalWrite(RED_LIGHT, LOW);
  digitalWrite(ORANGE_LIGHT, LOW);
  digitalWrite(GREEN_LIGHT, LOW);
  digitalWrite(BLUE_LIGHT, LOW);
  digitalWrite(WHITE_LIGHT, LOW);
  
  delay(3000);
}

int getButtonPress() {
  static bool buttonBeingDebounced = false;
  int buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != buttonPressedState) {
    if (buttonBeingDebounced) {
      buttonBeingDebounced = false;
    }
    return BUTTON_IDLE;
  }
  if (buttonBeingDebounced)
    return BUTTON_IDLE;

  // Wait to see if this is a double-press
  bool buttonDoublePress = false;
  bool buttonReleased = false;
  unsigned long buttonPressedMs = millis();
  unsigned long ignoreBounceMs = 100;
  unsigned long doublePressMs = 750;
  while (millis() < buttonPressedMs+doublePressMs || digitalRead(BUTTON_PIN) == buttonPressedState) {
    if (millis() < buttonPressedMs+ignoreBounceMs)
      continue;
    if (digitalRead(BUTTON_PIN) != buttonPressedState) {
      if (!buttonReleased)
        buttonReleased = true;
      continue;
    }
    if (buttonReleased) {
      buttonDoublePress = true;
      if (digitalRead(BUTTON_PIN) != buttonPressedState)
        break;
    }
  }

  return (buttonDoublePress ? BUTTON_DOUBLEPRESS : BUTTON_PRESS);
}

void attnISR() {
  attnInterruptOccurred = true;
}

void attnArm() {
  // Make sure that we pick up the next RISING edge of the interrupt
  attnInterruptOccurred = false;

  // Set the ATTN pin low, and wait for the earlier of file modification or a timeout
  J *req = notecard.newRequest("card.attn");
  JAddStringToObject(req, "mode", "reset");
  JAddNumberToObject(req, "seconds", 120);
  notecard.sendRequest(req);
}
