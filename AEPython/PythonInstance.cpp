#include "PythonInstance.h"
#include "Util.h"

#include <cstdlib>
#include <cstring>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;

std::unique_ptr<py::scoped_interpreter> interpreter;
std::unique_ptr<py::dict> locals;

static AEGP_PluginID S_my_id;
static SPBasicSuite* sP;

static void reportError(const std::string& utf8)
{
	const auto msg = toU16String(utf8);
	AEGP_SuiteHandler suites(sP);
	suites.UtilitySuite6()->AEGP_ReportInfoUnicode(S_my_id, reinterpret_cast<const A_UTF16Char*>(msg.c_str()));
}

static auto getMainHWND()
{
	A_Err err = A_Err_NONE;
	AEGP_SuiteHandler suites(sP);
	uint64 hwnd = 0;
	ERR(suites.UtilitySuite5()->AEGP_GetMainHWND(&hwnd));
	return hwnd;
}

static void raisePyError(const std::string& message)
{
	std::string escaped;
	escaped.reserve(message.size());
	for (const char c : message)
	{
		switch (c)
		{
		case '\\': escaped += "\\\\"; break;
		case '\'': escaped += "\\'"; break;
		case '\n': escaped += "\\n"; break;
		case '\r': escaped += "\\r"; break;
		default: escaped += c; break;
		}
	}
	py::exec("raise RuntimeError('" + escaped + "')");
}

static std::string lockedHandleToString(AEGP_SuiteHandler& suites, AEGP_MemHandle handle)
{
	std::string dst;
	if (handle)
	{
		A_char* p = NULL;
		if (suites.MemorySuite1()->AEGP_LockMemHandle(handle, reinterpret_cast<void**>(&p)) == A_Err_NONE && p)
		{
			dst = p;
		}
		suites.MemorySuite1()->AEGP_FreeMemHandle(handle);
	}
	return dst;
}

static std::string executeScript(std::string utf8_code)
{
	A_Err err = A_Err_NONE;
	AEGP_SuiteHandler suites(sP);

	A_Boolean outAvailablePB = false;
	AEGP_MemHandle outResultPH = 0;
	AEGP_MemHandle outErrorStringPH = 0;

	ERR(suites.UtilitySuite5()->AEGP_IsScriptingAvailable(&outAvailablePB));
	if (outAvailablePB == false) {
		py::exec("raise Exception('ScriptingNotAvailableError')");
	}

	ERR(suites.UtilitySuite5()->AEGP_ExecuteScript(S_my_id, utf8_code.c_str(), false, &outResultPH, &outErrorStringPH));

	std::string strRes = lockedHandleToString(suites, outResultPH);
	std::string strErr = lockedHandleToString(suites, outErrorStringPH);

	if (err != A_Err_NONE)
	{
		raisePyError("ExecuteScriptError:" + std::to_string(err) + ":" + strErr);
	}

	if (strErr.empty() == false)
	{
		raisePyError("ExecuteScriptError:" + strErr);
	}

	return strRes;
}

#ifdef AE_OS_WIN
static std::wstring getPluginPath()
{
	HMODULE hModule = NULL;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&getPluginPath), &hModule);
	wchar_t path[_MAX_PATH] = L"";
	GetModuleFileNameW(hModule, path, _MAX_PATH);
	return path;
}
#else
#include <dlfcn.h>
static std::wstring getPluginPath()
{
	Dl_info info;
	if (dladdr(reinterpret_cast<const void*>(&getPluginPath), &info) && info.dli_fname)
	{
		return toWString(info.dli_fname);
	}
	return std::wstring();
}
#endif

static void startUndoGroup(std::wstring wname)
{
	A_Err err = A_Err_NONE;
	AEGP_SuiteHandler suites(sP);
	auto name = toString(wname);
	ERR(suites.UtilitySuite1()->AEGP_StartUndoGroup(name.c_str()));
}

static void endUndoGroup()
{
	A_Err err = A_Err_NONE;
	AEGP_SuiteHandler suites(sP);
	ERR(suites.UtilitySuite1()->AEGP_EndUndoGroup());
}

