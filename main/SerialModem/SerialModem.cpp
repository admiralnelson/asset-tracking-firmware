#include "SerialModem.h"



SerialModem::SerialModem()
{
	m_serialBuffer = new char[MAX_BUFFER];
}

SerialModem::SerialModem(bool bIgnoreNetState)
{
	m_isIgnoringNetState = bIgnoreNetState;
}


void SerialModem::Loop()
{
	static unsigned int lastTime = 0 ;
	Command* cmd = nullptr;


	while (true)
	{
		if (!m_isReady)
		{
			m_networkStatus = ENOT_YET_READY;
			ERROR("Modem is not ready");
			continue;
		}

		if (m_cmdsQueue.size() > 0)
		{
			cmd = m_cmdsQueue.front();
			bool untilSuccess = false;
			
			bool bcommonCmd = strcmp("AT+COPS?", (const char*)cmd->command) == 0;
			bcommonCmd = bcommonCmd || strcmp("AT+CSQ", (const char*)cmd->command) == 0;
			bcommonCmd = bcommonCmd || strcmp("AT+CNMP?", (const char*)cmd->command) == 0;
			bcommonCmd = bcommonCmd || strcmp("ATE0", (const char*)cmd->command) == 0;

			if (!bcommonCmd)
			{
				INFO("Executing this command (truncated, append newline yes? :%d) %.*s", cmd->m_bAppendNewLine, 100, cmd->command);
			}
			if(cmd->nextChain != nullptr)
			{
				//we entered chained mode, 
				m_isBusy = true;
			}
			//INFO("QUEUED and sorted! expects %s for command %s", cmd->expects, cmd->command);
			m_cmdWaitingList.push_back(new CommandWait(
				(const char *)cmd->expects,
				(const char *)cmd->command,
				cmd->timeout,
				cmd->successCallback,
				cmd->failCallback,
				cmd->commandLength)
			);
			HEAP_CHECK(); //BANG, ERROR JIKA TOTAL STRING COMMAND > 100. note was FIXED
			vTaskDelay(cmd->waitBeforeWrite / portTICK_PERIOD_MS);
			if(m_failCount > FAIL_COUNT)
			{
				for (size_t i = 0; i < 10; i++)
				{
					m_serialStream->println("ATE0");
				}
				m_failCount = 0;
			}
			if(cmd->commandLength > 0)
			{
				if(cmd->m_bAppendNewLine)
				{
					m_serialStream->println(cmd->command);
				}
				else
				{
					m_serialStream->print(cmd->command);					
				}
				
			}
			vTaskDelay(cmd->delay / portTICK_PERIOD_MS);
			if (cmd->delay > 100)
			{
				INFO("Write procedure was halted by %d", cmd->delay);
			}
			if(!cmd->m_isDoitUntilSuccess)
			{
				m_cmdsQueue.pop_front();				
			}
		}
		else
		{
			if(millis() - lastTime > REFRESH_RATE_MS)
			{
				Enqueue(new Command("ATE0", "OK", 100,0));
				Enqueue(new SerialModem::Command(
					"AT+COPS?",
					"([0-9]*),([0-9]*),\"(.*)?\",([0-9]*)", 0, 100,
					[this](std::smatch  &m, char* p_data)
					{
						if (m.length() > 4)
						{
							int networkType = atoi(m[4].str().c_str());
							m_providerName = m[3].str().c_str();
							m_networkStatus = EREGISTERED_HOME;
							switch (networkType)
							{
							case 0:
								m_netPreffered = EGSM;
								break;
							case 7:
								m_netPreffered = ECATM;
								break;
							case 9:
								m_netPreffered = ENBIOT;
								break;
							default:
								m_netPreffered = EUNKNOWN;
								break;
							}
						}
					},
					[this]()
					{
						m_providerName = "Unknown";
						m_networkStatus = ENOT_REGISTERED;
					}
				));

				Enqueue(new SerialModem::Command(
					"AT+CSQ",
					"([0-9]*),([0-9]*)", 0, 100,
					[this](std::smatch  &m, char* p_data)
					{
						if (m.length() > 1)
						{
							m_signal = atoi(m[1].str().c_str());
						}
					},
					[this]()
					{
						m_signal = NAN;
					}
				));

				Enqueue(new SerialModem::Command(
					"AT+CNMP?",
					"CNMP: ([0-9]*)", 0, 100,
					[this](std::smatch  &m, char* p_data)
					{
						if (m.length() > 1)
						{
							m_netPreffered = (ENetworkType)atoi(m[1].str().c_str());
						}
					},
					[this]()
					{
						m_netPreffered = EUNKNOWN;
					}
				));
				

				lastTime = millis();	
			}
			

		}
		for (auto cmdw = m_cmdWaitingList.begin();
			cmdw != m_cmdWaitingList.end();
			cmdw++)
		{
			if (!(*cmdw)->isAlreadyCalled)
			{
				std::smatch &p_matches = *new std::smatch;
				HEAP_CHECK();
				std::string serialResult;
				serialResult.reserve(MAX_BUFFER);
				std::regex expects((*cmdw)->expects);
				int timeNow = millis();
				bool success = false;
				bool alreadyCalled = (*cmdw)->isAlreadyCalled;
				MicroNMEA gpsParser(m_nmeaBuffer, sizeof(m_nmeaBuffer));
				do
				{
					read_nmea_string_again:
					serialResult = ReadSerial();

					if(m_isGpsOn)
					{
						if( millis() - timeNow > m_gpsTimeout)
						{
							if(m_gpsCallbackTimeout != nullptr)
							{
								m_gpsCallbackTimeout();
							}
							ERROR("GPS fix timed out!");
							TurnOffGps();
						}
						if(serialResult.length() > 0)
						{
							INFO("NMEA string: %s", serialResult.c_str());
							for (char &c : serialResult)
							{
								gpsParser.process(c);							
							}
							if(!gpsParser.isValid())
							{
								goto read_nmea_string_again;
							}
							else
							{
								INFO("GPS location LAT: %ld, LONG: %ld", gpsParser.getLatitude(), gpsParser.getLongitude());
								Location loc;
								loc.longitude = (double)gpsParser.getLongitude() / 1000000;
								loc.latitude = (double) gpsParser.getLatitude() / 1000000;
								long alt;
								loc.altitude = (gpsParser.getAltitude(alt)) ? (double) alt / 1000000 : NAN;								
								loc.speed = (double)gpsParser.getSpeed() / 1000000;
								loc.course = (double)gpsParser.getCourse() / 1000000;
								loc.hdop = (float)gpsParser.getHDOP() / 10;

								loc.gpsTime.tm_sec = gpsParser.getSecond();
								loc.gpsTime.tm_min = gpsParser.getMinute();
								loc.gpsTime.tm_hour = gpsParser.getHour();

								loc.gpsTime.tm_mday = gpsParser.getDay();
								loc.gpsTime.tm_mon = gpsParser.getMonth();
								loc.gpsTime.tm_year = gpsParser.getYear();

								INFO("GPS CALLBACK SHOULD BE CALLED!");
								m_gpsCallbackSuccess(loc);
								TurnOffGps();
							}
						}
						else
						{
							goto read_nmea_string_again;
						}
						
					}

					/*bool bcommonCmd = strcmp("AT+COPS?", (const char*)(*cmdw)->command) == 0;
					bcommonCmd = bcommonCmd || strcmp("AT+CSQ", (const char*)(*cmdw)->command) == 0;
					if (!bcommonCmd && serialResult.size() > 0)
					{
						INFO("Current serial data %s", serialResult.c_str());
					}*/

					std::smatch matchForPDP;
					std::regex matchForPDPKeyword("PDP: DEACT");
					if(std::regex_search(serialResult, matchForPDP, matchForPDPKeyword))
					{
						m_isGprsReady  = false;
					}

					if (std::regex_search(serialResult, p_matches, expects) || strlen((*cmdw)->expects) == 0 )
					{
						(*cmdw)->isAlreadyCalled = true;
						if ((*cmdw)->successCallback != nullptr)
						{
							// bool bcommonCmd = strcmp("AT+COPS?", (const char*)(*cmdw)->command) == 0;
							// bcommonCmd = bcommonCmd || strcmp("AT+CSQ", (const char*)(*cmdw)->command) == 0;
							// if (!bcommonCmd && strlen(serialResult) > 0)
							// {
							// 	INFO("Current serial data (truncated) %s", serialResult.c_str());
							// }
							(*cmdw)->successCallback(p_matches, m_serialBuffer);
							if(cmd->m_isDoitUntilSuccess)
							{
								m_cmdsQueue.pop_front();
							}
						}
						success = true;
						m_failCount = 0;
						if(cmd->nextChain != nullptr)
						{
							ForceEnqueue(cmd->nextChain);
						}
						else
						{
							//we exited chained mode, 
							m_isBusy = false;
						}
						delete cmd;
						//INFO("SUCCESS! isAlreadyCalled %d", (*cmdw)->isAlreadyCalled);
					}
					alreadyCalled = (*cmdw)->isAlreadyCalled;

				} while (millis() - timeNow < (*cmdw)->timeout && !alreadyCalled);
				delete &p_matches;	
				HEAP_CHECK();
				if (!success)
				{
					m_failCount++;
					(*cmdw)->isAlreadyCalled = true;
					if ((*cmdw)->failCallback != nullptr)
					{
						(*cmdw)->failCallback();
					}
					ERROR("None match for command (truncated) %s -> %s", (*cmdw)->command, serialResult.c_str());
					if(cmd->m_isDoitUntilSuccess)
					{
						INFO("Redoing for command, %s", cmd->command);
					}
					else
					{
						cmd->DestroyChain();
						//we exited chained mode, 
						m_isBusy = false;					
					}
				}
			}
		}
		auto cmdw = m_cmdWaitingList.begin();
		while (cmdw != m_cmdWaitingList.end())
		{
			if ((*cmdw)->isAlreadyCalled)
			{
				delete (*cmdw);
				cmdw = m_cmdWaitingList.erase(cmdw);
			}
			else
			{
				++cmdw;
			}
		}

		if(m_onHoldQueue.size() > 0 && !m_isBusy)
		{
			while(m_onHoldQueue.size() > 0)
			{
				Enqueue(m_onHoldQueue.front());
				m_onHoldQueue.pop_front();
			}
		}
		HEAP_CHECK();
	}

}

