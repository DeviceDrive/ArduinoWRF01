# ArduinoWRF
Arduino WRF is an easy to use wrapper for Arduino Zero with the DeviceDrive WRF01 Arduino Zero shield connected.
This library abstracts all elements of Wifi operations and onboarding functions with simple commands.
To get started fast, just implement your own message callbacks, send data to the cloud and control your device from our mobile app, designed for iOS and Android

## Getting started
Create a WRF object with a Serial port reference, version number, product key, an Introspect document and optionally an Serial port for logging.
The Serial ports are Arduino's own serial interfaces, and you can reference them directly in the function call.

    WRF wrf(
    	&Serial1,			
    	VERSION,
    	PRODUCT_KEY,
    	INTROSPECTION_INTERFACES
    	&Serial
    );

- &Serial1: Pointer to serial port. Automatically initiated to 115200 baud by the library.
- VERSION: Version string for your code, used by the cloud to determine if you need an OTA upgrade. Please increment this number before publishing new versions.
- PRODUCT_KEY: Key issued by DeviceDrive identifying your product in the cloud.
- INTROSPECTION_INTERFACES: Meta information describing the products capabilities.
- &Serial: Log port. Set this if you want WRF to print log to Arduino serial monitor. Set to NULL to disable logging.

A prerequisite for logging is that the serial port is initialized in the setup routine. The communication port for WRF (Serial1) is handled in the library and your should not worry about this

    void setup() {
        ...
        Serial.begin(115200); //Log port
        ...
    }

