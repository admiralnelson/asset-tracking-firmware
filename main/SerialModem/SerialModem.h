// SerialModem.h
#pragma once
#include <Arduino.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <DebugUtils.h>
#include <regex>
#include <iostream>
#include <queue>
#include <string>
#include <sstream>
#include <algorithm>
#include <functional>
class SerialModem
{
public:
	struct Command
	{
		Command(const char * _command, 
				const char * _expects,
				unsigned int _timeout = 0,
				unsigned int _delay = 0,
				std::function<void(std::smatch&)> _successCallback = nullptr,
				std::function<void()> _failCallback = nullptr)
		{
			strcpy(command, _command);
			strcpy(expects, _expects);
			successCallback = _successCallback;
			failCallback	= _failCallback;
			timeout = _timeout;
			delay = _delay;

		}

		Command	*Chain(Command *cmd)
		{
			nextChain = cmd;
			return this;
		}

		void DestroyChain()
		{
			if (nextChain != nullptr)
			{
				delete nextChain;
			}
		}

		~Command()
		{
			DestroyChain();
		}

		unsigned int timeout;
		unsigned int delay;
		std::function<void(std::smatch&)> successCallback;
		std::function<void()>			  failCallback;
		char		 command[100];
		char		 expects[100];
		Command		*nextChain = nullptr;
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

private:
	struct CommandWait
	{
		CommandWait(const char * _expects, const char * _command, 
			unsigned int _timeout,
			std::function<void(std::smatch&)> _successCallback = nullptr, 
			std::function<void()> _failCallback = nullptr)
		{
			strcpy(expects, _expects);
			strcpy(command, _command);

			lastTime		= millis();
			timeout			= _timeout;
			successCallback = _successCallback;
			failCallback	= _failCallback;
		}
		bool isTimedOut()
		{
			return (millis() - lastTime) > timeout;
		}


		unsigned long lastTime;
		unsigned int  timeout;
		std::function<void(std::smatch&)> successCallback;
		std::function<void()>			  failCallback;
		char		  expects[100];
		char		  command[100];
		bool		  isAlreadyCalled = false;
	};
	
	int					m_signal = 0;
	ENetworkStatus		m_networkStatus;
	std::string			m_internetApn;
	std::string			m_internetUsername;
	std::string			m_internetPassword;
	std::string			m_providerName;
	std::queue<Command*> m_cmdsQueue;
	std::vector<CommandWait*> m_cmdWaitingList;
	Stream				 *m_serialStream;
	TaskHandle_t		 m_task;
	IPAddress			 m_ipaddr;
	ENetworkType		 m_netPreffered;
	bool				 m_isReady	   = false;
	bool				 m_isGprsReady = false;
	bool 				 m_isIgnoringNetState = false;
	bool 				 m_isBusy = false;
public:
	SerialModem();
	SerialModem(bool bIgnoreNetState);
	void	Begin(Stream *serialStream);
	void	Enqueue(Command* cmd);
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
	~SerialModem();

private:
	void		Loop();
	static void StartTaskImplLoop(void*);

	const char  *ReadSerial()
	{
		std::string out;
		while (m_serialStream->available())
		{
			out += (char)m_serialStream->read();
		}
		return out.c_str();
	}


};