void SerialModem::StartTaskImplLoop(void * thisObject)
{
	INFO("STARTING WRITE LOOP TASK, %x", (unsigned int)thisObject);
	static_cast<SerialModem*>(thisObject)->Loop();
	INFO("DONE");

}


void SerialModem::Enqueue(Command *cmd)
{
	if(!m_isBusy)
	{
		m_cmdsQueue.push_back(cmd);
	}
	else
	{
		m_onHoldQueue.push_back(cmd);
	}
}

void SerialModem::ForceEnqueue(Command *cmd)
{
	m_cmdsQueue.push_front(cmd);
	INFO("FORCE ENQUE %s", m_cmdsQueue.front()->command);
}

void SerialModem::SendUdp(UdpRequest udpReq)
{
	std::stringstream cipstartUdp;
	cipstartUdp << "AT+CIPSTART=" << "\"UDP\"," << "\"" << udpReq.dataToSend->domain << "\"," << udpReq.dataToSend->port; 
	
	m_udpQueue.push_back(udpReq);
	Command *c = new Command(
		"AT+CIPSRIP=1", 
		"OK", 
		10000, 10
	);
	c->Chain(
		cipstartUdp.str().c_str(),
		"OK",
		1000,0,
		nullptr,
		[=]()
		{
			ForceEnqueue(new Command(
				"AT+CIPCLOSE",
				"CLOSE OK",
				10000, 100
			));
		}
	)->Chain(
		"AT+CIPSEND", ">", 
		1000, 0
	)->Chain(
		udpReq.dataToSend->data, 
		"",
		0, 0, nullptr, nullptr, false, false
	)->Chain(
		"\032",
		"SEND OK", 
		5000, 0, 
		[this](std::smatch &s, char* p_data)
		{
			UdpRequest &q = m_udpQueue.front();
			q._timestamp = millis();
		}
	)->Chain(
		"",
		"^\r\nRECV FROM:([0-9]*).([0-9]*).([0-9]*).([0-9]*):([0-9]*)",
		udpReq.timeout, 0,
		[=](std::smatch &s, char *p_data)
		{
			size_t fsOctect = atoi(s[1].str().c_str()),
				   ndOctect = atoi(s[2].str().c_str()),
				   rdOctect = atoi(s[3].str().c_str()),
				   thOctect = atoi(s[4].str().c_str()),
				   port = atoi(s[5].str().c_str());
			IPAddress addr(fsOctect, ndOctect, rdOctect, thOctect);
			size_t offset = 14 + GetNumberOfDigits(fsOctect)
							   + GetNumberOfDigits(ndOctect)
							   + GetNumberOfDigits(rdOctect)
							   + GetNumberOfDigits(thOctect)
							   + GetNumberOfDigits(port)
							   + 2 ;
			UdpRequest &udp = m_udpQueue.front();
			UdpPacket udpPack((const char*)p_data + offset, addr.toString().c_str(), port);
			udpPack._rtt = (millis() - 10) - udp._timestamp;
			HEAP_CHECK();
			INFO("udpacket data %s", udpPack.data);
			INFO("udpacket domain %s", udpPack.domain);
			if(udp.callbackOnReceive != nullptr)
			{
				udp.callbackOnReceive(udpPack);
			}
			INFO("udp callbac should be executed");
			m_udpQueue.pop_front();
			INFO("deque");

		},
		[=]()
		{
			UdpRequest &udp = m_udpQueue.front();
			if(udp.callbackOnTimeout != nullptr)
			{
				udp.callbackOnTimeout();
			}
			INFO("udp callbac should be executed");
			m_udpQueue.pop_front();
			ForceEnqueue(new Command(
				"AT+CIPCLOSE",
				"CLOSE OK",
				10000, 100
			));
		}
	)->Chain(
		"AT+CIPCLOSE",
		"CLOSE OK",
		10000, 100
	);
	Enqueue(c);
}