- A product key is obtained from [devicedrive.com/subscription/](https://devicedrive.com/subscription/). This key identifies the product in the DeviceDrive cloud. This must be a valid key issued by DeviceDrive, or all communication will fail.
- The Introspect is a JSON object that describes your device and its capabilities. See the [Serial Specification](https://devicedrive.com/downloads/).


	  #define INTROSPECTION_INTERFACES [[\"com.devicedrive.light\",\
	                                    \"@status>s\",\
	                                    \"@power=b\"]]"

### setup()
In Arduino's setup() routine, define WRFConfig as you like, and start the communication with the WRF with wrf.setup(wrfConfig). If no changes are made on wrfConfig, default settings will be used. When the WRF shield has received it's parameters, it will try to connect to your WiFi if SSID and password is already saved.
If no SSID or password are already present, a [Linkup](#linkup) sequence is required.

    WRFConfig wrfConfig;
	  setup() {
		   ...
		   wrf.setup(wrfConfig); // Using default WRF settings.
		   ...
		   wrf.connect();
	  }


In addition, the setup() routine should also include your defined callbacks. A callback is a function that is called when a given event occurs. The WRF Arduino library contain the following events:

    void setup{
      ...
      wrf.onError(errorCallback);
      wrf.onConnected(connectCallback);
      wrf.onMessageSent(sentMessageCallback);
      wrf.onMessageReceived(messageCallback);
      wrf.onPendingUpgrade(upgradeCallback);
      wrf.onNotConnected(disconnectedCallback);
      wrf.onStatusReceived(wrfStatusCallback);
      wrf.overrideOnStart(startUpCallback);
      ...
    }

- onError : Triggered if the WRF encounters an error of some kind.
- onConnected : Triggered when the WRF connects to a network
- onMessageSent : Triggered when a message is sent and understood by the WRF
- onMessageReceived : Triggered when a message is delivered to the WRF from the cloud, e.g the mobile App
- onPendingUpgrade : Triggered if there is an upgrade waiting for either the WRF or your client code.
- onNotConnected : Triggered if you send an message while the WRF is not connected to a network.
- onStatusReceived : Triggered if you ask for the WRF status with given command.
- overrideOnStart : Triggered when the wrf has started. This is handeled in the library if not set. Note that this is an override, and needs to be handeled correctly if overriden to handle recover and upgrades of wrf. 


The setup() function might look like this (from the example provided in LightSwitch.ino)

    #define VERSION "1.0"
    #define PRODUCT_KEY "some-key-here"
    #define INTROSPECTION_INTERFACES [[\"com.devicedrive.light\",\
	                                    \"@status>s\",\
	                                    \"@power=b\"]]"

    ...

    WRF wrf(&Serial1, VERSION,PRODUCT_KEY, INTROSPECTION_INTERFACES, &Serial);
    WRFConfig wrfConfig;
    setup() {
        Serial.begin(115200); // Log port

        wrf.setup(wrfConfig); // Default settings

        wrf.onError(handleError);
        wrf.onConnected(connectedToWifi);
        wrf.onMessageSent(handleMessageSent);
        wrf.onMessageReceived(handleMessage);
        wrf.onPendingUpgrades(handleUpgrades);
        wrf.onNotConnected(notConnectedToWifi);
        wrf.onStatusReceived(handleWRFStatus);

        wrf.connect();
    }


### loop()
The wrf.loopHandler() must be called in your loop() routine. All communication with the WRF and the cloud are done asynchronously, so the library relies on your loop to run it's callbacks. NOTE: Avoid using delay(ms), as this prevents the communication flow by blocking the loop() routine. If waiting is required, implement a timer callback instead.

    loop() {
	    wrf.loopHandler();
      ...
    }

The rest of the loop routine is your own code and implementation of your product.

#### Optional WRF settings with WRFConfig

WRFConfig defines how the WRF should behave in certain situations. The following example describes the default settings, but there are optional settings that can be set.

    WRFConfig wrfConfig;

    void setup() {
      ...
      wrfConfig.ssid_prefix = "DeviceDrive";
      wrfConfig.ssl_enabled = true;
      wrfConfig.debug_mode = DEBUG_ALL;
      ...
    }

More commands can be found in the [serial specification](https://devicedrive.com/downloads/)

* ssid_prefix: The SSID prefix to apply next time when the WRF01 is set visible. E.g. "MyPrefix" will give "MyPrexix_aa:bb:cc:dd" (abcd is part of the mac address). If you are using our app, set this to default "DeviceDrive" as the Linkup SDK looks for the prefix in its filtering of SSID's
* ssl_enabled: To be able to communicate with your device from the app and to encrypt data, set this value to true. If you for some reason don't want to encrypt your data, it can be set to false. This will also disable the ability to use the mobile application.
* debug_mode: Defines what level of debug messages you want printed on the secondary USB port on the Arduino Shield. Options are:
  * DEBUG_ALL
  * DEBUG_NONE


### Linkup
DeviceDrive has developed it's own form of onboarding, called Linkup. From the client code, you can tell the WRF to enter "visibility mode", where the WRF broadcasts a local network, on which an app or a browser can connect and transfer SSID, password, and device token. The device token is issued by the DeviceDrive Cloud.
All of this logic is abstracted away from the client.

#### Handle Visibility (Access Point Mode (AP))

LinkUp is required when the WRF is new without SSID or password for your network, or you move it to a new network.
To enable the WRF Access point and prepare for the LinkUp procedure send:

	wrf.prepareLinkupMode(seconds);

An access point with the configured prefix will be broadcasted (wrfConfig has default "DeviceDrive").
The parameter "seconds" indicate how long the AP should be visible.
The linkup mode will enable the AP, and makes sure that you get the onConnected callback when the user has finished the linkup procedure with the app. If no linkup is performed, the WRF will turn
off the AP mode and go back to normal operations.
During LinkUp, the WRF is prevented from all messaging until the onConnected callback is triggered.

Instead of using the prepareLinkupMode() function, you can control the AP mode directly, by issuing wrf.setVisibility(int seconds). Linkup will also be available in this mode, but the WRF messaging might then conflict with the linkup process.

### Send and Receive
ArduinoWRF library handles the messages you want to send in a queue, so you do not need to wait for response in your client code. Both messages and commands uses this queue. The message queue can hold 10 messages. If you fill up the queue, an error will be triggered to your wrf.onError callback.
In this callback, you can issue the following command to clear the queue.

    wrf.clearMessageQueue();

##### Sending Message to App/Cloud
Before sending a message, it might be wise check if the WRF is online. This can be done by checking the boolean value of the function :

	wrf.isOnline();

This is not really necessary as a callback will be triggered on wrf.onNotConnected(callback), and you can handle the logic there.

Your messages to the cloud and app must match the introspect defined in the setup routine of your program. The reason is that the mobile app uses the introspect to identify the fields and commands available in the app and presented to the user.
To send a status message, you must create an JSON string with the correct parameters and values, then issue

	 wrf.sendMessage(JsonObject& message);

or

    wrf.sendMessage(String json_string);

To build a status message, you can use the ArduinoJSON library. See this example from (LightSwitch.ino):

	#define INTERFACE_NAME "com.devicedrive.light"
	#define STATUS_PARAM "status"
	#define POWER_PARAM "power"

	int power = 0;
	String state = "Off";

	...

	void sendStatus() {
		StaticJsonBuffer<200> jsonBuffer;
		JsonObject& status = jsonBuffer.createObject();
		JsonObject& params = status.createNestedObject(INTERFACE_NAME);
		params[STATUS_PARAM] = state;
		params[POWER_PARAM] = power;

		wrf.sendMessage(status);
	}

Or as a string:

    String message = "{\"com.devicedrive.light\":{"state\": \"Off\",\"power\": 1}}"
    wrf.sendMessage(message);

Based on the Introspection Document, the parameters are shown either as strings, numbers or buttons with a boolean value.

##### Sending command
The same way as checking if the WRF is ready to send a message, you can check if it's ready to send a command.

	wrf.canSendCommand();

This is redundant as the message queue should handle this, but it is useful to avoid filling up the queue.

##### Available commands

- wrf.prepareLinkupMode(seconds)
- wrf.setVisibility(seconds)
- wrf.getStatus()
- wrf.init()
- wrf.connect()
- wrf.checkPendingUpgrades()
- wrf.startClientUpgrade()
- wrf.startWrfUpgrade()
- wrf.sendMessage(jsonObject)
- wrf.sendMessage(jsonString)
- wrf.sendIntrospect()
- wrf.poll()
- wrf.setPollInterval(seconds)
- wrf.setup()

##### Receiving message
To receive a message, the wrf.loopHandler(); must be present in your loop function, and you must either call

	wrf.poll();

in the loop, or you can call the following function in your setup routine:

	wrf.setPollInterval(seconds);

setPollInterval(seconds) will enable the auto poll feature, and make a poll request to the cloud every given second.
A recommended value for this is around 5 seconds.
Each time a message is received, your callback defined in onMessageReceived(callback) will be triggered.
This might be handled like this:

	void handleMessage(String interface_name, String json_string) {
		Serial.println("Message received");
		// When we receive a message with our interface we handle it
		if (interface_name == INTERFACE_NAME)
		{
			StaticJsonBuffer<200> jsonBuffer;
			JsonObject& light_interface = jsonBuffer.parseObject(json_string);
			if (light_interface.containsKey(POWER_PARAM))
			{
				setLight(light_interface[POWER_PARAM]);
			}
		}

		// Whenever we have had communications with the
		// server we check if there is an upgrade pending
		wrf.checkPendingUpgrades();
	}

Notice that we check for upgrades every time we receive a message.
The upgrade information is sent from the cloud along with every message, so this does not trigger a separate cloud communication.

### OTA Upgrades

There are two kinds of upgrades available, either WRF01 or CLIENT upgrade. For the Arduino client code these act a bit differently.
To make sure that there is an upgrade available for the Arduino or the WRF01, send a message to your device from your mobile app. When the device receives a message from the cloud, an upgrade message is obtained
if available. The Arduino code can check this by issuing

	wrf.checkPendingUpgrades();

This will trigger your callback for updates

	wrf.onPendingUpgrades(upgradeCallback);

In this callback there is a reference to a list of upgrades. Based on this list, your Arduino software may decide which module to upgrade, or to postpone upgrades.
From our example (LightSwitch.ino), a handler for the upgrade callback:

	#define WRF_UPGRADE WRF01
	#define CLIENT_UPGRADE CLIENT

	...

	void setup() {
		...
		wrf.onPendingUpgrades(handleUpgrades);
		...
	}
	...

	void handleUpgrades(List &pending_upgrades) {
		int size = wrf.getListSize(pending_upgrades);
    	for (int i = 0; i < size; i++) {
    		Serial.println("Pending Upgrade: " + pending_upgrades[i]);
    	}

    	if (size > 0) {
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

#### WRF Upgrade
If there is an upgrade pending for the WRF01, just issue the command

	wrf.startWrfUpgrade();

This will start the upgrade procedure, and the WRF will reboot. All settings like SSID, network password, and token will be saved prior to the upgrade, so there is no need to run LinkUp again.

#### Client Upgrade
If there is an upgrade pending for the Client code, there are some other parameters to handle. For the Arduino Zero, please issue:

	wrf.startClientUpgrade(0, "ARDUINO_ZERO", 1000, "010");

The parameters are represented like this

	wrf.startClientUpgrade(file_no, protocol, delay, pin_toggle);

- file_no : The file number in the bundle you uploaded to the cloud service. In most cases with an Arduino, this is always 0.
- protocol : The WRF supports RAW and ARDUINO_ZERO as OTA protocols. Disregard RAW when working with this library and use ARDUINO_ZERO. Arduino Zero uses Atmels SAM-BA protocol with XMODEM for file transfers, both of which are included in the WRF firmware.
- delay : Time to wait from the point where the firmware is downloaded to the WRF from cloud to the point where the actual upgrade begins. Use this time to notify user with an LED or display that an upgrade is in progress...
- pin_toggle : To enter the boot loader on an Arduino Zero, the reset pin needs to be toggled from high to low two times. This is done with "010" where each character represents a state in 5ms. If you are not using the WRF Arduino Shield, but working with an WRF01 Module, please make sure that GPIO4 is connected to the Arduino Reset pin.

This procedure will erase the whole memory on your Arduino. If it should fail while upgrading, your Arduino sketch must be uploaded with the USB interface from Arduino IDE or another Arduino compatible IDE.

---
# Available resources
Some pins on the Arduino are being used by the WRF Arduino Shield.
#### Free pins

- Analog pins
  * A1
  * A2
  * A3
  * A4
  * A5
- Digital pins
   * D2
   * D3
   * D4
   * D10
   * D11
   * D12
   * D13

#### Power
The WRF shield can draw up to 130mA in peak. We get our power from the 5V output on the Arduino. If you have other shields that draw power from this, make sure that it's enough power for the WRF to operate.

---
# Examples
A simple example is included in the library. It is currently used for setting pin 10 high / low on request from the DeviceDrive App and with a possibility to add a pushbutton for long and short presses on pin 2.

---
# Third party components
The library uses :
- ArduinoJson by bblanchon (https://github.com/bblanchon/ArduinoJson)

These components are referenced in the library.
