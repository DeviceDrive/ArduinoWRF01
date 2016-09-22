/*	Copyright 2016 DeviceDrive AS
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*	http ://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License.
*
*/

#include <ArduinoWRF01.h>
#include "stdio.h"

//TODO: Please get your product key at https://devicedrive.com/subscription
#define PRODUCT_KEY "<Your-product-key-here>"

//Please refer to the serial specification for details on introspection syntax
//In this example we define one interface with two properties, status and power.
//  *  "status" is defined as read only
//  *  "power" is defined as read/write property
//TODO: Please update with your company name in the interface
#define INTROSPECTION_INTERFACES "[[\"com.devicedrive.light\",\
								\"@status>s\",\
								\"@power=b\"]]"

#define INTERFACE_NAME "com.devicedrive.light"
#define STATUS_PARAM "status"
#define POWER_PARAM "power"

#define STATUS_VISIBILITY "local_visibility"

#define ERROR_INVALID_TOKEN "INVALID_TOKEN"
#define ERROR_SYSTEM_BUSY "SYSTEM_BUSY"

#define WRF_UPGRADE "WRF01"
#define CLIENT_UPGRADE "CLIENT"

#define TEXT_ON "On"
#define TEXT_OFF "Off"

#define VERSION "1.0"
#define LED_PIN 10		// Define our light at pin 10
#define PUSH_PIN 2		// Define our button at pin 2

#define POLLING_INTERVAL 5

#define VISIBILITY_TIMEOUT 90	// Define how long the WRF should act as a Access Point

WRF wrf(
	&Serial1,					// Communication port to the WRF, baud 115200
	VERSION,					// Version of your code,it's used to determine if you need an OTA upgrade. Please itereate this number accordingly.
	PRODUCT_KEY,				// ProductKey
	INTROSPECTION_INTERFACES,	// Introspect
	&Serial						// Log port. Set this if you want WRF to print log to arduino serial monitor.
);
WRFConfig wrfConfig;	// Configuration of how you want to set up the WRF01

int power = 0;							// Indicates if the light is on or off
String state = TEXT_OFF;				// Status text to be sent to app
bool introspect_sent = false;			// Flag to make sure introspect is only sent once on startup
bool is_visible = false;				// Indicates if the WRF is an Acesse Point	
bool linkup_required = false;			// Indicates thta it startet with button pressed down.

// For button handeling Should be moved to a library!
typedef void ButtonPressCallback();
ButtonPressCallback *button_press_cb = NULL;
ButtonPressCallback *button_long_press_cb = NULL;
bool button_was_pressed; // previous state
int button_pressed_counter; // press running duration
void onButtonPress(ButtonPressCallback * callback);
void onButtonLongPress(ButtonPressCallback * callback);

void setup() {
  // We start with initializing our log port to 115200
  Serial.begin(115200);
  Serial1.begin(115200);
  // Now we set the pins for our button and light 
  pinMode(LED_PIN, OUTPUT);			// LED
  pinMode(PUSH_PIN, INPUT_PULLUP);	// Button

  //Defining our wrf.  
  wrfConfig.ssid_prefix = "DeviceDrive";

  // Applying our config when the wrf is ready
  wrf.setup(wrfConfig);

  // Now we can set our callbacks to handle the wrf.
  wrf.onError(handleError);
  wrf.onConnected(connectedToWifi);
  wrf.onMessageSent(handleMessageSent);
  wrf.onMessageReceived(handleMessage);
  wrf.onPendingUpgrades(handleUpgrades);
  wrf.onNotConnected(notConnectedToWifi);
  wrf.onStatusReceived(handleWRFStatus);

  //Setting up button handler
  onButtonPress(buttonPressed);
  onButtonLongPress(buttonLongPressed);

  Serial.println("Setup Complete");

  //We want to connect from start
  wrf.connect();
}

void loop() {
  wrf.loopHandler();	// Polls new messages and triggers event handlers
  handle_button();		// Handels the pins set for buttons and triggers event handlers	
  delay(1);
}

void handleError(String error_msg) {
	Serial.println("WRF ERROR -->" + error_msg);
	if (error_msg == ERROR_INVALID_TOKEN) {
		prepareLinkup();
	}
	else if (error_msg != ERROR_SYSTEM_BUSY) {
		wrf.clearMessageQueue();
		wrf.getStatus();
	}
}

