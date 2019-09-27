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
			INFO_D("HTTP inited.");
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
			 m_serialModem->ForceEnqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				300, 0, 
				[=](std::smatch  &s, char* p_data) 
				{ 
					INFO_D("HTTP failed. Terminating now"); 
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
			INFO_D("Failed to fill parameter. Check URL");
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
			INFO_D("Failed to fill parameter. Check timeout value");
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
				INFO_D("Failed to fill parameter. Check timeout value");
				HttpResponse res;
				res.code = HttpStatusCode::Init_Failed;
				
				if(m_queue.size() > 0)
				{
					HttpQueue &q = m_queue.front();				
					q.callFail(res);
					m_queue.pop();
				}
				m_serialModem->ForceEnqueue(new SerialModem::Command(
					"AT+HTTPTERM", 
					"OK", 
					200, 200, 
					[](std::smatch  &s, char* p_data) 
					{ 
						INFO_D("HTTP terminated.");
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
				INFO_D("Data download ok");
			},
			[=]()
			{
				INFO_D("failed to fill httpData param. Please check data len and actual data length");
				HttpResponse res;
				res.code = HttpStatusCode::Init_Failed;
				if(m_queue.size() > 0)
				{
					HttpQueue &q = m_queue.front();			
					q.callFail(res);
					m_queue.pop();
				}
				m_serialModem->ForceEnqueue(new SerialModem::Command(
					"AT+HTTPTERM", 
					"OK", 
					200, 200, 
					[](std::smatch  &s, char* p_data) 
					{ 
						INFO_D("HTTP terminated.");
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
			INFO_D("Executed"); 
			HttpQueue &q = m_queue.front();
			q.timeStart = millis() - 100;
		}
	)->Chain(
		"", 
		"HTTPACTION: ([0-9]+),([0-9]+),([0-9]+)",
		timeout*1000 + 5, 0,
		[=](std::smatch  &s, char* p_data) 
		{ 
			INFO_D("Http result: %d len: %d", atoi(s[2].str().c_str()), atoi(s[3].str().c_str()) ); 
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
			INFO_D("Looping for %d, freemem %d", count, ESP.getFreeHeap());
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
						INFO_D("[WARN] Queue nr %d", i);						
						//INFO_D("Msg len %d content  %.*s",  readLen,  readLen, p_data + cutHttpReadPart); 
						//INFO_D("Msg total %s", q.p_dataOutput);
						//INFO_D("Msg len %d content  %.*s",  readLen,  readLen, p_data); 

					},
					nullptr, true
				);
				m_serialModem->ForceEnqueue(command1);
			}
			
		},
		[=]() 
		{ 
			INFO_D("Timed out!"); 
			HttpResponse res;
			res.code = HttpStatusCode::Timeout;
			if(m_queue.size() > 0)
			{
				HttpQueue &q = m_queue.front();
				q.callFail(res);
				m_queue.pop();
			}
			m_serialModem->ForceEnqueue(new SerialModem::Command(
				"AT+HTTPTERM", 
				"OK", 
				200, 200, 
				[](std::smatch  &s, char* p_data) 
				{ 
					INFO_D("HTTP terminated.");
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
			INFO_D("HTTP terminated succesfully."); 
		}
	);
	m_serialModem->Enqueue(p);
}

float HttpSimcom::InternetTest(size_t len, unsigned int interation)
{
	const int numOfTrial = interation;
	const int httpTimeout = 60;
	m_speedCount = 0;
	m_speedCountLoss = 0;
	m_avgSpeed.clear();
    unsigned long timeout = millis();
	bool bIscompleted = false;
	std::stringstream httpReq ;

	httpReq << "http://35.240.207.36/api/values/" << len;

	auto successCallback = [this](HttpResponse &res)
	{
		if(res.code == HttpStatusCode::OK)
		{
			INFO("CODE %d, TIME %lu, download %d B ", res.code, res.timeTaken, res.length);
			float speed = res.length / float(float(res.timeTaken) / 1000);
			m_avgSpeed.push_back(speed);
			INFO("Downlink throughput = %f B/s", speed);
			m_speedCount++;
			INFO("Count %d", m_speedCount);

		}
		else
		{
			INFO("Timeout, counted as loss. CODE %d,", res.code);
			m_avgSpeed.push_back(0);
			m_speedCount++;
			m_speedCountLoss++;
			INFO("Count %d", m_speedCount);

		}
	};

	auto failcallback = [this](HttpResponse &res)
	{
		INFO("fail to download");
		m_avgSpeed.push_back(0);
		m_speedCount++;
		m_speedCountLoss++;
		INFO("Count %d", m_speedCount);
	};

	for (size_t i = 0; i < numOfTrial; i++)
	{
		HttpRequest req;
		req.bGetResult = false;
		req.url = httpReq.str().c_str();
		req.action = ActionHttp::Get;
		HttpDo(req, successCallback, failcallback, httpTimeout);
	}
	
	while(m_speedCount < numOfTrial )
	{
		
	}


	float result = std::accumulate(m_avgSpeed.begin(), m_avgSpeed.end(), 0);
	if(result > 0)
	{
		result = result / m_avgSpeed.size();
	}

	INFO("average downlink speed is %f B/s, after many %d trial, packet loss %d", result, m_avgSpeed.size(), m_speedCountLoss);
	return result;
}


float HttpSimcom::InternetUploadTest(size_t len, unsigned int interation)
{
	if(len * interation > ESP.getFreeHeap() - 10000)
	{
		ERROR("This operation might cause heap to be full. Aborting..., free heap is %d B", ESP.getFreeHeap());
		return 0;
	}
	const int numOfTrial = interation;
	const int httpTimeout = 60;
	m_speedCount = 0;
	m_speedCountLoss = 0;
	m_avgSpeed.clear();
    unsigned long timeout = millis();
	bool bIscompleted = false;
	std::stringstream httpReq ;
	char *data = new char[len+3];
	memset(data, 0, sizeof(char)*len+2);
	memset(data+1, 'A', sizeof(char)*len);
	data[0] = '"';
	data[len+1] = '"';
	//INFO("dummy data was allocated for upload");

	auto successCallback = [this, len](HttpResponse &res)
	{
		if(res.code == HttpStatusCode::OK)
		{
			INFO("CODE %d, TIME %lu", res.code, res.timeTaken);
			float speed = len / float(float(res.timeTaken) / 1000);
			m_avgSpeed.push_back(speed);
			INFO("Uplink throughput = %f B/s", speed);
			m_speedCount++;
			INFO("Count %d", m_speedCount);

		}
		else
		{
			INFO("Timeout, counted as loss. CODE %d", res.code);
			m_avgSpeed.push_back(0);
			m_speedCount++;
			m_speedCountLoss++;
			INFO("Count %d", m_speedCount);

		}
	};

	auto failcallback = [this](HttpResponse &res)
	{
		INFO("fail to upload");
		m_avgSpeed.push_back(0);
		m_speedCount++;
		m_speedCountLoss++;
		INFO("Count %d", m_speedCount);
	};

	//INFO("queue now");


	for (size_t i = 0; i < numOfTrial; i++)
	{
		HttpRequest req;
		req.bGetResult = false;
		req.url = "http://35.240.207.36/api/values/";
		//INFO("ok1");
		req.data = (const char*)data;
		std::map<std::string,std::string> userdata = 
		{{ "Content-Type", "application/json" }};
		//INFO("ok2");
		req.header = userdata;
		req.action = ActionHttp::Post;
		req.length = len+3;
		//INFO("ok3");
		HttpDo(req, successCallback, failcallback, httpTimeout);
		//INFO("ok4");
	}
	
	//INFO("running...");

	while(m_speedCount < numOfTrial )
	{
		
	}


	float result = std::accumulate(m_avgSpeed.begin(), m_avgSpeed.end(), 0);
	if(result > 0)
	{
		result = result / m_avgSpeed.size();
	}
	delete []data;
	INFO("average uplink speed is %f B/s, after many %d trial, packet loss %d", result, m_avgSpeed.size(), m_speedCountLoss);
	return result;
}