void SerialModem::SetEdrx(uint8_t edrxVal)
{
	std::stringstream edrxCommand;
	edrxCommand << "AT+CEDRXS=";

	if(edrxVal > 16)
	{
		ERROR("Invalid EDRX value (max 16)");
		return;
	}

	char edrx[4];
	memset(edrx, '0', sizeof(char) * 4);
	itoa(edrxVal, edrx, 2);
	if(m_netPreffered == ENBIOT)
	{	
		edrxCommand << 1 << "," << 5 << "," << "\"" << edrx << "\"";		
		Command *c = new Command
		(
			edrxCommand.str().c_str(),
			"OK", 2000, 100, 
			[](std::smatch &s, char *)
			{
				INFO("EDRX value has been set");
			} ,
			[]()
			{
				ERROR("Fail to set EDRX value!");
			}
		);
		Enqueue(c);
	}
	else if(m_netPreffered == ECATM)
	{
		edrxCommand << 1 << "," << 4 << "," << "\"" << edrx << "\"";		
		Command *c = new Command
		(
			edrxCommand.str().c_str(),
			"OK",2000, 100, 
			[](std::smatch &s, char *)
			{
				INFO("EDRX value has been set");
			} ,
			[]()
			{
				ERROR("Fail to set EDRX value!");
			}
		);
		Enqueue(c);
	}
	else
	{
		ERROR("GSM not supported!");
	}
}

