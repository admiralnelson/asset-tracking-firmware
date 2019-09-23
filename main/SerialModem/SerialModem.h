// SerialModem.h
#pragma once
#include <Arduino.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <regex>
#include <iostream>
#include <deque>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
#include <../DebugUtils/DebugUtils.h>

#define MAX_BUFFER 257
#define MAX_RETRY 20
#define REFRESH_RATE_MS 3000



class SerialModem
{
public:
	struct Command
	{
		Command(const char * _command, 
				const char * _expects,
				unsigned int _timeout = 0,
				unsigned int _delay = 0,
				std::function<void(std::smatch&, char*)> _successCallback = nullptr,
				std::function<void()> _failCallback = nullptr, 
				bool _doItUntilSuccess = false, 
				bool _insertNewLine = true, 
				size_t _commandLength = 0,
				unsigned int _waitBeforeWrite = 0)
		{
			if(_commandLength > 0)
			{
				commandLength = _commandLength + 1;
			}
			else
			{
				commandLength = strlen(_command) + 1 ;
			}
			HEAP_CHECK();
			//strcpy(command, _command);
			command = new char[commandLength];
			memset(command, 0, sizeof(char) * commandLength);
			memcpy(command, _command, sizeof(char) * commandLength);
			strcpy(expects, _expects);
			HEAP_CHECK();
			successCallback = _successCallback;
			failCallback	= _failCallback;
			timeout = _timeout;
			delay = _delay;
			m_isDoitUntilSuccess = _doItUntilSuccess;
			m_bAppendNewLine = _insertNewLine;
			waitBeforeWrite = _waitBeforeWrite;
		}

		Command	*Chain(const char * _command, 
				const char * _expects,
				unsigned int _timeout = 0,
				unsigned int _delay = 0,
				std::function<void(std::smatch&, char*)> _successCallback = nullptr,
				std::function<void()> _failCallback = nullptr,
				bool _doItUntilSuccess = false,
				bool _insertNewLine = true,
				size_t _commandLength = 0,
				unsigned int _waitBeforeWrite = 0)
		{
			nextChain = new Command(
				_command, _expects, _timeout, _delay, _successCallback, _failCallback, _doItUntilSuccess, _insertNewLine, _commandLength, _waitBeforeWrite
			);
			HEAP_CHECK();
			return nextChain;
		}

		void DestroyChain()
		{
			m_isDeletingNext = true;
			delete this;
		}

		~Command()
		{
			delete []command;
			if(m_isDeletingNext)
			{
				if(nextChain != nullptr)
				{
					nextChain->m_isDeletingNext = true;
					delete nextChain;
				}
			}
		}

		unsigned int timeout;
		unsigned int delay;
		unsigned int waitBeforeWrite;
		std::function<void(std::smatch&, char*)> successCallback;
		std::function<void()>			  failCallback;
		//char		 command[100];
		size_t 		commandLength = 0;
		char		*command;
		char		 expects[100];
		Command		*nextChain = nullptr;	
		bool		m_isDeletingNext = false; 
		bool		m_isDoitUntilSuccess = false;
		bool 		m_bAppendNewLine 	= false;
	};

	struct UdpPacket
	{
		IPAddress _ipAddr;
		char 	data[MAX_BUFFER - 1];
		char 	ip[100];
		unsigned int port;
	};

	struct UdpRequest
	{
		UdpPacket dataToSend;
		unsigned int timeout;
		std::function<void(UdpPacket &udp)> callbackOnReceive = nullptr;
		std::function<void()> callbackOnTimeout = nullptr;
	};

	enum ENetworkStatus
	{
		ENOT_YET_READY,
		ENOT_REGISTERED,
		EREGISTERED_HOME,
		EREGISTERED_ROAMING
	};

