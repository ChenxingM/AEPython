# W2 C++ 层健壮性 实施计划

> **For agentic workers:** 本计划由主会话内联执行（executing-plans 模式）。
> 每个任务：修改 → Windows 构建验证 → 提交。AE 内行为验证由用户在部署后按
> "AE 侧验证清单"执行；mac 编译验证由用户在 mac 机上跑 `Mac/build.sh`。

**Goal:** 消除 C++ 层已知的崩溃路径、错误信息丢失、编码遗留与跨 CRT 堆风险，全部行为兼容。

**Architecture:** 不改插件双模块结构（aex + dll）；把 dll 导出面换成 C ABI；
字符串主干道统一 UTF-8 `std::string`；错误经 `ReportInfoUnicode`/Python 异常透传。

**构建验证命令**（每任务执行）：
vcvars64 下
`msbuild <SDK>\Examples\AEGP\AEPython-r30\Win\AEPython.sln /p:Configuration=Release /p:Platform=x64`
期望：0 error。

## 全局约束

- 注释最少化；commit 不带 Co-Authored-By；消息用仓库既有短句风格。
- 新增/修改代码必须双平台可编译（Windows 实测，mac 保持 `#ifdef` 边界正确）。
- 不改 Python/jsx 侧公开行为（W1 才动协议）。

---

### Task 1: executeScript 重写（NULL 防护 + err/strErr 上抛 + UTF-8 直通）

**Files:** Modify `AEPython/PythonInstance.cpp`

- 签名 `std::wstring executeScript(std::wstring)` → `std::string executeScript(std::string utf8_code)`；
  pybind 侧自动按 UTF-8 编解码，`AEGP_ExecuteScript` 直接吃 `utf8_code.c_str()`，
  删除 `toString/toWString` 往返（双平台共用同一实现，Util 转换仅剩 `startUndoGroup` 使用）。
- Lock 后指针判空再构造 string；`FreeMemHandle` 不包 `ERR`、句柄非空才调。
- `err != A_Err_NONE` → `py::exec` 抛 `RuntimeError("ExecuteScriptError:<err>:<strErr>")`，
  strErr 内容做 Python 字符串安全转义（repr 语义：仅出现在异常消息，简单替换 `\` `'` 换行）。
- `strErr` 非空（err==0）同样带原文上抛（替换现在的裸 `Exception('ExecuteScriptError')`）。

**验证:** 构建绿；AE 侧清单#1。
**Commit:** `Hardened executeScript: NULL guards, error text passthrough, UTF-8 direct`

### Task 2: 错误对话框 Unicode 化

**Files:** Modify `AEPython/Util.h` `AEPython/Util.cpp` `AEPython/PythonInstance.cpp`

- Util 新增 `std::u16string toU16String(const std::string& utf8)`（手写 UTF-8→UTF-16，双平台同一实现）。
- `showError` 兜底改 `suites.UtilitySuite6()->AEGP_ReportInfoUnicode(S_my_id, (const A_UTF16Char*)u16.c_str())`。

**验证:** 构建绿；AE 侧清单#2。
**Commit:** `Report errors via ReportInfoUnicode`

### Task 3: init 全程防护

**Files:** Modify `AEPython/PythonInstance.h` `AEPython/PythonInstance.cpp` `AEPython/AEPython.cpp`

- `AEPython::init` 返回 `bool`；内部 try/catch（`std::exception` + `...`），
  失败经 ReportInfoUnicode 报告并返回 false，异常不越 DLL 边界。
- `AEPython.cpp` 持 `S_python_ok` 标志；init 失败时 CommandHook 弹提示不再调 showWindow。

**验证:** 构建绿；AE 侧清单#3（人为破坏 python 目录测试）。
**Commit:** `Guarded Python init failure instead of crashing AE startup`

### Task 4: InitPython 宽字符化 + 模块句柄去硬编码

**Files:** Modify `AEPython/AEPython.cpp` `AEPython/PythonInstance.cpp`

