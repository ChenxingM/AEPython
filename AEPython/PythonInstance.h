#pragma once
#include <string>
#include <AE_GeneralPlug.h>

#ifdef _WIN32
	#define AEPY_API __declspec(dllexport)
#else
	#define AEPY_API __attribute__((visibility("default")))
#endif

namespace AEPython
{
	AEPY_API void init(AEGP_PluginID _my_id, SPBasicSuite* _sP);
	AEPY_API bool exec(const std::string& utf8_code, const std::string& esStack);
	AEPY_API std::string eval(const std::string& utf8_code, const std::string& esStack);
	AEPY_API void del_py_object(const long id);
	AEPY_API void showWindow();
}
