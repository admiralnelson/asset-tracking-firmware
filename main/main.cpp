#include "Arduino.h"
#include <vector>
#include <DebugUtils.h>
#include <SerialModem.h>
#include <functional>
#include <regex>
#include <WiFi.h>
#include <string>
#include <HardwareSerial.h>
#include <functional>

#define RXD2 16
#define TXD2 17

//AsyncModem::SIM7000 *p_Modem = new AsyncModem::SIM7000();

SerialModem *p_Modem = new SerialModem();

HardwareSerial hardwareSerial(2);

void InitHTTP();

void setup() 
{
	Serial.begin(9600);
	Serial.print("test 123\n");
	int wifi = WiFi.scanNetworks();
	for (int i = 0; i < wifi; i++)
	{
		Serial.println(WiFi.SSID(i));
	}
	WiFi.begin("TelkomUniversity");
	hardwareSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
	p_Modem->Begin(&hardwareSerial);

}

bool ready = false;
bool readyToDownload = false;
bool callOnce = true;
bool successDownload = false;
int waitTimeToDownloadAgain = 60*1000;
int lastDownload = 0;
int lastConnect = 0;
// the loop function runs over and over again until power down or reset
int prevSignal = 0;
bool gprsIsConnected = false;
void loop() 
{
	//p_Modem->Loop();
	delay(1000);
	if(prevSignal != p_Modem->GetSignal() || 
	   gprsIsConnected != p_Modem->GetIsGPRSConnected() )
	{
		int freeRAM = ESP.getFreeHeap();

		prevSignal = p_Modem->GetSignal();
		INFO("Provider %s, signal %d dbm, gprs ready? %d, free RAM %d", 
		p_Modem->GetProviderName(), 
		p_Modem->GetSignal(), 
		p_Modem->GetIsGPRSConnected(), 
		freeRAM);

		prevSignal = p_Modem->GetSignal();
		gprsIsConnected = p_Modem->GetIsGPRSConnected();
	}
	
	if ( p_Modem->GetNetworkStatus() == SerialModem::EREGISTERED_HOME && 
		!p_Modem->GetIsGPRSConnected() && !p_Modem->isBusy())
	{
		INFO("Registering GPRS now");
		p_Modem->ConnectGPRS("m2mdev", "", "", 3);
		callOnce = false;
	}

	// if (p_Modem->GetIsGPRSConnected())
	// {
	// 	if (millis() - lastDownload > waitTimeToDownloadAgain)
	// 	{
	// 		INFO("DOWNLOAD PAGE");
	// 		lastDownload = millis();
	// 		InitHTTP();
	// 	}
	// }
}


void InitHTTP()
{
	p_Modem->Enqueue(new SerialModem::Command("AT+SAPBR=1,1", "OK", 3000, 1000, [](std::smatch &s) { INFO("HTTP bearer open.")}));
	p_Modem->Enqueue(new SerialModem::Command("AT+HTTPINIT", "OK", 100, 100, [](std::smatch &s) { INFO("HTTP inited.")}));
	p_Modem->Enqueue(new SerialModem::Command(
		"AT+HTTPPARA=\"URL\",\"http://www.google.com\"", "OK",
		0, 100
	));
	p_Modem->Enqueue(new SerialModem::Command(
		"AT+HTTPPARA=\"CID\",1", "OK",
		0, 100
	));
	
	p_Modem->Enqueue(new 
		SerialModem::Command(
			"AT+HTTPACTION=0", 
			"OK\r\n",
			20000, 1000,
			[](std::smatch &s) { INFO("Executed"); }
	));

	p_Modem->Enqueue(new
		SerialModem::Command(
			"AT+HTTPREAD",
			"HTTPREAD: ([0-9]*)\r\n((.|\r\n)*)",
			1000, 1000,
			[](std::smatch &s) 
			{ 
				INFO("Msg content %s", s[0].str().c_str()); 
			},
			[]()
			{
				ERROR("Timed out!");
			}
	));
	p_Modem->Enqueue(new SerialModem::Command("AT+HTTPTERM", "OK", 0, 100, [](std::smatch &s) { INFO("HTTP inited.")}));

}