- `GetPluginDir` → `std::wstring`：`GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | UNCHANGED_REFCOUNT, (LPCWSTR)&EntryPointFunc, ...)` + `GetModuleFileNameW`。
- PATH 拼接 → `GetEnvironmentVariableW/SetEnvironmentVariableW`；`LoadLibraryW`。
- `PythonInstance.cpp::getPluginPath` 同法去掉 `"AEPython.aex"` 字面量（取本 dll 路径的相邻 aex？
  不需要——直接对 dll 自身地址取模块路径，Scripts 就在同目录）。注意 mac 分支已等价（dladdr）。

**验证:** 构建绿；AE 侧清单#4。
**Commit:** `Wide-char environment handling and module path lookup`

### Task 5: ES 内存分配器统一

**Files:** Modify `AEPython/ExternalObject.cpp`

- `stringToCharP` 用 `malloc`；`ESFreeMem` 用 `free`（修 `delete` 释放 `new[]` 的未定义行为）。

**验证:** 构建绿。
**Commit:** `Unified ES memory allocation on malloc/free`

### Task 6: DLL 导出面改 C ABI

**Files:** Modify `AEPython/PythonInstance.h` `AEPython/PythonInstance.cpp` `AEPython/ExternalObject.cpp` `AEPython/AEPython.cpp`

- 导出改为：
  ```c
  extern "C" {
      AEPY_API bool  AEPython_init(AEGP_PluginID id, SPBasicSuite* sP);
      AEPY_API bool  AEPython_exec(const char* utf8_code, const char* es_stack);
      AEPY_API char* AEPython_eval(const char* utf8_code, const char* es_stack);
      AEPY_API void  AEPython_free(char* p);
      AEPY_API void  AEPython_del_py_object(long id);
      AEPY_API void  AEPython_showWindow();
      AEPY_API void  AEPython_shutdown(void);
  }
  ```
- `AEPython_eval` 返回 dll 内 `malloc` 的 UTF-8 缓冲（错误返回 NULL），调用方用完必须 `AEPython_free`；
  aex 侧 `_eval` 把结果复制进 aex 自己的 `malloc` 缓冲交给 ES（ES 经 aex 的 `ESFreeMem` 释放，
  两个模块各释放各的，跨 CRT 堆问题从结构上消失）。
- 旧 `namespace AEPython` C++ 导出删除；`std::string` 不再跨模块。

**验证:** 构建绿（隐含验证 aex 链接新导出）；AE 侧清单#5（全功能回归）。
**Commit:** `C ABI across the aex/dll boundary`

### Task 7: 退出钩子

**Files:** Modify `AEPython/AEPython.cpp` `AEPython/PythonInstance.cpp`

- `EntryPointFunc` 注册 `AEGP_RegisterDeathHook` → `AEPython_shutdown()`：
  try/catch 内 `locals.reset(); interpreter.reset();`，幂等（重复调用无害）。
  Qt 的有序退出属 W4，此处仅保证解释器不再死在 DLL_PROCESS_DETACH。

**验证:** 构建绿；AE 侧清单#6（开关 AE 数次不崩不挂）。
**Commit:** `Finalize Python interpreter via AEGP death hook`

---

## AE 侧验证清单（用户部署后执行）

1. `ae.alert("你好・テスト")` 正常；`ae.executeScript('missing_func()')` 报错信息含 ES 原文。
2. 临时改名 `Scripts\Startup\AEPython.jsx` 后调用 → 弹出含 `__AEPython_executeScript is not defined` 的错误而非崩溃/静默。
3. 临时改名 `python-3.11.9-embed-amd64` 目录 → AE 正常启动，Python 菜单弹"初始化失败"提示。
4. PATH 含非 ASCII 条目的机器上加载正常（现有日文环境即是）。
5. `tests/test_AEPython.py` 全绿 + `Python.exec/eval` from ExtendScript 回归。
6. 反复开关 AE 5 次，退出无崩溃/挂起（事件查看器无 AfterFX 错误）。

## mac 侧验证（用户在 mac 机执行）

- `Mac/build.sh` 编译通过；AE 2026 中 `Python.exec("ae.alert('你好')")` 显示正常。
