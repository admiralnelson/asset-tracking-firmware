#include "Arduino.h"
#include <vector>
#include <../DebugUtils/DebugUtils.h>
#include <../SerialModem/SerialModem.h>
#include <../HttpSimcom/HttpSimcom.h>
#include <functional>
#include <regex>
#include <WiFi.h>
#include <string>
#include <HardwareSerial.h>
#include <functional>
#include <map>
#define RXD2 16
#define TXD2 17
#define MAX_CHAR 4000
//AsyncModem::SIM7000 *p_Modem = new AsyncModem::SIM7000();

SerialModem *p_Modem = new SerialModem();
HttpSimcom *p_http;
const char *p_rawData = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

HardwareSerial hardwareSerial(2);

void InitHTTP();
void PostDataTest();

void setup() 
{
	Serial.begin(115200);
	Serial.print("test 123\n");
	int wifi = WiFi.scanNetworks();
	for (int i = 0; i < wifi; i++)
	{
		Serial.println(WiFi.SSID(i));
	}
	WiFi.begin("TelkomUniversity");
	hardwareSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
	// p_rawData = new char[MAX_CHAR+1];
	// p_rawData[MAX_CHAR] = 0;
	// memset(p_rawData, 'A', sizeof(char)*MAX_CHAR);
	// HEAP_CHECK();
	// for (size_t i = 0; i < 4000; i++)
	// {
	// 	Serial.print(p_rawData[i]);
	// }
	p_Modem->Begin(&hardwareSerial);
	p_Modem->SetPrefferedNetwork(SerialModem::ENBIOT);
	p_http = new HttpSimcom(*p_Modem); 

	

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
	if(true )
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
			lastDownload = millis();
	 		PostDataTest();
			 //InitHTTP();
	 	}
	}
}

void PostDataTest()
{
	INFO("UPLOAD Data");
	HttpSimcom::HttpRequest req;
	req.url = "http://pastebin.com/raw/WBn3FmgE";
	std::map<std::string, std::string> header = {
		{"Authorization","test"}
	};
	req.header = header;
	req.action = HttpSimcom::ActionHttp::Get;
	req.data = (const char*)p_rawData;
	req.length = strlen(p_rawData);
	p_http->HttpDo(req, 
		[](HttpSimcom::HttpResponse &r)
		{
			INFO("SUCCESS!");
			INFO("CODE %d, TIME %lu, Freemem %d B ", r.code, r.timeTaken, ESP.getFreeHeap());
			INFO("DATA \n%s", r.data);
		},
		[](HttpSimcom::HttpResponse &r)
		{
			INFO("FAILED, due to network or param error");
			INFO("code %d", r.code);
		},
		120
	);
}

void DownloadNovelProcedure()
{
	INFO("DOWNLOAD PAGE");
	HttpSimcom::HttpRequest req;
	req.url = "http://pastebin.com/raw/TUtLdNHZ";
	std::map<std::string, std::string> header = {
		{"Authorization","test"}
	};
	req.header = header;
	req.action = HttpSimcom::ActionHttp::Get;
	p_http->HttpDo(req, 
		[](HttpSimcom::HttpResponse &r)
		{
			INFO("SUCCESS!");
			INFO("CODE %d, TIME %lu, Freemem %d B ", r.code, r.timeTaken, ESP.getFreeHeap());
			int showInHalf = r.length / 2;
			int otherHalf = r.length - showInHalf;
			INFO("DATA \n%.*s", showInHalf , r.data);
			INFO("DATA \n%.*s", otherHalf , r.data + showInHalf);

		},
		[](HttpSimcom::HttpResponse &r)
		{
			INFO("FAILED, due to network or param error");
			INFO("code %d", r.code);
		}
	);
}
void InitHTTP()
{
	 SerialModem::Command *p = new SerialModem::Command(
		"AT+HTTPINIT", "OK", 
		100, 100, 
		[](std::smatch  &s, char* p_data) { INFO("HTTP inited.")},
		[=]() 
		{
			 p_Modem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200, 
				[=](std::smatch  &s, char* p_data) 
				{ 
					INFO("HTTP failed. Terminating now"); 
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
		[](std::smatch  &s, char* p_data) { INFO("Executed"); }
	)->Chain(
		"", 
		"HTTPACTION: ([0-9]+),([0-9]+),([0-9]+)",
		1000 * 120, 1000,
		[=](std::smatch  &s, char* p_data) 
		{ 
			INFO("Http result: %d len: %d", atoi(s[2].str().c_str()), atoi(s[3].str().c_str()) ); 
			size_t dataLen =  atoi(s[3].str().c_str());
			size_t endByte = 0;
			size_t count = ceil(dataLen / 256);
			for (size_t i = 0; i < count; i++)
			{
				std::stringstream strcmd;
				size_t startingByte = 0;
				if( i > 0)
				{
					char c[10];
					itoa( i * 256, c , 10);
					size_t len = strlen(c);
					startingByte = 256 * i - (18 * i) ;
				}
				
				endByte = 256 * ( i + 1 );
				if(endByte > dataLen)
				{
					endByte = dataLen ;
				}
				
				strcmd << "AT+HTTPREAD=" << startingByte << "," << endByte;		
				SerialModem::Command *command1 = new SerialModem::Command(
					strcmd.str().c_str(),
					"HTTPREAD: ([0-9]*)\r\n",
					1000, 1000,
					[=](std::smatch  &s, char* p_data) 
					{ 
						unsigned int len = s[1].str().size();	
						INFO("Msg content %s len %d", p_data, strlen(p_data)); 
					}
				);
				p_Modem->Enqueue(command1);
			}
			
		},
		[=]() 
		{ 
			INFO("Timed out!"); 
			p_Modem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200, 
				[](std::smatch  &s, char* p_data) { INFO("HTTP terminated."); }
			));
		}
	)->Chain(
		"AT+HTTPTERM", 
		"OK", 
		200, 200,
		[](std::smatch  &s, char* p_data) { INFO("HTTP terminated."); }
	);
	p_Modem->Enqueue(p);
}
