# W2: C++ 层健壮性 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 C++ 层已知的崩溃路径、跨 CRT 堆风险与生命周期缺陷，错误信息可自诊断。

**Architecture:** `executeScript` 改 UTF-8 `std::string` 直通并补齐错误处理；aex↔dll 边界改
extern "C" + `char*` + 各模块释放自己的分配；AEGP DeathHook 有序退出；init 失败不崩 AE。

**Tech Stack:** C++17 / pybind11 / AE SDK（UtilitySuite5/6、MemorySuite1、RegisterSuite5）。

## Global Constraints

- 平台差异走 `AE_OS_WIN` / `AE_OS_MAC` 分支，双平台可编译（mac 实机验证由用户进行）。
- 代码注释最少化；commit 不带 Co-Authored-By；commit message 用仓库现有短句风格。
- 每个任务以 Windows Release x64 构建通过为门槛（junction 路径
  `<SDK>\Examples\AEGP\AEPython-r30\Win\AEPython.sln`，`AE_PLUGIN_BUILD_DIR=C:`）。
- 用户 API（`_AEPython.executeScript` 等 Python 侧签名与 jsx `Python.exec/eval`）行为不变。

---

### Task 1: executeScript 健壮化 + UTF-8 直通

**Files:**
- Modify: `AEPython/PythonInstance.cpp`（`executeScript` 整体替换）

**Interfaces:**
- Produces: `static std::string executeScript(std::string code)`（pybind 绑定名不变）

- [x] **Step 1: 替换实现**

```cpp
static std::string executeScript(std::string code)
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

	ERR(suites.UtilitySuite5()->AEGP_ExecuteScript(S_my_id, code.c_str(), false, &outResultPH, &outErrorStringPH));

	std::string strRes;
	std::string strErr;

	A_char* res = NULL;
	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(outResultPH, reinterpret_cast<void**>(&res)));
	if (res != NULL) strRes = res;

	A_char* error = NULL;
	ERR(suites.MemorySuite1()->AEGP_LockMemHandle(outErrorStringPH, reinterpret_cast<void**>(&error)));
	if (error != NULL) strErr = error;

	if (outResultPH) suites.MemorySuite1()->AEGP_FreeMemHandle(outResultPH);
	if (outErrorStringPH) suites.MemorySuite1()->AEGP_FreeMemHandle(outErrorStringPH);

	if (err != A_Err_NONE || strErr.empty() == false)
	{
		std::string msg = "ExecuteScriptError";
		if (err != A_Err_NONE) msg += " (A_Err " + std::to_string(err) + ")";
		if (strErr.empty() == false) msg += ": " + strErr;
		PyErr_SetString(PyExc_RuntimeError, msg.c_str());
		throw py::error_already_set();
	}

	return strRes;
}
```

要点：pybind 对 `std::string` 参数/返回值按 UTF-8 编解码，两次 `toString/toWString`
转换直接消失（双平台同一代码）；API 失败与 ES 错误统一上抛 `RuntimeError`（携带
A_Err 码与 ES 错误原文），不再有 NULL 构造 `std::string` 的崩溃路径；句柄非空才 free，
不经 ERR 短路。

- [x] **Step 2: 构建验证** — Release x64 全绿
- [x] **Step 3: Commit** — `executeScript: UTF-8 passthrough, error propagation, NULL guards`

### Task 2: aex↔dll 边界 C ABI 化 + 分配器统一

**Files:**
- Modify: `AEPython/PythonInstance.h`（API 全部改 extern "C"）
- Modify: `AEPython/PythonInstance.cpp`（内部 impl + 导出包装）
- Modify: `AEPython/ExternalObject.cpp`（调用点、`ESFreeMem`、复制函数）
- Modify: `AEPython/AEPython.cpp`（调用点）

**Interfaces:**
- Produces（PythonInstance.h）:

```cpp
#pragma once
#include <AE_GeneralPlug.h>

#ifdef _WIN32
	#define AEPY_API __declspec(dllexport)
#else
	#define AEPY_API __attribute__((visibility("default")))
#endif

extern "C" {
	AEPY_API int   AEPython_init(AEGP_PluginID my_id, SPBasicSuite* sP);
	AEPY_API int   AEPython_exec(const char* utf8_code, const char* es_stack);
	AEPY_API char* AEPython_eval(const char* utf8_code, const char* es_stack);
	AEPY_API void  AEPython_freeString(char* p);
	AEPY_API void  AEPython_del_py_object(long id);
	AEPY_API void  AEPython_showWindow();
	AEPY_API void  AEPython_shutdown();
}
```

