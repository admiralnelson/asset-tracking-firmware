#include <SerialModem.h>
SerialModem::SerialModem()
{
	
}

SerialModem::SerialModem(bool bIgnoreNetState)
{
	m_isIgnoringNetState = bIgnoreNetState;
}


void SerialModem::WriteLoop()
{
	std::string providerName = "Unknown";
	float signalStrength = 0;
	ENetworkType prefferedNet;
	
	while (true)
	{
		if (!m_isReady)
		{
			m_networkStatus = ENOT_YET_READY;
			ERROR("Modem is not ready");
			continue;
		}

		if (xSemaphoreTake(m_xSemaphore, 500 / portTICK_PERIOD_MS) == pdTRUE)
		{

			if (m_cmdsQueue.size() > 0)
			{
				//INFO("Executing...%d", m_cmdsQueue.size());

				Command* cmd = m_cmdsQueue.front();
				bool untilSuccess = false;
				
				bool bcommonCmd = strcmp("AT+COPS?", (const char*)cmd->command) == 0;
				bcommonCmd = bcommonCmd || strcmp("AT+CSQ", (const char*)cmd->command) == 0;
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

				// std::sort(m_cmdWaitingList.begin(), m_cmdWaitingList.end(), [](const CommandWait *lhs, const CommandWait *rhs)
				// {
				// 	return lhs->timeout < rhs->timeout;
				// });

				m_serialStream->println(cmd->command);
				//delay(cmd->delay);
				vTaskDelay(cmd->delay / portTICK_PERIOD_MS);
				if (cmd->delay > 100)
				{
					INFO("Write procedure was halted by %d", cmd->delay);
				}
				

				m_cmdsQueue.pop();
				delete cmd;
			}
			else
			{
				Enqueue(new SerialModem::Command(
					"AT+COPS?",
					"([0-9]*),([0-9]*),\"(.*)?\",([0-9]*)", 0, 100,
					[this](std::smatch &m)
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
					[this](std::smatch &m)
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

			}

			{
				// m_signal = signalStrength;
				// m_providerName = providerName;
				// m_netPreffered = prefferedNet;
			}
			//INFO("RELEASED SEMAPHORE");
			xSemaphoreGive(m_xSemaphore);

		}
		else
		{
			//INFO("WRITE: TASK BLOCKED");

		}
	}

}

void SerialModem::ReadLoop()
{
	while (true)
	{
		if (!m_isReady)
		{
			m_networkStatus = ENOT_YET_READY;
			ERROR("Modem is not ready");
			continue;;
		}

		if (xSemaphoreTake(m_xSemaphore, 1000 / portTICK_PERIOD_MS) == pdTRUE)
		{
			for (std::vector<CommandWait*>::iterator cmdw = m_cmdWaitingList.begin();
				cmdw != m_cmdWaitingList.end();
				cmdw++)
			{
				if (!(*cmdw)->isAlreadyCalled)
				{
					std::smatch matches;
					std::string serialResult;
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

						if (std::regex_search(serialResult, matches, expects))
						{
							(*cmdw)->isAlreadyCalled = true;
							if ((*cmdw)->successCallback != nullptr)
							{
								(*cmdw)->successCallback(matches);
							}
							success = true;
							//INFO("SUCCESS! isAlreadyCalled %d", (*cmdw)->isAlreadyCalled);
						}
						alreadyCalled = (*cmdw)->isAlreadyCalled;

					} while (millis() - timeNow < (*cmdw)->timeout && !alreadyCalled);
					if (!success)
					{
						(*cmdw)->isAlreadyCalled = true;
						if ((*cmdw)->failCallback != nullptr)
						{
							(*cmdw)->failCallback();
						}
						ERROR("None match for command %s -> %s", (*cmdw)->command, serialResult.c_str());
					}
				}
			}
			std::vector<CommandWait*>::iterator cmdw = m_cmdWaitingList.begin();
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
			//INFO("CLEANED UP!");
			xSemaphoreGive(m_xSemaphore);
		}
		else
		{
			//INFO("READ: TASK BLOCKED");
		}
	}

}

void SerialModem::StartTaskImplWriteLoop(void * thisObject)
{
	INFO("STARTING WRITE LOOP TASK, %x", (unsigned int)thisObject);
	static_cast<SerialModem*>(thisObject)->WriteLoop();
	INFO("DONE");

}

void SerialModem::StartTaskImplReadLoop(void * thisObject)
{
	INFO("STARTING READ LOOP TASK, %x", (unsigned int)thisObject);
	static_cast<SerialModem*>(thisObject)->ReadLoop();
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
	bool isReady = false;
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
	Enqueue(new Command(
		"AT+CIPSHUT", 
		"SHUT OK",
		1000, 1000,
		[this](std::smatch &m)
		{
			INFO("GPRS should be disconnected now");
		}
	));
	Enqueue(new Command(
		"AT+CGATT=0",
		"OK",
		5000, 3000,
		[this](std::smatch &m)
		{
			INFO("deegistered to net");
		}
	));
	Enqueue(new Command(
		"AT+CGATT=1",
		"OK",
		5000, 3000,
		[this](std::smatch &m)
		{
			INFO("Registered to net");
		}
	));

	//SAPBR SETUP
	Enqueue(new Command(
		"AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"",
		"OK",
		1000, 100,
		[this](std::smatch &m)
		{
			INFO("Bearer set to GPRS");
		}
	));
	Enqueue(new Command(
		atSapbrApn.str().c_str(),
		"OK",
		1000, 100,
		[this](std::smatch &m)
		{
			INFO("Bearer APN set");
		}
	));
	Enqueue(new Command(
		atSapbrUser.str().c_str(),
		"OK",
		1000, 200,
		[this](std::smatch &m)
		{
			INFO("Bearer user set");
		}
	));
	Enqueue(new Command(
		atSapbrPass.str().c_str(),
		"OK",
		1000, 200,
		[](std::smatch &m) 
		{
			INFO("Bearer pass set");
		}
	));
	//SAPBR END

	Enqueue(new Command(
		gprsCmd.str().c_str(),
		"OK",
		1000, 500,
		[](std::smatch &m)
		{
			INFO("GPRS APN set");
		}
	));
	Enqueue(new Command(
		"AT+SAPBR=1,1",
		"OK",
		1000, 200,
		[](std::smatch &m) 
		{
			INFO("Bearer OPEN, connecting now");
		}
	));
	Enqueue(new Command(
		"AT+CIICR",
		"OK",
		10000, 3000,
		[](std::smatch &m) 
		{
			INFO("OK, WAITING TO QUERY IP NOW");
		}
	));

	Enqueue(new Command(
		"AT+CIFSR",
		"([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)",
		10000, 10000,
		[this](std::smatch &m)
		{
			m_isGprsReady = true;
			m_ipaddr = IPAddress(
			atoi(m[1].str().c_str()),
			atoi(m[2].str().c_str()),
			atoi(m[3].str().c_str()),
			atoi(m[4].str().c_str()));
			INFO("IP address: %s", m_ipaddr.toString().c_str());
		},
		[]()
		{
			ERROR("FAIL to connect GPRS");
		}
	));
}

void SerialModem::Begin(Stream *serialStream)
{
	m_serialStream = serialStream;
	bool notReady = true;
	int trial = 0;
	while (notReady && trial < 10)
	{
		m_serialStream->println("AT");
		std::string s = ReadSerial();
		std::string output = String2Hex(s);
		notReady = (s.find("OK") == std::string::npos);
		INFO("REady ? %s , %s\n", s.c_str(), output.c_str());
		INFO("Wait for 200ms");
		delay(200);
		trial++;
	}
	if (trial < 10)
	{
		m_isReady = true;
		INFO("Modem is ready!");
		m_serialStream->println("ATE0");
		m_serialStream->println("AT+CLTS=1");
		std::string s = ReadSerial();
		INFO("%s", s.c_str());
		m_xSemaphore =  xSemaphoreCreateMutex();
		xTaskCreatePinnedToCore(this->StartTaskImplWriteLoop, "SerialModem_LoopW", 16384, this, 1, &m_taskWrite, 1);
		xTaskCreatePinnedToCore(this->StartTaskImplReadLoop, "SerialModem_LoopR", 16384, this, 1, &m_taskRead, 1);


	}
	else
	{
		INFO("Modem not connected");
	}

}