#include "Arduino.h"
#include <vector>
#include <../DebugUtils/DebugUtils.h>
#include <../SerialModem/SerialModem.h>
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

	if (p_Modem->GetIsGPRSConnected())
	{
	 	if (millis() - lastDownload > waitTimeToDownloadAgain)
	 	{
	 		INFO("DOWNLOAD PAGE");
	 		lastDownload = millis();
	 		InitHTTP();
	 	}
	}
}


void InitHTTP()
{
	 SerialModem::Command *p = new SerialModem::Command(
		"AT+HTTPINIT", "OK", 
		100, 100, 
		[](std::smatch  *s, char* p_data) { INFO("HTTP inited.")},
		[=]() 
		{
			 p_Modem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200, 
				[=](std::smatch  *s, char* p_data) 
				{ 
					INFO("HTTP failed."); 
				}
			));
		}	
	);
	p->Chain(
		"AT+HTTPPARA=\"URL\",\"http://example.com/\"", "OK",
		0, 100
	)->Chain(
		"AT+HTTPPARA=\"CID\",1", "OK",
		0, 100
	)->Chain(
		"AT+HTTPACTION=0", 
		"OK\r\n",
		20000, 1000,
		[](std::smatch  *s, char* p_data) { INFO("Executed"); }
	)->Chain(
		"", 
		"HTTPACTION: ([0-9]+),([0-9]+),([0-9]+)",
		60000, 1000,
		[](std::smatch  *s, char* p_data) 
		{ 
			INFO("Http result: %d len: %d", atoi((*s)[2].str().c_str()), atoi((*s)[3].str().c_str()) ); 
		},
		[=]() 
		{ 
			INFO("Timed out!"); 
			p_Modem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200, 
				[](std::smatch  *s, char* p_data) { INFO("HTTP terminated."); }
			));
		}
	)->Chain(
		"AT+HTTPREAD=0,256",
		"HTTPREAD: ([0-9]*)\r\n",
		1000, 1000,
		[=](std::smatch  *s, char* p_data) 
		{ 
			INFO("Msg content %s len %d", p_data, strlen(p_data)); 
			p_Modem->Enqueue(new SerialModem::Command(
				"AT+HTTPREAD=257,512",
				"HTTPREAD: ([0-9]*)\r\n",
				1000, 1000,
				[](std::smatch  *s, char* p_data) 
				{ 
					INFO("Msg content part 2 %s len %d", p_data, strlen(p_data)); 
				}
			));
		},
		[=]()
		{
			ERROR("Timed out!");
			p_Modem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200,
				[](std::smatch  *s, char* p_data) { INFO("HTTP terminated."); }
			));
		}
	)->Chain(
		"AT+HTTPTERM", 
		"OK", 
		200, 200,
		[](std::smatch  *s, char* p_data) { INFO("HTTP terminated."); }
	);
	p_Modem->Enqueue(p);
}
