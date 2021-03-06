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
#include <vector>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <cJSON.h>
#include <time.h>
#include <sys/time.h>
#include <iomanip>

//{"id":"9605aa56-d851-4692-bbb1-0db2c6d481ca","auth_token":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJIZWlucmljaDEyMyIsImp0aSI6IjMxZjQ1NTc0LTZmNzYtNGE2Ny05YTUzLWQ5M2RhZjI0ZDRkZiIsImlhdCI6MTU3MjUyNTIyNCwicm9sIjoiYXBpX2FjY2Vzc19kZXZpY2UiLCJpZCI6Ijk2MDVhYTU2LWQ4NTEtNDY5Mi1iYmIxLTBkYjJjNmQ0ODFjYSIsIm5iZiI6MTU3MjUyNTIyMywiZXhwIjoxNjA0MDYxMjIzLCJpc3MiOiJ3ZWJBcGkiLCJhdWQiOiJodHRwOi8vMzUuMjQwLjIwNy4zNi8ifQ.8AlpXb_qxwQCiU0svYaveAbBd0pBKpbOEyz5yte6-po","expires_in":31536000} 

const char *jwt = "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJIZWlucmljaDEyMyIsImp0aSI6IjMxZjQ1NTc0LTZmNzYtNGE2Ny05YTUzLWQ5M2RhZjI0ZDRkZiIsImlhdCI6MTU3MjUyNTIyNCwicm9sIjoiYXBpX2FjY2Vzc19kZXZpY2UiLCJpZCI6Ijk2MDVhYTU2LWQ4NTEtNDY5Mi1iYmIxLTBkYjJjNmQ0ODFjYSIsIm5iZiI6MTU3MjUyNTIyMywiZXhwIjoxNjA0MDYxMjIzLCJpc3MiOiJ3ZWJBcGkiLCJhdWQiOiJodHRwOi8vMzUuMjQwLjIwNy4zNi8ifQ.8AlpXb_qxwQCiU0svYaveAbBd0pBKpbOEyz5yte6-po";

#define RXD2 16
#define TXD2 17
//AsyncModem::SIM7000 *p_Modem = new AsyncModem::SIM7000();

SerialModem *p_Modem = new SerialModem();
HttpSimcom *p_http;
HardwareSerial hardwareSerial(2);
const char* token = "";
bool isTampered = false;
bool doOnce = false;
bool doOnceGetTime = false;

void PrintCurrentTime()
{
    time_t t = time(0);
    tm lc = *std::localtime(&t);
    std::ostringstream tmStr;
    tmStr << std::put_time(&lc, "%H:%M:%S, %d/%m/%Y");
    INFO("current time %s", tmStr.str().c_str());
    INFO("in unix timestamp %u", (unsigned)time(NULL)); 
}


void SendLocation()
{
    p_Modem->TurnOnGps(
        [](SerialModem::Location loc)
        {
            HttpSimcom::HttpRequest req;
            req.action = HttpSimcom::ActionHttp::Post;
            req.contentType = "application/json";
            req.header = std::map<std::string, std::string> {
		        {"Authorization",jwt}
	        };
            cJSON *root;
            // not accurate!
            // time_t t = mktime(&loc.gpsTime);
            // INFO("time is %d:%d:%d, %d/%d/%d", loc.gpsTime.tm_hour,loc.gpsTime.tm_min,loc.gpsTime.tm_sec, loc.gpsTime.tm_mday, loc.gpsTime.tm_mon, loc.gpsTime.tm_year);
            time_t now = time(0);
            root = cJSON_CreateObject();
            cJSON_AddItemToObject(root, "CurrentModem", cJSON_CreateNumber(0));
            cJSON_AddItemToObject(root, "CurrentEsp", cJSON_CreateNumber(0));
            cJSON_AddItemToObject(root, "Elevation", cJSON_CreateNumber(loc.altitude));
            cJSON_AddItemToObject(root, "Longitude", cJSON_CreateNumber(loc.longitude));
            cJSON_AddItemToObject(root, "Latitude", cJSON_CreateNumber(loc.latitude));
            cJSON_AddItemToObject(root, "Timestamp", cJSON_CreateNumber(now));
            cJSON_AddItemToObject(root, "Velocity", cJSON_CreateNumber(loc.speed));
            cJSON_AddItemToObject(root, "IsTampered", isTampered ? cJSON_CreateFalse() :  cJSON_CreateTrue() );
            std::stringstream desc;
            desc << "periodic update" << ", provider " << p_Modem->GetProviderName();
            cJSON_AddItemToObject(root, "Descriptions", cJSON_CreateString(desc.str().c_str()));
            cJSON_AddItemToObject(root, "SignalStrength", cJSON_CreateNumber(p_Modem->GetSignal()));
            cJSON_AddItemToObject(root, "Direction", cJSON_CreateNumber(loc.course));
            cJSON_AddItemToObject(root, "Accuracy", cJSON_CreateNumber(loc.hdop));
            cJSON_AddItemToObject(root, "Battery", cJSON_CreateNumber(0));
            cJSON_AddItemToObject(root, "ConnectionType", cJSON_CreateString( p_Modem->GetNetworkStatus() == SerialModem::EGSM ? "GSM" : "NB-IoT" ));
            req.data = cJSON_Print(root);
            req.bGetResult = true;
            req.length = strlen(cJSON_Print(root));
            req.url = "http://35.240.207.36/api/device/update/" + std::string(p_Modem->GetIMEI());
            cJSON_Delete(root);
            p_http->HttpDo(req, [](HttpSimcom::HttpResponse &r)
            {
                INFO("connect success \nresult %d, data = %s", r.code ,r.data);
            },
            [](HttpSimcom::HttpResponse &r)
            {
                INFO("failed \nresult %d, data = %s", r.code ,r.data);
            });
        },
        1000*60*10
    );
}


