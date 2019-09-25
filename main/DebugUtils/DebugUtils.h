#pragma once
#include <string>
#include <Arduino.h>
#include <esp_heap_caps.h>

using cstr = const char * const;

static constexpr cstr past_last_slash(cstr str, cstr last_slash)
{
	return
		*str == '\0' ? last_slash :
		(*str == '\\' || *str == '/') ? past_last_slash(str + 1, str + 1) :
		past_last_slash(str + 1, last_slash);
}

static constexpr cstr past_last_slash(cstr str)
{
	return past_last_slash(str, str);
}

#define __SHORT_FILE__ ({constexpr cstr sf__ {past_last_slash(__FILE__)}; sf__;})
#define INFO(...) { Serial.printf("[INFO ]  %s (%d) =>", __SHORT_FILE__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}
#define ERROR(...) { Serial.printf("[ERROR]  %s (%d) =>", __SHORT_FILE__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}	
#define FATAL(...) { Serial.printf("[FATAL]  %s (%d) =>", __SHORT_FILE__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}	

#define INFO_D(...) { Serial.printf("debug [INFO ]  %s (%d) =>", __SHORT_FILE__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}
#define ERROR_D(...) { Serial.printf("debug [ERROR]  %s (%d) =>", __SHORT_FILE__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}	

#define _DEBUG_ON 1

#ifdef _DEBUG_ON
	#undef INFO_D
	#undef ERROR_D

	#define INFO_D(...) {};
	#define ERROR_D(...) {};
#endif
//end

#define HEAP_CHECK() if(!heap_caps_check_integrity_all(true)) { FATAL("HEAP ERROR AT %s %d", __SHORT_FILE__, __LINE__); }
//#define INFO(...) { Serial.printf("Info :"); Serial.printf(__VA_ARGS__); }
//#define ERROR(...) { Serial.printf("ERROR :"); Serial.printf(__VA_ARGS__); }

const char * String2Hex(const std::string& input);