void connectedToWifi() {
	Serial.println("Connected To Wifi");
	if (!introspect_sent) {
		wrf.sendIntrospect();
		introspect_sent = true;
		sendStatus();
	}
	linkup_required = false;
	wrf.setPollInterval(POLLING_INTERVAL);
}

void notConnectedToWifi() {
	Serial.println("Not connected to wifi");
	wrf.setPollInterval(0);
	// We should set WRF visible if it is not already visible.
	if (!is_visible) {
		linkup_required = true;
		wrf.getStatus();
	}
	else if (linkup_required)
		prepareLinkup();
}

void handleMessageSent() {
	Serial.println("Mesage Sent!");
}

void handleMessage(String interface_name, String json_string) {
	Serial.println("Message received");
	// When we recive a message with our interface we handle it
	if (interface_name == INTERFACE_NAME)
	{
		StaticJsonBuffer<200> jsonBuffer;
		JsonObject& light_interface = jsonBuffer.parseObject(json_string);
		if (light_interface.containsKey(POWER_PARAM))
		{
			setLight(light_interface[POWER_PARAM]);
		}
	}

	// Whenever we have had comunications with the 
	// server, we check if there is an upgrade pending
	wrf.checkPendingUpgrades();
}

void handleUpgrades(List &pending_upgrades) {
	int size = wrf.getListSize(pending_upgrades);
	for (int i = 0; i < size; i++) {
		Serial.println("Pending Upgrade: " + pending_upgrades[i]);
	}

	if (size > 0) {
		wrf.setPollInterval(0); 

		if (pending_upgrades[0] == WRF_UPGRADE) {
			Serial.println("Upgrading WRF..");
			wrf.startWrfUpgrade();
		}
		else if (pending_upgrades[0] == CLIENT_UPGRADE) {
			Serial.println("Upgrading CLIENT..");
			wrf.startClientUpgrade(0, "ARDUINO_ZERO", 1000, "010");
		}
	}
}

void handleWRFStatus(String interface_name, String json_string) {
	Serial.println("Status Received");
	StaticJsonBuffer<512> buffer;
	JsonObject& status = buffer.parseObject(json_string.c_str());

	if (status.containsKey(STATUS_VISIBILITY)) {
		if ((String)status[STATUS_VISIBILITY].asString() == "OFF" && linkup_required) {
			prepareLinkup();
		}
	}
}

void sendStatus() {
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& status = jsonBuffer.createObject();
	JsonObject& params = status.createNestedObject(INTERFACE_NAME);
	params[STATUS_PARAM] = state;
	params[POWER_PARAM] = power;

	wrf.sendMessage(status);
}

void prepareLinkup() {
	Serial.println("Preparing for LinkUp");
	wrf.setPollInterval(0);
	wrf.prepareLinkupMode(VISIBILITY_TIMEOUT);
}

void buttonPressed() {
	setLight(!power);
}
void buttonLongPressed() {
	prepareLinkup();
}

void setLight(bool on) {
	digitalWrite(LED_PIN, on);
	power = on;
	if (power)
		state = TEXT_ON;
	else state = TEXT_OFF;

	Serial.println("Power : " + state);
	sendStatus();
}

// END OF EXAMPLE! // REMOVE AFTER BUTTON LIB IS CREATED!


// TODO CREATE own library
#define LONGPRESS 5000
#define SHORTPRESS 50


void onButtonPress(ButtonPressCallback * callback)
{
	button_press_cb = callback;
}
void onButtonLongPress(ButtonPressCallback * callback) {
	button_long_press_cb = callback;
}

void handle_button()
{
  int button_now_pressed = !digitalRead(PUSH_PIN); // pin low -> pressed
  // Trigger current event
  if (!button_now_pressed && button_was_pressed) {
    if (button_pressed_counter < LONGPRESS) {
      if (button_pressed_counter > SHORTPRESS) {
		  if(button_press_cb != NULL)
			button_press_cb();
      }
    } else {
		if(button_long_press_cb != NULL)
			button_long_press_cb();
    }
  } 
  // Tracking button push
  if (button_now_pressed) {
    button_pressed_counter++;
  } else {
    button_pressed_counter = 0;
  }
  button_was_pressed = button_now_pressed;
}