void ConnectToInternet()
{
	if ( p_Modem->GetNetworkStatus() == SerialModem::EREGISTERED_HOME && 
		!p_Modem->GetIsGPRSConnected() && !p_Modem->isBusy())
	{
		INFO("Registering GPRS now");
		p_Modem->ConnectGPRS("m2mdev", "", "");
	}
}

unsigned long lasttime = 0;

void UpdateDateAndTIme()
{
    if(millis() - lasttime < 1000 * 60 * 2) return;
    INFO("Getting time and date...");
    PrintCurrentTime();
    HttpSimcom::HttpRequest req;
    req.action = HttpSimcom::Get;
    req.contentType = "application/json";
    
    req.url = "http://35.240.207.36/api/device/time";

    p_http->HttpDo(req, [](HttpSimcom::HttpResponse &r)
    {
        INFO("data returned from server (%d) : %s", r.code ,r.data);
        if(r.code == HttpSimcom::OK)
        {
           
            cJSON *json = cJSON_Parse(r.data);
            cJSON *jsonSecond = cJSON_GetObjectItem(json, "Seconds");
            cJSON *jsonMicroSecond = cJSON_GetObjectItem(json, "MicroSeconds");      
            
          
            timeval t = { .tv_sec =  static_cast<long>(jsonSecond->valuedouble) };
            INFO("value of jsonSecond is %ld", t.tv_sec);

            settimeofday(&t, nullptr);     
            INFO("TIME is set!");  
            PrintCurrentTime(); 
            
            cJSON_Delete(json);    
            SendLocation();
        }
    },
    [](HttpSimcom::HttpResponse &r)
    {
        INFO("Failed to get time data code: %d", r.code);
    } );
    lasttime = millis();

}

void setup() 
{
    Serial.begin(115200);
    INFO("Hallo world, delaying for 5 seconds.");
    unsigned long now = millis();
    while(millis() - now < 1000 * 5)
    {
        Serial.print(".");
        delay(100);
    }
    INFO("OK");
    hardwareSerial.begin(115200, SERIAL_8N1, RXD2, TXD2);
    
	p_Modem->Begin(&hardwareSerial);
	//p_Modem->SetPrefferedNetwork(SerialModem::ENBIOT); //commented, NB is OK, i just don't want it to finding network again
	//p_Modem->SetPrefferedNetwork(SerialModem::ENBIOT);
    p_http = new HttpSimcom(*p_Modem); 
    INFO("delay for 60 sec");
    while(millis() - now < 1000 * 60)
    {
        Serial.print(".");
        delay(100);
    }
    INFO("\n ok");
    timeval t = { .tv_sec = 1234567890 };
    settimeofday(&t, nullptr);
    PrintCurrentTime();

}

void loop() 
{
    if(!p_Modem->isBusy())
    {
        ConnectToInternet();
    }
    if(p_Modem->GetIsGPRSConnected() && !p_Modem->isBusy())
    {
        UpdateDateAndTIme();
    }
    if(!p_Modem->isBusy())
    {
        
        // INFO("IP address %s", p_Modem->GetIPAddress().toString().c_str());
        // INFO("Operator is %s, signal strength is %d", p_Modem->GetProviderName(), p_Modem->GetSignal());    
        // INFO("device IMEI is %s", p_Modem->GetIMEI());
        // PrintCurrentTime();
    }
    
}