#define PY_CLASS(m, name) py::class_<name>(m, #name)
#define PY_SUB_CLASS(m, name, base_name) py::class_<name, base_name>(m, #name)

PYBIND11_EMBEDDED_MODULE(_AEPython, m) {
	m.def("executeScript", executeScript);
	m.def("getPluginPath", getPluginPath);
	m.def("getMainHWND", getMainHWND);
	m.def("startUndoGroup", startUndoGroup);
	m.def("endUndoGroup", endUndoGroup);
}

static bool exec_impl(const std::string& utf8_code, const std::string& esStack);

bool AEPython_init(AEGP_PluginID _my_id, SPBasicSuite* _sP)
{
	S_my_id = _my_id;
	sP = _sP;

	try
	{
		interpreter = std::make_unique< py::scoped_interpreter>();
		locals = std::make_unique< py::dict>();
		py::module_::import("_AEPython").add_object("locals", *locals);

#ifdef AE_OS_WIN
		return exec_impl(u8R"(
import sys
import os
import _AEPython

sys.path.append(os.path.join(os.path.dirname(_AEPython.getPluginPath()), "Scripts"))
from AEPython import ae, qtae
)", "");
#else
		return exec_impl(u8R"(
import sys
import os
import _AEPython

sys.path.append(os.path.join(os.path.dirname(_AEPython.getPluginPath()), "..", "Resources", "Scripts"))
from AEPython import ae
)", "");
#endif
	}
	catch (const std::exception& e)
	{
		reportError("AEPython initialization failed.\n" + std::string(e.what()));
	}
	catch (...)
	{
		reportError("AEPython initialization failed.");
	}
	return false;
}

void showError(py::error_already_set& e, const std::string& esStack) {
	try
	{
		auto msg = esStack + e.what();
		py::print(msg.c_str(), py::arg("file") = py::module_::import("sys").attr("stderr"));
	}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4101)
#endif
	catch (py::error_already_set& _)
	{
		reportError("Python error.\n" + std::string(e.what()));
	}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

static bool exec_impl(const std::string& utf8_code, const std::string& esStack)
{
	try
	{
		py::exec(utf8_code, py::globals(), *locals);
		return true;
	}
	catch (py::error_already_set& e)
	{
		showError(e, esStack);
		return false;
	}
	catch (...)
	{
		return false;
	}
}

static std::string eval_impl(const std::string& utf8_code, const std::string& esStack)
{
	try
	{
		return py::module_::import("AEPython.ae").attr("_eval")(utf8_code).cast<std::string>();
	}
	catch (py::error_already_set& e)
	{
		showError(e, esStack);
		return "";
	}
	catch (...)
	{
		return "";
	}
}

bool AEPython_exec(const char* utf8_code, const char* es_stack)
{
	if (!interpreter) return false;
	return exec_impl(utf8_code ? utf8_code : "", es_stack ? es_stack : "");
}

char* AEPython_eval(const char* utf8_code, const char* es_stack)
{
	if (!interpreter) return nullptr;
	const std::string ret = eval_impl(utf8_code ? utf8_code : "", es_stack ? es_stack : "");
	if (ret.empty()) return nullptr;
	char* dst = static_cast<char*>(std::malloc(ret.size() + 1));
	if (dst) std::memcpy(dst, ret.c_str(), ret.size() + 1);
	return dst;
}

void AEPython_free(char* p)
{
	std::free(p);
}

void AEPython_del_py_object(long id)
{
	if (!interpreter) return;
	try
	{
		py::module_::import("AEPython.ae").attr("_del_py_object")(id);
	}
	catch (py::error_already_set& e)
	{
		auto esStack = "<SoObjectInterface.finalize at PyObjects[" + std::to_string(id) + "]>\n";
		showError(e, esStack);
	}
	catch (...)
	{
	}
}

void AEPython_showWindow(void)
{
	if (!interpreter) return;
	exec_impl(u8R"(
from AEPython import qtae
qtae.ShowPythonWindow()
)", "");
}

void AEPython_shutdown(void)
{
	if (!interpreter) return;
	try
	{
		locals.reset();
		interpreter.reset();
	}
	catch (...)
	{
	}
}
