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
	std::string providerName = "Unknown";
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
				INFO("Executing this command %s", cmd->command);
			}

			//INFO("QUEUED and sorted! expects %s for command %s", cmd->expects, cmd->command);
			m_cmdWaitingList.push_back(new CommandWait(
				(const char *)cmd->expects,
				(const char *)cmd->command,
				cmd->timeout,
				cmd->successCallback,
				cmd->failCallback)
			);
			if(strlen(cmd->command) > 0)
			{
				m_serialStream->println(cmd->command);
			}
			vTaskDelay(cmd->delay / portTICK_PERIOD_MS);
			if (cmd->delay > 100)
			{
				INFO("Write procedure was halted by %d", cmd->delay);
			}
			
			m_cmdsQueue.pop();
		}
		else
		{
			if(millis() - lastTime > REFRESH_RATE_MS)
			{
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
							Enqueue(new Command("ATE0", "OK", 100,100));
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
				std::string serialResult;
				serialResult.reserve(200);
				std::regex expects((*cmdw)->expects);
				int timeNow = millis();
				bool success = false;
				bool alreadyCalled = (*cmdw)->isAlreadyCalled;
				do
				{
					serialResult += ReadSerial();
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

					if (std::regex_search(serialResult, p_matches, expects))
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
						}
						success = true;
						if(cmd->nextChain != nullptr)
						{
							Enqueue(cmd->nextChain);
						}
						delete cmd;
						//INFO("SUCCESS! isAlreadyCalled %d", (*cmdw)->isAlreadyCalled);
					}
					alreadyCalled = (*cmdw)->isAlreadyCalled;

				} while (millis() - timeNow < (*cmdw)->timeout && !alreadyCalled);
				delete &p_matches;				
				if (!success)
				{
					(*cmdw)->isAlreadyCalled = true;
					if ((*cmdw)->failCallback != nullptr)
					{
						(*cmdw)->failCallback();
					}
					ERROR("None match for command (truncated) %s -> %s", (*cmdw)->command, serialResult.c_str());
					cmd->DestroyChain();
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

		{
			// m_signal = signalStrength;
			// m_providerName = providerName;
			// m_netPreffered = prefferedNet;
		}

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
	m_cmdsQueue.push(cmd);
}

int SerialModem::GetSignal()
{
	return -113 + m_signal * 2;
}

const char * SerialModem::GetProviderName()
{
	return m_providerName.c_str();
}


void SerialModem::ConnectGPRS(const char * apn, const char * username, const char * pass, unsigned int retry)
{
	m_isBusy = true;
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
			[this](){ m_isBusy = false; }
		);
	cmd->Chain(
		"AT+CGATT=0",
		"OK",
		5000, 3000,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("deegistered to net");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		"AT+CGATT=1",
		"OK",
		5000, 3000,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Registered to net");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",
		"OK",
		1000, 100,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Bearer set to GPRS");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		atSapbrApn.str().c_str(),
		"OK",
		1000, 100,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Bearer APN set");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		atSapbrUser.str().c_str(),
		"OK",
		1000, 200,
		[this](std::smatch  &m, char* p_data)
		{
			INFO("Bearer user set");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		atSapbrPass.str().c_str(),
		"OK",
		1000, 200,
		[](std::smatch  &m, char* p_data) 
		{
			INFO("Bearer pass set");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		gprsCmd.str().c_str(),
		"OK",
		1000, 500,
		[](std::smatch  &m, char* p_data)
		{
			INFO("GPRS APN set");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		"AT+CIICR",
		"OK",
		10000, 3000,
		[](std::smatch  &m, char* p_data) 
		{
			INFO("OK, WAITING TO QUERY IP NOW");
		},
		[this](){ m_isBusy = false; }
	)->Chain(
		"AT+CIFSR",
		"([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)",
		10000, 10000,
		[this](std::smatch  &m, char* p_data)
		{
			m_isGprsReady = true;
			m_ipaddr = IPAddress(
				atoi(m[1].str().c_str()),
				atoi(m[2].str().c_str()),
				atoi(m[3].str().c_str()),
				atoi(m[4].str().c_str())
			);
			INFO("IP address: %s", m_ipaddr.toString().c_str());
			m_isBusy = false;
			Enqueue(new SerialModem::Command(
				"AT+SAPBR=1,1", 
				"OK", 
				0, 100, 
				[](std::smatch  &s, char* p_data) { INFO("Bearer open!."); }
			));
		},
		[this]()
		{
			ERROR("FAIL to connect GPRS");
			m_isBusy = false;
		}
	);
	Enqueue(cmd);
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
		m_serialStream->println("AT+CLTS=1");
		std::string s = ReadSerial();
		INFO("%s", s.c_str());
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

	Command *c  = new Command(
		cmd.c_str(),
		"OK", 10000, 3000, nullptr, 
		[](){ ERROR("Failed to switch network");  }
	);
	c->Chain(
		"AT+CFUN=1,1",
		"OK", 10000, 0, 
		[=](std::smatch &s, char *p)
		{
			INFO("Success switched net!");	
		}
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