void SerialModem::MeasureTCPHandshakeTime(unsigned int howManyTime, const char *domain,  unsigned int port)
{
	std::stringstream cipstartTcp;
	cipstartTcp << "AT+CIPSTART=" << "\"TCP\"," << "\"" << domain << "\"," << port;   
	
	Command *c = new Command(
		"AT+CIPCLOSE",
		".*",
		10000, 100
	);
	Command *c2 = c->Chain(
		"AT+CIPSRIP=1", 
		"OK", 
		10000, 10
	);
	for (size_t i = 0; i < howManyTime; i++)
	{
		c2 = c2->Chain(
			cipstartTcp.str().c_str(),
			"CONNECT OK",
			20000,500, 
			[](std::smatch &s, char *p)
			{
				INFO("Connection success!");
			},
			[=]()
			{
				ERROR_D("unable to connect");
				ForceEnqueue(new Command(
					"AT+CIPCLOSE",
					"CLOSE OK",
					10000, 100
				));
			}
		)->Chain(
			"AT+CIPCLOSE",
			"CLOSE OK",
			10000, 100
		);
	}
	Enqueue(c);
}

int SerialModem::GetSignal()
{
	return -113 + m_signal * 2;
}

const char * SerialModem::GetProviderName()
{
	return m_providerName.c_str();
}