	enum ENetworkType
	{
		EUNKNOWN,
		EGSM,
		ECATM,
		ENBIOT
	};


protected:
	struct CommandWait
	{
		CommandWait(const char * _expects, 
			const char * _command, 
			unsigned int _timeout,
			std::function<void(std::smatch&, char* p_data)> _successCallback = nullptr, 
			std::function<void()> _failCallback = nullptr,
			unsigned int _commandLength = 0)
		{
			//strcpy(command, _command);
			if(_commandLength > 0)
			{
				commandLength = _commandLength;
			}
			else
			{
				commandLength = strlen(_command) + 1;
			}
			command = new char[commandLength];
			memset(command, 0, sizeof(char) * commandLength);
			memcpy(command, _command, sizeof(char) * commandLength);
			strcpy(expects, _expects);

			lastTime		= millis();
			timeout			= _timeout;
			successCallback = _successCallback;
			failCallback	= _failCallback;
		}
		bool isTimedOut()
		{
			return (millis() - lastTime) > timeout;
		}
		~CommandWait()
		{
			delete []command;
		}

		unsigned long lastTime;
		unsigned int  timeout;
		std::function<void(std::smatch&, char* p_data)> successCallback;
		std::function<void()>			  failCallback;
		char		  expects[100];
		char		  *command;
		bool		  isAlreadyCalled = false;
		unsigned int  commandLength;
	};
	
	int					m_signal = 0;
	ENetworkStatus		m_networkStatus;
	std::string			m_internetApn;
	std::string			m_internetUsername;
	std::string			m_internetPassword;
	std::string			m_providerName;
	std::deque<Command*> m_cmdsQueue;
	std::deque<Command*> m_onHoldQueue;
	std::deque<UdpRequest> m_udpQueue;
	std::vector<CommandWait*> m_cmdWaitingList;
	Stream				 *m_serialStream;
	TaskHandle_t		 m_task;
	IPAddress			 m_ipaddr;
	ENetworkType		 m_netPreffered;
	char				 *m_serialBuffer;
	bool				 m_isReady	   = false;
	bool				 m_isGprsReady = false;
	bool 				 m_isIgnoringNetState = false;
	bool 				 m_isBusy = false;
	
public:
	SerialModem();
	SerialModem(bool bIgnoreNetState);
	void	Begin(Stream *serialStream);
	void	Enqueue(Command* cmd);
	void	ForceEnqueue(Command* cmd);
	void 	SendUdp(UdpRequest &udpReq);
	void 	SetEdrx(uint8_t edrxVal);
	int		GetSignal();
	const char	 *GetProviderName();
	void		 ConnectGPRS(const char * apn,const char *username, const char *pass, unsigned int retry);
	ENetworkStatus	GetNetworkStatus()
	{
		return m_networkStatus;
	}
	bool		 GetIsGPRSConnected()
	{
		return m_isGprsReady;
	}
	IPAddress	 GetIPAddress()
	{
		return m_ipaddr;
	}
	bool 		isBusy()
	{
		return m_isBusy;
	}
	void 		SetPrefferedNetwork(ENetworkType net);
	ENetworkType GetPrefferedNetwork();
	~SerialModem()
	{
		vTaskDelete(m_task);
		delete []m_serialBuffer;
	}

protected:
	void		Loop();
	static void StartTaskImplLoop(void*);
	const char* ReadSerial()
	{
		int i = 0;
		memset(m_serialBuffer, 0, sizeof(char) * MAX_BUFFER);
		while (m_serialStream->available())
		{
			if(i < MAX_BUFFER)
			{
				unsigned char c =(unsigned char) m_serialStream->read();
				if(c > 0)
				{
					m_serialBuffer[i] += c ;					
				}
				i++;
			}
			else
			{
				return  m_serialBuffer;
			}
		}
		return  m_serialBuffer;
	}

private:
	unsigned GetNumberOfDigits (unsigned i)
	{
		return i > 0 ? (int) log10 ((double) i) + 1 : 1;
	}

};
