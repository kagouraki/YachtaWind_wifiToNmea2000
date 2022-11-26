// Demo: NMEA2000 library. Send main wind data to the bus.
#define ESP32_CAN_TX_PIN GPIO_NUM_21
#define ESP32_CAN_RX_PIN GPIO_NUM_22

#include <Arduino.h>
#include <NMEA2000_CAN.h>  // This will automatically choose right CAN library and create suitable NMEA2000 object
#include <N2kMessages.h>

// List here messages your device will transmit.
const unsigned long TransmitMessages[] PROGMEM = { 130306L, 130311L, 0 };

#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <EEPROM.h>

#define TRIGGER_PIN 4
#define Button_pin 13
#define Portal_pin 12
#define Connected_pin 14

// wifimanager can run in a blocking mode or a non blocking mode
// Be sure to know how to process loops with no delay() if using non blocking
bool wm_nonblocking = false; // change to true to use non blocking

WiFiManager wm; // global wm instance
WiFiManagerParameter ip; // global param ( for non blocking w params )
WiFiManagerParameter port;

int customFieldLength = 40;

String wind_ip;
String wind_port;
bool changes_made = false;
bool first_run = true;
IPAddress server;
WiFiClient client;
char charBuf[50];
char eeprom_ip[50];
String eeprom_port;
unsigned long tik = 0;
const unsigned long timeout = 10000;
bool tik_first = true;
double windDirection = 0;
String windSpeedUnits = "N";
double windSpeed = 0;
double AirTemp = 0;
double AirPressure = 0;
double Humidity = 0;
double DewPoint = 0;
bool MWV = false;
bool MDA = false;