const char * SerialModem::GetIMEI()
{
	return m_imei.c_str();
}

void SerialModem::ConnectGPRS(const char * apn, const char * username, const char * pass, unsigned int retry)
{
	if(m_isBusy)
	{
		INFO("Modem is busy");
		return;
	}
	std::stringstream gprsCmd;
	std::stringstream atSapbrApn, atSapbrUser, atSapbrPass;
	atSapbrApn << "AT+SAPBR=3,1,\""<< "APN" << "\",\"" << apn << "\"";
	atSapbrUser << "AT+SAPBR=3,1,\""<< "USER" << "\",\"" << username << "\"";
	atSapbrPass << "AT+SAPBR=3,1,\""<< "PWD" << "\",\"" << pass << "\"";
	
	gprsCmd << "AT+CSTT=\"" <<  apn << "\",\"" <<  username << "\",\"" <<  pass << "\"";
	
	if(!m_isIgnoringNetState)
	{
		if (m_networkStatus != EREGISTERED_HOME)
		{
			ERROR("Modem is not connected to network!");
			return;
		}
	}
	else
	{
		INFO("Igonring network status");
	}

	delay(3000);
	char currentRet = 0;
	Command *cmd = 
		new Command(
			"AT+CIPSHUT", 
			"SHUT OK",
			1000, 1000,
			[this](std::smatch  &m, char* p_data)
			{
				INFO("GPRS should be disconnected now");
			},
			[this](){  }
		);
	cmd->Chain(
		"AT+CGATT=0",
		"OK",
		5000, 3000,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("deegistered to net");
		},
		[this](){  }
	)->Chain(
		"AT+CGATT=1",
		"OK",
		5000, 3000,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Registered to net");
		},
		[this](){  }
	)->Chain(
		"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",
		"OK",
		1000, 100,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Bearer set to GPRS");
		},
		[this](){  }
	)->Chain(
		atSapbrApn.str().c_str(),
		"OK",
		1000, 100,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Bearer APN set");
		},
		[this](){  }
	)->Chain(
		atSapbrUser.str().c_str(),
		"OK",
		1000, 200,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Bearer user set");
		},
		[this](){  }
	)->Chain(
		atSapbrPass.str().c_str(),
		"OK",
		1000, 200,
		[](std::smatch  &m, char* p_data) 
		{
			INFO("Bearer pass set");
		},
		[this](){  }
	)->Chain(
		gprsCmd.str().c_str(),
		"OK",
		1000, 500,
		[](std::smatch  &m, char* p_data)
		{
			INFO("GPRS APN set");
		},
		[this](){  }
	)->Chain(
		"AT+CIICR",
		"OK",
		10000, 3000,
		[](std::smatch  &m, char* p_data) 
		{
			INFO("Querying IP...");
		},
		[this](){  }
	)->Chain(
		"AT+CIFSR",
		"([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)",
		10000, 10000,
		[this](std::smatch  &m, char* p_data)
		{
			m_ipaddr = IPAddress(
				atoi(m[1].str().c_str()),
				atoi(m[2].str().c_str()),
				atoi(m[3].str().c_str()),
				atoi(m[4].str().c_str())
			);
			INFO("IP address: %s", m_ipaddr.toString().c_str());
		},
		[this]()
		{
			ERROR("FAIL to connect GPRS");
			
		}
	)->Chain(
		"AT+SAPBR=1,1", 
		"OK", 
		1000, 100, 
		[this](std::smatch  &s, char* p_data) 
		{ 
			INFO("Bearer open!."); 
			m_isGprsReady = true;
		},
		[this]()
		{
			ERROR("FAIL to connect GPRS");
			
		}
	);
	m_isBusy = true;
	ForceEnqueue(cmd);
}

