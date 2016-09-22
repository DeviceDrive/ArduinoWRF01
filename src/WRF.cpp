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

#include <Arduino.h>
#include "WRF.h"

#define JSON_COMMAND_MAX_SIZE 512

#define DEVICEDRIVE_LOCAL "devicedrive"
#define DEVICEDRIVE_REMOTE "DeviceDrive"

#define DEVICEDRIVE_ERROR "error"
#define DEVICEDRIVE_UPGRADE "upgrade"
#define CONFIGURATION "configuration"
#define DEVICEDRIVE_RESULT "result"
#define DEVICEDRIVE_STATUS "status"

#define DEVICEDRIVE_INTERFACES "interfaces"

#define STATUS_CONNECTION_STATUS "connection_status"
#define STATUS_LOCAL_VISISBILITY "local_visibility"
#define STATUS_GOT_IP "GOT_IP"
#define STATUS_CONNECTING "CONNECTING"
#define STATUS_ON  "ON"

#define ERROR_CODE "ErrorCode"
#define ERROR_INVALID_TOKEN "INVALID_TOKEN"
#define ERROR_SYSTEM_BUSY "SYSTEM_BUSY" 
#define ERROR_MSG_QUEUE_FULL "Message queue is full"
#define ERROR_UPGRADE "UPGRADE_ERROR"

#define RESULT_SENT "SENT"
#define RESULT_OK "OK"

#define COMMAND_SETUP "setup"
#define COMMAND_CHECK_UPGRADE "check_upgrade"
#define COMMAND_GET_UPGRADE "get_upgrade"
#define COMMAND_INTROSPECT "introspect"

#define PARAM_VISIBILITY "visibility"
#define PARAM_SILENT_CONNECT "silent_connect"

#define PLOYNOMIAL 0xedb88320
#define BUFSIZE     512
#define CRC_BLOCK_SIZE 512

WRF::WRF(HardwareSerial *serial, String version, String productKey, String introspect, HardwareSerial *log_port /*=NULL*/, int pollInterval) {
	this->product_key = productKey;
	this->serial = serial;
    this->version = version;
    this->baud_rate = 115200;
    this->introspect = introspect;
    this->log_port = log_port;
	this->pollInterval = pollInterval;
	this->message_queue = StringQueue();
}

void WRF::setup(WRFConfig &config) {
	serial->begin(baud_rate);
	this->config = config;
	String command = serializeConfigData(this->config);
	send(command);
}

void WRF::loopHandler()
{
	this->current_time = millis();
	handleSerialInput();
	handleLinkupTimeout();
	handleAutomaticPoll();

	handleMessageQueue();

}

void WRF::setupParam(String param, String value)
{
	Dictionary params = {
		{ param,value },
		END_OF_DICTIONARY
	};
	sendCommand(COMMAND_SETUP, params);
}

void WRF::connect()
{
	setupParam(PARAM_SILENT_CONNECT, "0");
}

void WRF::prepareLinkupMode(int vivibility_seconds)
{
	is_connected = false;
	awaiting_linkup = true;
	linkup_timeout = current_time + ((vivibility_seconds +1) * 1000);

	Dictionary params = {
		{ PARAM_SILENT_CONNECT, "1" },
		{ PARAM_VISIBILITY, String(vivibility_seconds)},
		END_OF_DICTIONARY
	};
	sendCommand(COMMAND_SETUP, params);
}

void WRF::poll() {
	if (isOnline()) {
		log_message("Polling");
		send(String());
	}
	else {
		log_message("Unable to poll");
	}
}

void WRF::automaticPoll()
{
	if (current_time - previous_time >= pollInterval) {
		previous_time = current_time;
		poll();
	}
}

void WRF::setPollInterval(int seconds)
{
	this->pollInterval = seconds* 1000;
}

void WRF::checkPendingUpgrades()
{
	sendCommandWithoutParams(COMMAND_CHECK_UPGRADE);
}

void WRF::startWrfUpgrade()
{
	String params[][2] = {
		{ "module","WRF01" },
		END_OF_DICTIONARY
	};
	sendCommand(COMMAND_GET_UPGRADE, params);
}

void WRF::startClientUpgrade(int file_no, String protocol, int delay_millis, String toggle_pattern)
{
	Dictionary params = {
		{ "module", "CLIENT" },
		{ "protocol", protocol },
		{ "file_no", String(file_no) },
		END_OF_DICTIONARY
	};
	if (delay_millis != DEFAULT_DELAY)
		addToDictionary(params, "delay", String(delay_millis));
	if (toggle_pattern != DEFAULT_TOGGLE)
		addToDictionary(params, "pin_toggle", toggle_pattern);
	sendCommand(COMMAND_GET_UPGRADE, params);
}

