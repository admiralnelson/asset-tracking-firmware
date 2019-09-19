#include "HttpSimcom.h"

void HttpSimcom::HttpDo(HttpRequest req, 
                std::function<void(HttpResponse &)> callbackSuccess,
                std::function<void(HttpResponse &)> callbackFail,
                unsigned int timeout)
{
	//char *p_dataOutput = nullptr;
	std::shared_ptr<char> p_dataOutput;
	std::stringstream httpParaUrl, httpParaTimeout;
	httpParaUrl 	<<	  "AT+HTTPPARA=\"URL\"," 	 << "\"" << req.url << "\""; 
	httpParaTimeout <<    "AT+HTTPPARA=\"TIMEOUT\"," << timeout;
	SerialModem::Command *p = new SerialModem::Command(
		"AT+HTTPINIT", "OK", 
		300, 0, 
		[=](std::smatch  &s, char* p_data)
		{ 
			INFO("HTTP inited.");
			m_counter++;
			HttpQueue q;
			q.id = m_counter;
			q.timeStart = millis();
			m_queue.push(q);
		},
		[=]() 
		{
			 m_serialModem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				300, 0, 
				[=](std::smatch  &s, char* p_data) 
				{ 
					INFO("HTTP failed. Terminating now"); 
                    HttpResponse res;
					res.code = HttpStatusCode::Init_Failed;
					callbackFail(res);
					m_queue.pop();
				}
			));
		}	
	);
	SerialModem::Command *p2 = p->Chain(
		httpParaUrl.str().c_str(), "OK",
		1000, 0, nullptr,
		[=]()
		{
			INFO("Failed to fill parameter. Check URL");
			HttpResponse res;
			res.code = HttpStatusCode::Init_Failed;
			callbackFail(res);
			m_queue.pop();
		}
	)->Chain(
		httpParaTimeout.str().c_str(),"OK",
		1000,0, nullptr,
		[=]()
		{
			INFO("Failed to fill parameter. Check timeout value");
			HttpResponse res;
			res.code = HttpStatusCode::Init_Failed;
			callbackFail(res);
			m_queue.pop();
		}
	)->Chain(
		"AT+HTTPPARA=\"CID\",1", "OK",
		1000, 0
	);
	for (const auto& kv : req.header) 
	{
		std::stringstream httpParaUserHeader;
		httpParaUserHeader << "AT+HTTPPARA=\"USERDATA\","<< "\"" << kv.first << ":" << kv.second << "\"";
		p2 = p2->Chain(
			httpParaUserHeader.str().c_str(),"OK",
			1000,0, nullptr,
			[=]()
			{
				INFO("Failed to fill parameter. Check timeout value");
				HttpResponse res;
				res.code = HttpStatusCode::Init_Failed;
				callbackFail(res);
				m_queue.pop();
			}
		);
		httpParaUserHeader.clear();
    }

	p2->Chain(
		"AT+HTTPACTION=0", 
		"OK\r\n",
		1000, 0,
		[](std::smatch  &s, char* p_data) { INFO("Executed"); }
	)->Chain(
		"", 
		"HTTPACTION: ([0-9]+),([0-9]+),([0-9]+)",
		timeout*1000 + 5, 0,
		[=, &p_dataOutput](std::smatch  &s, char* p_data) 
		{ 
			INFO("Http result: %d len: %d", atoi(s[2].str().c_str()), atoi(s[3].str().c_str()) ); 
			size_t dataLen =  atoi(s[3].str().c_str());
			size_t endByte = 0;
			size_t count = int(ceil(dataLen / 256));
			HttpQueue &q = m_queue.front();
			p_dataOutput = std::shared_ptr<char>(new char[dataLen], std::default_delete<char[]>());
			q.timeEnd = millis();
			q.status = (HttpStatusCode) atoi(s[2].str().c_str());
			memset(p_dataOutput.get(), 0, sizeof(char) * dataLen);
			for (size_t i = 0; i <= count; i++)
			{
				std::stringstream strcmd;
				size_t startingByte = 0;
				size_t cutHttpReadPart = 15 + GetNumberOfDigits(dataLen);
				if( i > 0)
				{
					char c[10];
					itoa( i * 256, c , 10);
					size_t len = strlen(c);
					startingByte = 256 * i - (cutHttpReadPart * i) ;
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
						size_t len = s[1].str().size();	
						//ShiftStringLeft(p_data, 18);
						if(strlen(p_data) < 256)
						{
							memset(p_data+strlen(p_data)-4,0,sizeof(char)*4);
						}
						strcpy(p_dataOutput.get() + startingByte, p_data + cutHttpReadPart);
						INFO("Msg content %s len %d", p_data, strlen(p_data)); 
					}
				);
				m_serialModem->Enqueue(command1);
			}
			
		},
		[=]() 
		{ 
			INFO("Timed out!"); 
			HttpResponse res;
			res.code = HttpStatusCode::Timeout;
			callbackFail(res);
			m_queue.pop();
			m_serialModem->Enqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200, 
				[](std::smatch  &s, char* p_data) 
				{ 
					INFO("HTTP terminated.");
				}
			));
		}
	)->Chain(
		"AT+HTTPTERM", 
		"OK", 
		200, 200,
		[=, &p_dataOutput](std::smatch  &s, char* p_data)
		{ 
			HttpResponse res;
			HttpQueue q = m_queue.front();
			res.code = q.status;
			res.data = p_dataOutput;
			m_queue.pop();
			res.timeTaken = q.timeEnd - q.timeStart;
			callbackSuccess(res);
			INFO("HTTP terminated succesfully."); 
		}
	);
	m_serialModem->Enqueue(p);
}

bool HttpSimcom::InternetTest()
{
    return true;
}
