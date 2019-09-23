#include "HttpSimcom.h"

void HttpSimcom::HttpDo(HttpRequest req, 
                std::function<void(HttpResponse &)> callbackSuccess,
                std::function<void(HttpResponse &)> callbackFail,
                unsigned int timeout)
{
	//char *p_dataOutput = nullptr;
	std::stringstream httpParaUrl, httpParaTimeout, httpData, httpactionCmd;
	httpParaUrl 	<< "AT+HTTPPARA=\"URL\"," << "\"" << req.url << "\""; 
	httpParaTimeout << "AT+HTTPPARA=\"TIMEOUT\"," << timeout;
	httpData 	  	<< "AT+HTTPDATA=" << req.length << "," << "5000";
	httpactionCmd   << "AT+HTTPACTION=" << int(req.action);

	SerialModem::Command *p = new SerialModem::Command(
		"AT+HTTPINIT", "OK", 
		300, 0, 
		[=](std::smatch  &s, char* p_data)
		{ 
			INFO("HTTP inited.");
			m_counter++;
			HttpQueue q;
			q.id = m_counter;
			q.callFail = callbackFail;
			q.callSuccess = callbackSuccess;
			q.bGetResult = req.bGetResult;
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
					if(m_queue.size() > 0)
					{
						HttpQueue &q = m_queue.front();
						q.callFail(res);
						m_queue.pop();
					}
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
			
			if(m_queue.size() > 0)
			{
				HttpQueue &q = m_queue.front();
				q.callFail(res);
				m_queue.pop();
			}
		}
	)->Chain(
		httpParaTimeout.str().c_str(),"OK",
		1000,0, nullptr,
		[=]()
		{
			INFO("Failed to fill parameter. Check timeout value");
			HttpResponse res;
			res.code = HttpStatusCode::Init_Failed;
			if(m_queue.size() > 0)
			{
				HttpQueue &q = m_queue.front();			
				q.callFail(res);
				m_queue.pop();
			}
			
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
				
				if(m_queue.size() > 0)
				{
					HttpQueue &q = m_queue.front();				
					q.callFail(res);
					m_queue.pop();
				}
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
		);
		httpParaUserHeader.clear();
    }

	if(req.length > 0 && req.data != nullptr)
	{
		p2 = p2->Chain(
			httpData.str().c_str(), "DOWNLOAD",
			3000,0
		)->Chain(
			req.data, "OK", 
			6000, 0,
			[](std::smatch &s, char *p_dat)
			{
				INFO("Data download ok");
			},
			[=]()
			{
				INFO("failed to fill httpData param. Please check data len and actual data length");
				HttpResponse res;
				res.code = HttpStatusCode::Init_Failed;
				if(m_queue.size() > 0)
				{
					HttpQueue &q = m_queue.front();			
					q.callFail(res);
					m_queue.pop();
				}
				m_serialModem->Enqueue(new SerialModem::Command(
					"AT+HTTPTERM", 
					"OK", 
					200, 200, 
					[](std::smatch  &s, char* p_data) 
					{ 
						INFO("HTTP terminated.");
					}
				));
			},
			false, true,
			req.length, 1000
		);
	}

	p2->Chain(
		httpactionCmd.str().c_str(), 
		"OK\r\n",
		1000, 100,
		[=](std::smatch  &s, char* p_data) 
		{
			INFO("Executed"); 
			HttpQueue &q = m_queue.front();
			q.timeStart = millis() - 100;
		}
	)->Chain(
		"", 
		"HTTPACTION: ([0-9]+),([0-9]+),([0-9]+)",
		timeout*1000 + 5, 0,
		[=](std::smatch  &s, char* p_data) 
		{ 
			INFO("Http result: %d len: %d", atoi(s[2].str().c_str()), atoi(s[3].str().c_str()) ); 
			size_t dataLen =  atoi(s[3].str().c_str());
			size_t endByte = 0;
			size_t count = int(ceil(dataLen / MAX_READ));
			HttpQueue &q = m_queue.front();
			q.timeEnd = millis();
			q.status = (HttpStatusCode) atoi(s[2].str().c_str());
			q.length = dataLen;
			if(dataLen == 0 || !q.bGetResult) 
			{
				q.p_dataOutput = new char[2];//std::shared_ptr<char>(new char[2], std::default_delete<char[]>());
				memset(q.p_dataOutput, 0, sizeof(char) *  2);
				return;
			}
			q.p_dataOutput = new char[dataLen + 2 ];//std::shared_ptr<char>(new char[dataLen + 2], std::default_delete<char[]>());
			memset(q.p_dataOutput, 0, sizeof(char) * dataLen + 2);
			INFO("Looping for %d, freemem %d", count, ESP.getFreeHeap());
			for (size_t i = 0; i <= count; i++)
			{
				std::stringstream strcmd;
				size_t startingByte = 0;
				if( i > 0)
				{
					char c[10];
					size_t len = strlen(c);
					startingByte = MAX_READ * i; //- (cutHttpReadPart * i);
				}
				
				endByte = MAX_READ * ( i + 1 );
				if(endByte > dataLen)
				{
					endByte = dataLen ;
					//endByte = (dataLen - MAX_READ);
				}

				strcmd << "AT+HTTPREAD=" << startingByte << "," << endByte;		
				SerialModem::Command *command1 = new SerialModem::Command(
					strcmd.str().c_str(),
					"^\r\n\\+HTTPREAD: ([0-9]*)",
					500, 200,
					[=](std::smatch  &s, char* p_data) 
					{ 
						size_t len = atoi(s[1].str().c_str());	
						size_t cutHttpReadPart = 15 + GetNumberOfDigits(len);
						size_t readLen = MAX_READ;
						if(MAX_READ * ( i + 1 ) > dataLen)
						{
							readLen = dataLen - startingByte ;
						}
						memcpy(q.p_dataOutput + startingByte, p_data + cutHttpReadPart, readLen);
						INFO("[WARN] Queue nr %d", i);						
						//INFO("Msg len %d content  %.*s",  readLen,  readLen, p_data + cutHttpReadPart); 
						//INFO("Msg total %s", q.p_dataOutput);
						//INFO("Msg len %d content  %.*s",  readLen,  readLen, p_data); 

					},
					nullptr, true
				);
				m_serialModem->ForceEnqueue(command1);
			}
			
		},
		[=]() 
		{ 
			INFO("Timed out!"); 
			HttpResponse res;
			res.code = HttpStatusCode::Timeout;
			if(m_queue.size() > 0)
			{
				HttpQueue &q = m_queue.front();
				q.callFail(res);
				m_queue.pop();
			}
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
		[=](std::smatch  &s, char* p_data)
		{ 
			HttpResponse res;
			HttpQueue q = m_queue.front();
			INFO_D("POINTER %p", q.p_dataOutput);
			INFO_D("DATA FIRST CHAR %c", q.p_dataOutput[0]);
			INFO_D("DATA LAST CHAR %c", q.p_dataOutput[q.length - 1]);
			res.bGetResult = q.bGetResult;
			res.code = q.status;
			res.data = static_cast<const char*>(q.p_dataOutput);
			res.timeTaken = q.timeEnd - q.timeStart;	
			res.length = q.length;		
			q.callSuccess(res);
			delete []q.p_dataOutput;
			m_queue.pop();
			INFO("HTTP terminated succesfully."); 
		}
	);
	m_serialModem->Enqueue(p);
}

bool HttpSimcom::InternetTest()
{
    return true;
}