void setup() {

	// Set Product information
	NMEA2000.SetProductInformation("00000002", // Manufacturer's Model serial code
			100, // Manufacturer's product code
			"Yachta WindSensor",  // Manufacturer's Model ID
			"1.0 (13-06-2022)",  // Manufacturer's Software version code
			"1.0 (13-06-2022)" // Manufacturer's Model version
			);
	// Set device information
	NMEA2000.SetDeviceInformation(1, // Unique number. Use e.g. Serial number.
			130, // Device function=Atmospheric. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
			85, // Device class=External Environment. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
			2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
			);
	// Uncomment 2 rows below to see, what device will send to bus. Use e.g. OpenSkipper or Actisense NMEA Reader
	//Serial.begin(115200);
	//NMEA2000.SetForwardStream(&Serial);
	// If you want to use simple ascii monitor like Arduino Serial Monitor, uncomment next line
	//NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.

	// If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below
	NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, 23);
	// NMEA2000.SetDebugMode(tNMEA2000::dm_Actisense); // Uncomment this, so you can test code without CAN bus chips on Arduino Mega
	NMEA2000.EnableForward(false);
	NMEA2000.ExtendTransmitMessages(TransmitMessages);
	NMEA2000.Open();

	//WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
	Serial.begin(115200);
	EEPROM.begin(200);
	Serial.setDebugOutput(false);
	delay(3000);
	Serial.println("\n Starting");

	pinMode(TRIGGER_PIN, INPUT_PULLUP);
	pinMode(Button_pin, OUTPUT);
	pinMode(Portal_pin, OUTPUT);
	pinMode(Connected_pin, OUTPUT);
	digitalWrite(Button_pin, LOW);
	digitalWrite(Portal_pin, LOW);
	digitalWrite(Connected_pin, LOW);

	// wm.resetSettings(); // wipe settings

	if (wm_nonblocking)
		wm.setConfigPortalBlocking(false);

	// add a custom input field

	new (&ip) WiFiManagerParameter("ip",
			"Wind instrument ip adress (Default 192.168.10.1 if connected to instrument hotspot)",
			"", customFieldLength, "placeholder=\"\"");
	new (&port) WiFiManagerParameter("port",
			"Wind instrument server port(Default 6666)", "", customFieldLength,
			"placeholder=\"\"");

	// test custom html input type(checkbox)
	//new (&check) WiFiManagerParameter("check", "Custom Field Label", "Custom Field Value", customFieldLength,"placeholder=\"Custom Field Placeholder\" type=\"checkbox\""); // custom html type

	// test custom html(radio)
	//const char* custom_radio_str = "<br/><label for='customfieldid'>Custom Field Label</label><input type='radio' name='customfieldid' value='1' checked> One<br><input type='radio' name='customfieldid' value='2'> Two<br><input type='radio' name='customfieldid' value='3'> Three";
	//new (&custom_field) WiFiManagerParameter(custom_radio_str); // custom html input

	wm.addParameter(&ip);
	wm.addParameter(&port);
	wm.setSaveParamsCallback(saveParamCallback);

	// custom menu via array or vector
	//
	// menu tokens, "wifi","wifinoscan","info","param","close","sep","erase","restart","exit" (sep is seperator) (if param is in menu, params will not show up in wifi page!)
	// const char* menu[] = {"wifi","info","param","sep","restart","exit"};
	// wm.setMenu(menu,6);
	std::vector<const char*> menu = { "wifi", "info", "param", "sep", "restart",
			"exit" };
	wm.setMenu(menu);

	// set dark theme
	wm.setClass("invert");

	//set static ip
	// wm.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0)); // set static ip,gw,sn
	// wm.setShowStaticFields(true); // force show static ip fields
	// wm.setShowDnsFields(true);    // force show dns field always

	wm.setConnectTimeout(20); // how long to try to connect for before continuing
	//wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
	// wm.setCaptivePortalEnable(false); // disable captive portal redirection
	// wm.setAPClientCheck(true); // avoid timeout if client connected to softap

	// wifi scan settings
	// wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
	// wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
	// wm.setShowInfoErase(false);      // do not show erase button on info page
	// wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons

	// wm.setBreakAfterConfig(true);   // always exit configportal even if wifi save fails

	EEPROM.get(0, eeprom_ip);
	EEPROM.get(100, eeprom_port);
	EEPROM.end();
	//Serial.println(eeprom_ip);
	//Serial.println(eeprom_port);

	bool res;
	// res = wm.autoConnect(); // auto generated AP name from chipid
	digitalWrite(Portal_pin, HIGH);
	res = wm.autoConnect("Wifi2Nmea2000"); // anonymous ap
	//res = wm.autoConnect("Wifi2Nmea2000", "12345678"); // password protected ap

	if (!res) {
		Serial.println("Failed to connect or hit timeout");
		digitalWrite(Portal_pin, LOW);
		// ESP.restart();
	} else {
		//if you get here you have connected to the WiFi
		Serial.println("connected...yeey :)");
		digitalWrite(Portal_pin, LOW);
	}
}

double ReadAirPressure() {

	return mBarToPascal(AirPressure * 1000); // Read here the measured wind angle e.g. from analog input
}

double ReadAirTemp() {

	return CToKelvin(AirTemp); // Read here the measured wind angle e.g. from analog input
}

double ReadHumidity() {

	return Humidity; // Read here the measured wind angle e.g. from analog input
}

double ReadDewPoint() {

	return CToKelvin(DewPoint); // Read here the measured wind angle e.g. from analog input
}

double ReadWindAngle() {

	return DegToRad(windDirection); // Read here the measured wind angle e.g. from analog input
}

double ReadWindSpeed() {
	if (windSpeedUnits == "N") {
		//Serial.println("KNOTS");
		return KnotsToms(windSpeed);

	} else if (windSpeedUnits == "M") {
		//Serial.println("meters");
		return windSpeed;

	} else if (windSpeedUnits == "K") {
		//Serial.println("kilometers");
		return windSpeed * 0.2777777;

	} else {
		return -1;
	}

}

//#define WindUpdatePeriod 250