void WRF::send(String raw_string) {
	bool message_pushed = message_queue.push_back(raw_string);
	handleMessageQueue();
	if (!message_pushed)
	{
		if(error_cb != NULL)
			error_cb(ERROR_MSG_QUEUE_FULL);
	}
}

bool WRF::canSendCommand()
{
	return !awaiting_response;
}

bool WRF::isOnline()
{
	return is_connected && !is_visible;
}

void WRF::sendMessage(JsonObject & msg)
{
	String json;
	msg.printTo(json);
	sendMessage(json);
}

void WRF::sendMessage(String msg) {
		send(msg + ETX_CHAR);
}

void WRF::sendCommandWithoutParams(String command)
{
	Dictionary no_params = { END_OF_DICTIONARY };
	sendCommand(command, no_params);
}

void WRF::sendCommand(String command, Dictionary params)
{
	StaticJsonBuffer<JSON_COMMAND_MAX_SIZE> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	JsonObject& json_command = root.createNestedObject("devicedrive");
	json_command["command"] = command;

	for (int i = 0; i < MAX_DICTIONARY_SIZE; i++)
	{
		if (params[i][0] == END_OF_LIST)
			break;
		json_command[params[i][0]] = params[i][1];
	}

	String send_string;
	root.printTo(send_string);
    send(send_string);
}

void WRF::sendCommand(String command, String param)
{
	StaticJsonBuffer<JSON_COMMAND_MAX_SIZE> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	JsonObject& json_command = root.createNestedObject("devicedrive");
	json_command["command"] = command;

	StaticJsonBuffer<JSON_COMMAND_MAX_SIZE> buffer;
	param = "{" + param + "}";
	JsonObject& introspect_object = buffer.parseObject(param);
	merge(json_command, introspect_object);

	String send_string;
	root.printTo(send_string);
	send(send_string);
}

String WRF::generateIntrospectDocument()
{
	return String("\"interfaces\":" + introspect);
}

void WRF::sendIntrospect() {
	String introspect_document = generateIntrospectDocument();
	sendCommand(COMMAND_INTROSPECT, introspect_document);
}

void WRF::setVisibility(int seconds) {
    String secondsStr(seconds);
    setupParam("visibility", secondsStr);
}

void WRF::getStatus()
{
	sendCommandWithoutParams(DEVICEDRIVE_STATUS);
}

WRFConfig WRF::getConfig()
{
	return config;
}

void WRF::onError(WrfErrorCallback * error_cb)
{
	this->error_cb = error_cb;
}

void WRF::onConnected(WrfCallback * connection_cb)
{
	this->connection_cb = connection_cb;
}

void WRF::onMessageSent(WrfCallback * message_sent_cb)
{
	this->message_sent_cb = message_sent_cb;
}

void WRF::overrideOnStart(WrfStartUpCallback * start_cb)
{
	this->start_cb = start_cb;
}

void WRF::onMessageReceived(WrfMessageReceivedCallback * message_received_cb)
{
	this->message_received_cb = message_received_cb;
}

void WRF::onPendingUpgrades(WrfUpgradeCallback * pending_upgrades_cb)
{
	this->pending_upgrades_cb = pending_upgrades_cb;
}

void WRF::onNotConnected(WrfCallback * not_connected_cb)
{
	this->not_connected_cb = not_connected_cb;
}

void WRF::onStatusReceived(WrfMessageReceivedCallback * status_received_cb)
{
	this->status_received_cb = status_received_cb;
}

void WRF::clearMessageQueue()
{
	awaiting_response = false;
	message_queue.clear();
}

int WRF::getListSize(List &list)
{
	int i = 0;
	while (list[i] != END_OF_LIST && i<MAX_LIST_SIZE)
		i++;
	return i;
}

void WRF::addToList(List &list, String item)
{
	int i = getListSize(list);

	if (i >= MAX_LIST_SIZE - 1)
	{
		log_message("Max list size reached. Cannot add item: " + item);
		return;//Error: Max size reached
	}
	//Add new entry
	list[i++] = item;
	list[i] = END_OF_LIST;
}

