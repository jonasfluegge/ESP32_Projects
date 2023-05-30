// Include libraries
#include "MQTT_DHT22_Temperature_Humidity.h"
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <Preferences.h>

// Define the DHT sensor type and pin
#define DHTTYPE DHT22
#define DHTPIN 18 // GPIO-Pin of the ESP32
DHT dht(DHTPIN, DHTTYPE);

// WiFi configuration
const char* ssid = "YourSSID";
const char* password = "YourWifiPassword";

// MQTT broker configuration
const char* mqttServer = "XXX.XXX.XXX.XXX"; // IP address of your MQTT broker
const int mqttPort = 1883; // Port of your MQTT broker

//Define deep sleep
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  60        // Time ESP32 will go to sleep (in seconds)

int wifiReconnects = 0;
int mqttReconnects = 0;

// Initialize variables for sending WhatsApp messages
String AlertHumidity;
String phoneNumber = "+1234567898765"; // Your phone number
String apiKey = "XXXXXXX"; // Get it from https://textmebot.com/

// Initialize WiFi and MQTT clients
WiFiClient wifi;
PubSubClient client(wifi);
Preferences preferences;

// Function to send a message via WhatsApp using HTTP POST
void sendMessage(String WhatsappMessage) {
	// Construct URL for the CallMeBot API
	String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(WhatsappMessage);

	// Create an HTTP client and connect to the URL
	HTTPClient http;
	http.begin(url);

	// Specify content-type header
	http.addHeader("Content-Type", "application/x-www-form-urlencoded");

	// Send HTTP POST request
	int httpResponseCode = http.POST(url);

	// Check if the message was sent successfully and print appropriate message
	if (httpResponseCode == 200){
		Serial.println("Message sent successfully");
	} else {
		Serial.println("Error sending the message");
		Serial.print("HTTP response code: ");
		Serial.println(httpResponseCode);
	}

	// Free resources
	http.end();
}


void setup()
{
	Serial.begin(115200);
	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	preferences.begin("jf", false);
	bool sentWarning = preferences.getBool("sentWarning", 0);
	dht.begin();
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	// Connect to WiFi network
	while (WiFi.status() != WL_CONNECTED) {
		if (wifiReconnects <= 5) {
			delay(1000);
			++wifiReconnects;
			Serial.println("Connecting to WiFi...");
		} else {
			ESP.restart();
		}

	}

	Serial.println("Connected to WiFi");

	// Connect to MQTT broker on defined port
	client.setServer(mqttServer, mqttPort);
	while (!client.connected()) {
		if (mqttReconnects <= 5) {
			++mqttReconnects;
			if (client.connect("ArduinoClient")) {
				Serial.println("Connected to MQTT broker");
			} else {
				Serial.print("Failed to connect to MQTT broker, rc=");
				Serial.print(client.state());
				delay(1000);
			}
		} else {
			ESP.restart();
		}
	}

	// Read sensor data and check for errors
	float h = dht.readHumidity();
	float t = dht.readTemperature();
	if (isnan(h) || isnan(t)) {
	Serial.println("Failed to read from DHT sensor");
	return;
	}

	// Convert float values to string format
	String tempString = String(t);
	String humString = String(h);

	char tempTopic[50];
	char humTopic[50];
	snprintf (tempTopic, 50, "your/mqtt/topic/temperature");
	snprintf (humTopic, 50, "your/mqtt/topic/humidity");
	client.publish(humTopic, humString.c_str());
	client.publish(tempTopic, tempString.c_str());
	delay(1000); // Wait a second to ensure the MQTT message has been published before putting the ESP into deep sleep mode.

	// Print sensor data to console
	Serial.print("Humidity: ");
	Serial.print(h);
	Serial.print(" %\tTemperature: ");
	Serial.print(t);
	Serial.println(" *C ");

	// To prevent a WhatsApp message from being sent on each run
	if (sentWarning && h <= 49.0) {
	  sentWarning = false;
	  preferences.putBool("sentWarning", sentWarning);
	  Serial.println("sentWarning variable was set to false.");
	}

	// If humidity > 50%, send a warning message
	if (!sentWarning && h > 50.0) {
		String alertMessage = "*Humidity too high!*\n";
		alertMessage += " The humidity in XXXXX room is > 50%.\n\n";
		alertMessage += "Humidity: *" + humString + "* %\n";
		alertMessage += "Temperature: *" + tempString + "* *C";
		Serial.println(alertMessage);

		// Send warning message via WhatsApp
		sendMessage(alertMessage);
		sentWarning = true;
		preferences.putBool("sentWarning", sentWarning);
	}

	preferences.end();
	Serial.flush();
	esp_deep_sleep_start();
}


void loop()
{
// Is not used because the deep sleep mode is used and the setup function is run through after each wake-up.
}
