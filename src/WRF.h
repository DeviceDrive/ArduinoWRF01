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

#pragma once

#include <Arduino.h>
#include "WRFConfig.h"
#include "ArduinoJson/ArduinoJson.h"
#include "StringQueue.h"

#define MAX_DICTIONARY_SIZE 8
#define MAX_LIST_SIZE 8
typedef String KeyValuePair[2];
typedef KeyValuePair Dictionary[MAX_DICTIONARY_SIZE + 1];
typedef String List[MAX_LIST_SIZE + 1];

typedef void WrfCallback();
typedef void WrfStartUpCallback(WRFConfig &config);
typedef void WrfMessageReceivedCallback(String interfaceName, String json_string);
typedef void WrfErrorCallback(String errorMessage);
typedef void WrfUpgradeCallback(List &pending_upgrades);

#define PROTOCOL_RAW "RAW"
#define DEFAULT_PROTOCOL PROTOCOL_RAW
#define DEFAULT_TOGGLE ""
#define DEFAULT_DELAY 0
#define DEFAULT_FILE_NO 0
#define END_OF_LIST ""
#define END_OF_DICTIONARY {END_OF_LIST,END_OF_LIST}
#define SEND_QUEUE_LEN 10

#define STX_CHAR ((char)0x02)
#define ETX_CHAR ((char)0x03)
#define EOT_CHAR ((char)0x04)

class WRF
{
	public:
		WRF(HardwareSerial *serial, String version, String product_Key, String introspect, HardwareSerial *logPort = NULL, int pollInterval = 0);
		void setup(WRFConfig &config);
		void loopHandler();

		void setupParam(String param, String value);
        void connect();
		void prepareLinkupMode(int vivibility_seconds);

		void poll();
		void setPollInterval(int seconds);
		
		void checkPendingUpgrades();
		void startWrfUpgrade();
		void startClientUpgrade(int file_no = DEFAULT_FILE_NO, String protocol = DEFAULT_PROTOCOL, int delay_millis = DEFAULT_DELAY, String toggle_pattern = DEFAULT_TOGGLE);

		void send(String raw_string);
		bool canSendCommand();
        bool isOnline();
        void sendMessage(JsonObject &msg);
		void sendMessage(String msg);
        void sendCommandWithoutParams(String command);
        void sendCommand(String command, Dictionary params);
		void sendCommand(String command, String param);
		void sendIntrospect();

		void setVisibility(int seconds);
		void getStatus();
		WRFConfig getConfig();

		void onError(WrfErrorCallback *error_cb);
		void onConnected(WrfCallback *connection_cb);
		void onMessageSent(WrfCallback *message_sent_cb);
		void overrideOnStart(WrfStartUpCallback *start_cb);
		void onMessageReceived(WrfMessageReceivedCallback *message_received_connection_cb);
		void onPendingUpgrades(WrfUpgradeCallback *pending_upgrades_cb);
		void onNotConnected(WrfCallback *not_connected_cb);
		void onStatusReceived(WrfMessageReceivedCallback *status_received_cb);

		void clearMessageQueue();
        int getListSize(List & list);
        void addToList(List & list, String item);
        void addToDictionary(Dictionary & dictionary, String key, String value);
       
	private:
        WrfCallback *connection_cb = NULL;
        WrfCallback *message_sent_cb = NULL;
		WrfCallback *not_connected_cb = NULL;
		WrfStartUpCallback *start_cb = NULL;
        WrfErrorCallback *error_cb = NULL;
        WrfMessageReceivedCallback *message_received_cb = NULL;
		WrfMessageReceivedCallback *status_received_cb = NULL;
        WrfUpgradeCallback *pending_upgrades_cb = NULL;

        bool is_connected = false;
		bool is_visible = false;
        bool awaiting_response = false;

		WRFConfig config;
		String product_key;
		int baud_rate;
		StringQueue message_queue;

		int len = 0;
		String introspect;
        String version;

		int crc_wrf;
		HardwareSerial * serial;
		HardwareSerial * log_port;
		String received_message;
		void log_message(String msg);
		void log_message(int data);

		unsigned long pollInterval = 0;
		unsigned long current_time = 0;
		unsigned long previous_time = 0;

		unsigned long awaiting_linkup = false;
		unsigned long linkup_timeout = 0;


		void automaticPoll();
		void triggerSentMessage();
		void handleMessageQueue();
		void triggerStartup();
		void handleStartup(WRFConfig &config);
        void handleConfiguration(JsonObject &configuration);
        void handleResultMsg(String result);
        void handleReceivedMessage(JsonObject &message_obj);
        void handleUpgradeMsg(JsonArray & message_obj);
        void handleWrfMessage(String & msg);
        void handleSerialInput();
		void handleErrorMsg(String error_msg);
		void handleStatusMsg(JsonObject& status_object);
		void handleAutomaticPoll();
		void handleLinkupTimeout();
        JsonObject& deserialize(String msg);
		String serializeConfigData(WRFConfig &config);
		String generateIntrospectDocument();
		
		
		unsigned int* createCrcTable(void);
		unsigned int crc32(unsigned int crc, unsigned int* crc_table, unsigned char* buffer, unsigned int size);
		unsigned int calcCrc(unsigned char* buffer, int size);
		static void merge(JsonObject& dest, JsonObject& src);
};