- 所有权规则：`AEPython_eval` 返回 dll 内 `malloc` 的 UTF-8 串（失败/空返回 `nullptr`），
  调用方用 `AEPython_freeString` 归还给 dll；aex 给 ExtendScript 的 `retval->data.string`
  在 aex 内 `malloc` 复制，`ESFreeMem` 用 `free` 释放。任何分配都由分配它的模块释放，
  跨 CRT 堆问题从结构上消除。
- `AEPython_init` 返回 0/1（Task 3 使用）；`AEPython_shutdown` 在 Task 3 实现，本任务先出空实现。
- 旧的 `namespace AEPython` C++ 接口删除；`std::string` 不再跨模块。

- [x] **Step 1-4: 四个文件改造 + 构建 + Commit** — `C ABI plugin boundary, module-local allocation`

### Task 3: 生命周期 — init 保护、DeathHook、Unicode 报错

**Files:**
- Modify: `AEPython/PythonInstance.cpp`
- Modify: `AEPython/AEPython.cpp`

**Interfaces:**
- Produces: `static void reportError(const std::string& utf8_msg)`（dll 内部，Win 走
  `AEGP_ReportInfoUnicode`，mac 走 `AEGP_ReportInfo`）；`AEPython_shutdown` 实体化。

- [x] **Step 1: init 保护**

```cpp
extern "C" int AEPython_init(AEGP_PluginID my_id, SPBasicSuite* _sP)
{
	S_my_id = my_id;
	sP = _sP;
	try
	{
		init_impl();
		return 1;
	}
	catch (py::error_already_set& e)
	{
		const std::string what = e.what();
		e.discard_as_unraisable("AEPython_init");
		reportError("AEPython failed to initialize.\n" + what);
	}
	catch (const std::exception& e)
	{
		reportError(std::string("AEPython failed to initialize.\n") + e.what());
	}
	return 0;
}
```

其余导出（exec/eval/showWindow/del_py_object）入口加 `if (!interpreter) return ...;` 守卫。

- [x] **Step 2: shutdown + DeathHook**

```cpp
extern "C" void AEPython_shutdown()
{
	if (!interpreter) return;
	try
	{
		py::exec(R"(
import sys
if 'PySide6.QtWidgets' in sys.modules:
    from PySide6 import QtWidgets
    app = QtWidgets.QApplication.instance()
    if app is not None:
        app.closeAllWindows()
)");
	}
	catch (...) {}
	locals.reset();
	interpreter.reset();
}
```

AEPython.cpp：

```cpp
static A_Err DeathHook(AEGP_GlobalRefcon, AEGP_DeathRefcon)
{
	AEPython_shutdown();
	return A_Err_NONE;
}
```

`EntryPointFunc` 中 `ERR(suites.RegisterSuite5()->AEGP_RegisterDeathHook(S_my_id, DeathHook, NULL));`

- [x] **Step 3: showError 兜底改 reportError** — 原 `AEGP_ReportInfo`（ANSI）路径删除
- [x] **Step 4: 构建 + Commit** — `Guarded init, ordered shutdown via DeathHook, unicode error reports`

### Task 4: Windows API 卫生

**Files:**
- Modify: `AEPython/AEPython.cpp`（`GetPluginDir`/`InitPython` Windows 分支）
- Modify: `AEPython/PythonInstance.cpp`（`getPluginPath` Windows 分支）

- [x] **Step 1: PATH 与模块路径全部 W API**

```cpp
static std::wstring GetPluginDirW()
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
	const auto plugin_dir = GetPluginDirW();

	const DWORD n = GetEnvironmentVariableW(L"PATH", NULL, 0);
	std::wstring path(n ? n - 1 : 0, L'\0');
	if (n) GetEnvironmentVariableW(L"PATH", &path[0], n);
	SetEnvironmentVariableW(L"PATH", (plugin_dir + L"\\python-3.11.9-embed-amd64;" + path).c_str());

	LoadLibraryW((plugin_dir + L"\\AEPython.dll").c_str());

	AEPython_init(S_my_id, sP);
}
```

`getPluginPath`（dll，Windows 分支）同样改 `GetModuleHandleExW` from-address
（返回值从 aex 路径变为同目录的 dll 路径，现有调用只取 dirname，无行为差异；
python 目录名硬编码在 W6 解决，本任务不动）。

- [x] **Step 2: 构建 + Commit** — `Wide-char environment and module path handling`

## Self-Review

- 覆盖 roadmap W2 全部条目；`startUndoGroup` ANSI 保留（API 限制，roadmap 已注明）。
- mac：Task 1/2/3 代码均平台无关或已分支；`reportError` mac 走 ReportInfo(UTF-8)。
- 类型一致性：`AEPython_*` 名称在 h/cpp/两个调用文件间一致；`eval` 空串=错误的旧语义
  改为 `nullptr`=错误，空串正常返回（修掉 2.0 的语义重载）。