void SendN2kWind() {
	//static unsigned long WindUpdated = millis();
	tN2kMsg N2kMsg;

	//if (WindUpdated + WindUpdatePeriod < millis()) {

	SetN2kWindSpeed(N2kMsg, 1, ReadWindSpeed(), ReadWindAngle(),
			N2kWind_Apparent);
	NMEA2000.SendMsg(N2kMsg);
	SetN2kEnvironmentalParameters(N2kMsg, 2, N2kts_OutsideTemperature,
			ReadAirTemp(), N2khs_OutsideHumidity, ReadHumidity(),
			ReadAirPressure());
	NMEA2000.SendMsg(N2kMsg);
	SetN2kEnvironmentalParameters(N2kMsg, 2, N2kts_DewPointTemperature,
			ReadDewPoint());
	NMEA2000.SendMsg(N2kMsg);
	//WindUpdated = millis();
//}
}

void checkButton() {
// check for button press
	if (digitalRead(TRIGGER_PIN) == LOW) {
		// poor mans debounce/press-hold, code not ideal for production
		delay(50);
		if (digitalRead(TRIGGER_PIN) == LOW) {
			Serial.println("Button Pressed");
			digitalWrite(Button_pin, HIGH);
			digitalWrite(Portal_pin, LOW);
			digitalWrite(Connected_pin, LOW);
			// still holding button for 8000 ms, reset settings, code not idea for production
			delay(8000); // reset delay hold
			if (digitalRead(TRIGGER_PIN) == LOW) {
				Serial.println("Button Held");
				Serial.println("Erasing Config and EEPROM, restarting");
				client.stop();
				first_run = true;
				wm.resetSettings();
				EEPROM.begin(200);
				for (int i = 0; i <= 200; i++) {
					EEPROM.write(i, 0);
				}
				boolean ok1 = EEPROM.commit();
				Serial.println((ok1) ? "commit OK" : "Commit failed");
				EEPROM.end();
				digitalWrite(Button_pin, LOW);
				digitalWrite(Portal_pin, LOW);
				digitalWrite(Connected_pin, LOW);
				for (int i = 0; i < 8; ++i) {
					digitalWrite(Button_pin, !digitalRead(Button_pin));
					digitalWrite(Portal_pin, !digitalRead(Portal_pin));
					digitalWrite(Connected_pin, !digitalRead(Connected_pin));
					delay(500);
				}
				ESP.restart();

			}

			// start portal w delay
			Serial.println("Starting config portal");
			client.stop();
			first_run = true;
			//wm.setConfigPortalTimeout(120);
			digitalWrite(Button_pin, LOW);
			digitalWrite(Portal_pin, HIGH);
			if (!wm.startConfigPortal("Wifi2Nmea2000")) {
				Serial.println("failed to connect or hit timeout");
				digitalWrite(Portal_pin, LOW);
				delay(3000);
				ESP.restart();
			} else {
				//if you get here you have connected to the WiFi
				Serial.println("connected...yeey :)");
				digitalWrite(Portal_pin, LOW);
			}
		}
	}

}

String getParam(String name) {
//read parameter from server, for customhtml input
	String value;
	if (wm.server->hasArg(name)) {
		value = wm.server->arg(name);
	}
	return value;
}

void switch_server() {
	client.stop();
	delay(1000);
	EEPROM.begin(200);
	EEPROM.get(0, eeprom_ip);
	EEPROM.get(100, eeprom_port);
	EEPROM.end();
	Serial.println(eeprom_ip);
	Serial.println(eeprom_port);

	first_run = true;
	changes_made = false;

}

void saveParamCallback() {
	String temp_ip = getParam("ip");
	String temp_port = getParam("port");
	temp_ip.toCharArray(charBuf, temp_ip.length() + 1);
	Serial.println(charBuf);
	Serial.println(temp_port);
	EEPROM.begin(200);
	EEPROM.put(0, charBuf);
	EEPROM.put(100, temp_port);
	boolean ok1 = EEPROM.commit();
	Serial.println((ok1) ? "commit OK" : "Commit failed");
	EEPROM.end();
	changes_made = true;

}

