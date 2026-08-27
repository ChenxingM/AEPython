#pragma once
#include <AE_GeneralPlug.h>

#ifdef _WIN32
	#define AEPY_API __declspec(dllexport)
#else
	#define AEPY_API __attribute__((visibility("default")))
#endif

extern "C" {
	AEPY_API bool  AEPython_init(AEGP_PluginID my_id, SPBasicSuite* sP);
	AEPY_API bool  AEPython_exec(const char* utf8_code, const char* es_stack);
	AEPY_API char* AEPython_eval(const char* utf8_code, const char* es_stack);
	AEPY_API void  AEPython_free(char* p);
	AEPY_API void  AEPython_del_py_object(long id);
	AEPY_API void  AEPython_showWindow(void);
	AEPY_API void  AEPython_shutdown(void);
}
