#include "AEConfig.h"
#include "entry.h"

#ifdef AE_OS_WIN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#elif defined AE_OS_MAC
#include <wchar.h>
#endif

#include "AE_GeneralPlug.h"
#include "AE_Effect.h"
#include "A.h"
#include "AE_EffectUI.h"
#include "SPSuites.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectCBSuites.h"
#include "AEGP_SuiteHandler.h"
#include "AE_Macros.h"

extern "C" DllExport AEGP_PluginInitFuncPrototype EntryPointFunc;

#include "PythonInstance.h"

#ifdef _WIN32
#ifdef _DEBUG
#pragma comment(lib, "x64/Debug/dll/AEPython.lib")
#else
#pragma comment(lib, "x64/Release/dll/AEPython.lib")
#endif
#endif


static AEGP_Command S_python_cmd = 0;
static bool S_python_ok = false;

AEGP_PluginID S_my_id = 0;
SPBasicSuite* sP = 0;

static A_Err UpdateMenuHook(
	AEGP_GlobalRefcon		plugin_refconPV,		/* >> */
	AEGP_UpdateMenuRefcon	refconPV,				/* >> */
	AEGP_WindowType			active_window)			/* >> */
{
	A_Err 				err = A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);

	if (S_python_cmd) {
		err = suites.CommandSuite1()->AEGP_EnableCommand(S_python_cmd);
	}
	return err;
}

static A_Err CommandHook(
	AEGP_GlobalRefcon	plugin_refconPV,		/* >> */
	AEGP_CommandRefcon	refconPV,				/* >> */
	AEGP_Command		command,				/* >> */
	AEGP_HookPriority	hook_priority,			/* >> */
	A_Boolean			already_handledB,		/* >> */
	A_Boolean* handledPB)				/* << */
{
	A_Err 				err = A_Err_NONE;
	AEGP_SuiteHandler	suites(sP);

	*handledPB = FALSE;

	if (command == S_python_cmd) {
		if (S_python_ok) {
			AEPython::showWindow();
		}
		else {
			suites.UtilitySuite5()->AEGP_ReportInfo(S_my_id, "Python is unavailable: initialization failed. Check the AEPython installation.");
		}
		*handledPB = TRUE;
	}
	return err;
}

#ifdef AE_OS_WIN
static std::wstring GetPluginDir()
{
	HMODULE hModule = NULL;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&EntryPointFunc), &hModule);
	wchar_t path[_MAX_PATH] = L"";
	GetModuleFileNameW(hModule, path, _MAX_PATH);

	std::wstring strPath = path;
	return strPath.substr(0, strPath.find_last_of(L'\\'));
}

static void InitPython()
{
	const auto plugin_dir = GetPluginDir();

	std::wstring path;
	const DWORD size = GetEnvironmentVariableW(L"PATH", NULL, 0);
	if (size > 0)
	{
		path.resize(size);
		GetEnvironmentVariableW(L"PATH", &path[0], size);
		path.resize(size - 1);
	}
	path = plugin_dir + L"\\python-3.11.9-embed-amd64;" + path;
	SetEnvironmentVariableW(L"PATH", path.c_str());

	const auto dll = plugin_dir + L"\\AEPython.dll";
	if (LoadLibraryW(dll.c_str()) == NULL)
	{
		AEGP_SuiteHandler suites(sP);
		suites.UtilitySuite5()->AEGP_ReportInfo(S_my_id, "AEPython.dll could not be loaded.");
		return;
	}

	S_python_ok = AEPython::init(S_my_id, sP);
}
#else
static void InitPython()
{
	S_python_ok = AEPython::init(S_my_id, sP);
}
#endif

A_Err EntryPointFunc(
	struct SPBasicSuite* pica_basicP,			/* >> */
	A_long				 	major_versionL,			/* >> */
	A_long					minor_versionL,			/* >> */
	AEGP_PluginID			aegp_plugin_id,			/* >> */
	AEGP_GlobalRefcon* global_refconP)		/* << */
{
	A_Err 				err = A_Err_NONE;

	S_my_id = aegp_plugin_id;
	sP = pica_basicP;

	AEGP_SuiteHandler	suites(pica_basicP);

	ERR(suites.CommandSuite1()->AEGP_GetUniqueCommand(&S_python_cmd));
	ERR(suites.CommandSuite1()->AEGP_InsertMenuCommand(S_python_cmd, "Python", AEGP_Menu_WINDOW, AEGP_MENU_INSERT_SORTED));
	ERR(suites.RegisterSuite5()->AEGP_RegisterCommandHook(S_my_id, AEGP_HP_BeforeAE, AEGP_Command_ALL, CommandHook, NULL));
	ERR(suites.RegisterSuite5()->AEGP_RegisterUpdateMenuHook(S_my_id, UpdateMenuHook, NULL));

	InitPython();

	return err;
}