String getValue(String data, char separator, int index) {
	int found = 0;
	int strIndex[] = { 0, -1 };
	int maxIndex = data.length() - 1;

	for (int i = 0; i <= maxIndex && found <= index; i++) {
		if (data.charAt(i) == separator || i == maxIndex) {
			found++;
			strIndex[0] = strIndex[1] + 1;
			strIndex[1] = (i == maxIndex) ? i + 1 : i;
		}
	}
	return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void loop() {
	if (MWV && MDA) {
		SendN2kWind();
		MWV = false;
		MDA = false;
	}
	NMEA2000.ParseMessages();
	if (wm_nonblocking)
		wm.process(); // avoid delays() in loop when non-blocking and other long running code

	checkButton();
// put your main code here, to run repeatedly:
	if (WiFi.status() == WL_CONNECTED) {
		if (first_run) {
			digitalWrite(Portal_pin, LOW);
			Serial.println("Initial Delay");
			delay(2000);
			Serial.print("Connecting to:");

			if (server.fromString(eeprom_ip) && eeprom_port.toInt() > 0
					&& eeprom_port.toInt() <= 65536) {
				Serial.print(server);
				Serial.print(" : ");
				Serial.print(eeprom_port.toInt());
				client.setTimeout(0.5);
				while (!client.connect(server, eeprom_port.toInt(), 500)) {

					Serial.print(".");
					digitalWrite(Connected_pin, !digitalRead(Connected_pin));
					checkButton();

				}
				Serial.println(".");
				Serial.println("CONNECTED");
				digitalWrite(Connected_pin, HIGH);
				first_run = false;

			} else {
				digitalWrite(Connected_pin, LOW);
				Serial.println("Not valid IP or port");
				Serial.println("Starting config portal");
				while (true) {
					digitalWrite(Connected_pin, HIGH);
					delay(200);
					checkButton();
					digitalWrite(Connected_pin, LOW);
					delay(200);
					checkButton();
					digitalWrite(Connected_pin, HIGH);
					delay(200);
					checkButton();
					digitalWrite(Connected_pin, LOW);
					delay(200);
					checkButton();
					digitalWrite(Connected_pin, HIGH);
					delay(200);
					checkButton();
					digitalWrite(Connected_pin, LOW);
					delay(200);
					checkButton();
					delay(1000);
					checkButton();

				}
			}
		}

		if (changes_made) {

			switch_server();

		}

		if (client.connected()) {
			digitalWrite(Connected_pin, HIGH);
			if (client.available()) {
				String line = client.readStringUntil('\r');
//				Serial.print(line);
				if (line.indexOf("MWV") > 0) {
					Serial.println(line);
					windDirection = getValue(line, ',', 1).toDouble();
					windSpeed = getValue(line, ',', 3).toDouble();
					windSpeedUnits = getValue(line, ',', 4);
					MWV = true;
//					Serial.println(windDirection);
//					Serial.println(windSpeed);
//					Serial.println(windSpeedUnits);

				} else if (line.indexOf("MDA") > 0) {
					Serial.println(line);
					AirPressure = getValue(line, ',', 3).toDouble();
					AirTemp = getValue(line, ',', 5).toDouble();
					Humidity = getValue(line, ',', 9).toDouble();
					DewPoint = getValue(line, ',', 11).toDouble();
					MDA = true;
//					Serial.println(AirPressure);
//					Serial.println(AirTemp);
//					Serial.println(Humidity);
//					Serial.println(DewPoint);

				}

				tik_first = true;
			} else {
				if (tik_first) {
					tik = millis();
					tik_first = false;
				}
				if (millis() - tik > timeout) {
					first_run = true;
					digitalWrite(Connected_pin, LOW);
					client.stop();
					tik_first = true;
				}

			}

		} else {
			digitalWrite(Connected_pin, LOW);
			first_run = true;
		}
	} else {
		digitalWrite(Connected_pin, !digitalRead(Connected_pin));
		delay(100);
	}
}

