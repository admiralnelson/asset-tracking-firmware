#pragma once
#include <string>
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
#define INFO(...) { Serial.printf("[INFO ]  %s (%d) =>", __FUNCTION__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}
#define ERROR(...) { Serial.printf("[ERROR]  %s (%d) =>", __FUNCTION__, __LINE__); Serial.printf(__VA_ARGS__); Serial.println();}

//#define INFO(...) { Serial.printf("Info :"); Serial.printf(__VA_ARGS__); }
//#define ERROR(...) { Serial.printf("ERROR :"); Serial.printf(__VA_ARGS__); }

const char * String2Hex(const std::string& input);