void WRF::addToDictionary(Dictionary &dictionary, String key, String value)
{
	int i;
	//Replace existing value if any
	for (i = 0; i < MAX_DICTIONARY_SIZE; i++)
	{
		String current_key = dictionary[i][0];
		if (current_key == key)
		{
			dictionary[i][1] = value;
			return;
		}
		if (current_key == END_OF_LIST)
			break;
	}
	if (i >= MAX_DICTIONARY_SIZE - 1)
	{
		log_message("Max dictionary size reached. Cannot add key: " + key);
		return;//Error: Max size reached
	}
	//Add new entry
	dictionary[i][0] = key;
	dictionary[i][1] = value;
	i++;
	//Terminate list
	dictionary[i][0] = END_OF_LIST;
	dictionary[i][1] = END_OF_LIST;
}


void WRF::handleErrorMsg(String error_msg)
{
    log_message("Error message: " + error_msg);
	if (error_cb != NULL)
		error_cb(error_msg);
}

void WRF::handleStatusMsg(JsonObject& status_obj)
{
	bool previous_online_status = isOnline();

	if (status_obj.containsKey(STATUS_CONNECTION_STATUS)) {
		if (String(status_obj[STATUS_CONNECTION_STATUS].asString()) == STATUS_GOT_IP)
			is_connected = true;
		else if (String(status_obj[STATUS_CONNECTION_STATUS].asString()) != STATUS_CONNECTING)
			is_connected = false;
	}
	if (status_obj.containsKey(STATUS_LOCAL_VISISBILITY)) {
		if (String(status_obj[STATUS_LOCAL_VISISBILITY].asString()) == STATUS_ON) 
			is_visible = true;
		else
			is_visible = false;
	}

	if (!previous_online_status && isOnline() && connection_cb != NULL) {
		connection_cb();
	}
	else if (previous_online_status && !isOnline() && not_connected_cb != NULL) {
		not_connected_cb();
	}

	if (awaiting_linkup && !is_visible)
		awaiting_linkup = is_visible;

	if (status_received_cb != NULL) {
		String status_string;
		status_obj.printTo(status_string);
		status_received_cb(DEVICEDRIVE_STATUS, status_string);
	}
}

void WRF::handleAutomaticPoll()
{
	if (pollInterval != 0)
	{
		automaticPoll();
	}
}

void WRF::handleLinkupTimeout()
{
	if (awaiting_linkup && current_time > linkup_timeout) {
		awaiting_linkup = false;
		if (!isOnline()) {
			getStatus();
		}
	}
}

void WRF::handleConfiguration(JsonObject &configuration)
{
	// Need to check if AP is off to be sure we kan send messages. 
	getStatus();
}


void WRF::triggerSentMessage()
{
    if (message_sent_cb != NULL)
        message_sent_cb();
}

void WRF::handleMessageQueue()
{
	if (!message_queue.empty() && canSendCommand()) {
		serial->print(message_queue.front() + EOT_CHAR);
		awaiting_response = true;
	}
}

void WRF::triggerStartup()
{
	is_connected = false;
	if (start_cb != NULL)
		start_cb(config);
	else {
		handleStartup(config);
	}
}

void WRF::handleStartup(WRFConfig & config)
{
	log_message("Handle Startup");
	String command = serializeConfigData(config);
	clearMessageQueue();
	send(command);
	connect();
}

void WRF::handleResultMsg(String result)
{
    if (result == RESULT_SENT)
        triggerSentMessage();
}

void WRF::handleReceivedMessage(JsonObject &message_obj)
{
    if (message_received_cb == NULL)
        return;

    for (JsonObject::iterator it = message_obj.begin(); it != message_obj.end(); ++it)
    {
        String interface_name = it->key;
        JsonObject &data = message_obj[interface_name].asObject();
        String json_string;
        data.printTo(json_string);
        message_received_cb(interface_name, json_string);
    }

}


void WRF::handleUpgradeMsg(JsonArray &pending_upgrades)
{
    if (pending_upgrades_cb == NULL)
        return;
    List list;
    for (JsonArray::iterator it = pending_upgrades.begin(); it != pending_upgrades.end(); ++it)
        addToList(list, *it);
    if (getListSize(list) > 0)
        pending_upgrades_cb(list);
}