void SerialModem::Begin(Stream *serialStream)
{
	m_serialStream = serialStream;
	bool notReady = true;
	int trial = 0;
	while (notReady && trial < MAX_RETRY)
	{
		m_serialStream->println("AT");
		std::string s = ReadSerial();
		notReady = (s.find("OK") == std::string::npos);
		INFO("REady ? %s \n", s.c_str());
		INFO("Wait for 200ms");
		delay(200);
		trial++;
	}
	if (trial < MAX_RETRY)
	{
		m_isReady = true;
		INFO("Modem is ready!");
		m_serialStream->println("ATE0");
		delay(100);
		m_serialStream->println("AT+CLTS=1");
		std::string s = ReadSerial();
		INFO("%s", s.c_str());
		Enqueue(new Command
		(
			"AT+CGSN", "^\r\n([0-9]+)", 1000, 100,
			[this](std::smatch &s, char *c)
			{
				INFO("Serial number is %s", s[1].str().c_str());
				m_imei = s[1].str();
			}
		));
		xTaskCreatePinnedToCore(this->StartTaskImplLoop, "SerialModem_Loop", 16384 * 2, this, 1,  &m_task, 1);
		
	}
	else
	{
		INFO("Modem not connected");
	}

}


void SerialModem::SetPrefferedNetwork(ENetworkType net)
{
	std::string cmd = "AT+CNMP=";
	switch (net)
	{
	case EGSM:
		cmd += "13";
		break;
	case ENBIOT:
		cmd += "38";
		break;	
	case ECATM:
		cmd += "38";
		break;
	default:
		break;
	};

	INFO("Changing net...");
	Command *c  = new Command(
		cmd.c_str(),
		"OK", 10000, 0, 
		[=](std::smatch &s, char *p)
		{
			INFO("Net changed.");
		}, 
		[](){ ERROR("Failed to switch network");  }
	);

	if(net == ECATM)
	{
		//for catM//
	}
	
	Enqueue(c);
	
}

SerialModem::ENetworkType SerialModem::GetPrefferedNetwork()
{
	return m_netPreffered;
}


//NOTE: this blocks the queue!
void SerialModem::TurnOnGps(std::function<void(Location)> callback, unsigned int timeout, std::function<void()> failCallback )
{
	if(m_isGpsOn || m_isBusy)
	{
		ERROR("Driver is busy!");
		return;
	}
	Command *c = new Command("AT+CGNSPWR=1", "OK", 1000, 100);
	c->Chain("AT+CGNSTST=1","OK", 1000, 1000, 
	[this, callback, timeout, failCallback](std::smatch &s, char *c)
	{
		m_isGpsOn = true;
		m_isBusy = true;
		m_gpsTimeout = timeout;
		m_gpsCallbackSuccess = callback;
		m_gpsCallbackTimeout = failCallback;
		INFO("GPS is ON, and driver is BUSY");
	});
	Enqueue(c);

}

void SerialModem::TurnOffGps()
{
	m_isGpsOn = false;
	m_isBusy = false;
	Command *c = new Command("AT+CGNSPWR=0", "", 1000, 100);
	Enqueue(c);
}