void WRF::handleWrfMessage(String &msg)
{
    log_message("Received message: " + msg);
    StaticJsonBuffer<JSON_COMMAND_MAX_SIZE> jsonBuffer;
    JsonObject &message_obj = jsonBuffer.parseObject(msg);

	String msg_request = message_queue.pop_front();
	awaiting_response = false;

	if (!message_obj.success())
        handleErrorMsg("Invalid JSON from WRF");

	else if (message_obj.containsKey(DEVICEDRIVE_LOCAL))
	{	
		JsonObject &dd_message = message_obj[DEVICEDRIVE_LOCAL].asObject();
		if (dd_message.containsKey(DEVICEDRIVE_ERROR)) {
			if (String(dd_message[DEVICEDRIVE_ERROR].asString()) == ERROR_SYSTEM_BUSY) {
				awaiting_response = true;
				message_queue.push_front(msg_request);
			}
			else {
				handleErrorMsg(message_obj[DEVICEDRIVE_LOCAL].asString());
			}
		}
		if (dd_message.containsKey(DEVICEDRIVE_RESULT))
			handleResultMsg(dd_message[DEVICEDRIVE_RESULT].asString());
		if (dd_message.containsKey(DEVICEDRIVE_UPGRADE))
			handleUpgradeMsg(dd_message[DEVICEDRIVE_UPGRADE].asArray());
		if (dd_message.containsKey(DEVICEDRIVE_STATUS)) {
			handleStatusMsg(dd_message[DEVICEDRIVE_STATUS].asObject());
		}
	}
	else if (message_obj.containsKey(DEVICEDRIVE_REMOTE))
	{
		JsonObject &dd_message = message_obj[DEVICEDRIVE_REMOTE].asObject();
		if (dd_message.containsKey(ERROR_CODE)) {
			handleErrorMsg(message_obj[DEVICEDRIVE_REMOTE].asString());
		}
	}
	else if (message_obj.containsKey(CONFIGURATION))
	{
		handleConfiguration(message_obj[CONFIGURATION].asObject());
	}
	else
	{
		handleReceivedMessage(message_obj);
	}

    
}

void WRF::handleSerialInput() {
	char last_char = 0x00;
	while (serial->available() > 0) {
		char data = serial->read();
		
		if (data == ETX_CHAR && received_message.endsWith(String(STX_CHAR))) {
			received_message = "";
			triggerStartup();
			return;
		}
		last_char = data;
		
		if (data != EOT_CHAR) 
			received_message += data;
        else
        {
            handleWrfMessage(received_message);
            received_message = "";
        }
	}
}

unsigned int * WRF::createCrcTable(void)
{
    unsigned int* crc_table;
    unsigned int c;
    unsigned int i, j;
    crc_table = (unsigned int*)malloc(256 * 4);
    for (i = 0; i < 256; i++) {
        c = (unsigned int)i;
        for (j = 0; j < 8; j++) {
            if (c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        }
        crc_table[i] = c;
    }
    return crc_table;
}

unsigned int WRF::crc32(unsigned int crc, unsigned int* crc_table, unsigned char *buffer, unsigned int size)
{
    unsigned int i;
    for (i = 0; i < size; i++) {
        crc = crc_table[(crc ^ buffer[i]) & 0xff] ^ (crc >> 8);
    }
    return crc ;
}

/* Replace this function with your implementation */
unsigned int WRF::calcCrc(unsigned char* buffer, int size)
{
	log_message("Length: ");
	log_message(len);
    unsigned int *crc_table = createCrcTable();
    unsigned int crc = 0xffffffff;
    crc = crc32(crc, crc_table, buffer, size);
    free(crc_table);
    return crc;
}

void WRF::log_message(String msg)
{
	if (log_port != NULL)
	{
		log_port->println("WRF: " + msg);
	}
}

void WRF::log_message(int data)
{
	log_message(String(data));
}

String WRF::serializeConfigData(WRFConfig &config) {
	StaticJsonBuffer<JSON_COMMAND_MAX_SIZE> jsonBuffer;
	JsonObject& root = jsonBuffer.createObject();
	JsonObject& command = root.createNestedObject("devicedrive");
	command["command"] = "setup";
    command["debug_mode"] = (config.debug_mode == DEBUG_ALL ? "all" : "none");
    command["error_mode"] = (config.error_mode == ERROR_ALL ? "all" : "none");
	command["ssid_prefix"] = config.ssid_prefix;
	command["product_key"] = product_key;
	command["version"] = version;
	command["ssl_enabled"] = (config.ssl_enabled ? "1" : "0");
    String result;
	root.printTo(result);
    return result;
}


JsonObject& WRF::deserialize(String msg){
  msg.trim();
  msg.replace("\n", "");
  msg.replace(" ", "");
  msg.replace("\t", "");
  StaticJsonBuffer<JSON_COMMAND_MAX_SIZE> jsonBuffer;
  return jsonBuffer.parseObject(msg.c_str());
}

void WRF::merge(JsonObject& dest, JsonObject& src) {
	for (auto kvp : src) {
		dest[kvp.key] = kvp.value;
	}
}
