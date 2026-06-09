#include "Util.h"

#include <vector>

#ifdef AE_OS_WIN

std::string toString(const std::wstring& wstr, UINT CodePage)
{
	if (wstr.empty()) return std::string();
	const int size_needed = WideCharToMultiByte(CodePage, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string dst(size_needed, 0);
	WideCharToMultiByte(CodePage, 0, &wstr[0], (int)wstr.size(), &dst[0], size_needed, NULL, NULL);
	return dst;
}

std::wstring toWString(const std::string& str, UINT CodePage)
{
	if (str.empty()) return std::wstring();
	const int size_needed = MultiByteToWideChar(CodePage, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring dst(size_needed, 0);
	MultiByteToWideChar(CodePage, 0, &str[0], (int)str.size(), &dst[0], size_needed);
	return dst;
}

#else

#include <cwchar>
#include <cstring>

std::string toString(const std::wstring& wstr, UINT)
{
	if (wstr.empty()) return std::string();
	std::mbstate_t state = std::mbstate_t();
	const wchar_t* src = wstr.c_str();
	size_t len = std::wcsrtombs(nullptr, &src, 0, &state);
	if (len == static_cast<size_t>(-1)) return "[[conversion failed]]";
	std::vector<char> buf(len + 1);
	src = wstr.c_str();
	state = std::mbstate_t();
	std::wcsrtombs(buf.data(), &src, buf.size(), &state);
	return std::string(buf.data(), len);
}

std::wstring toWString(const std::string& str, UINT)
{
	if (str.empty()) return std::wstring();
	std::mbstate_t state = std::mbstate_t();
	const char* src = str.c_str();
	size_t len = std::mbsrtowcs(nullptr, &src, 0, &state);
	if (len == static_cast<size_t>(-1)) return L"[[conversion failed]]";
	std::vector<wchar_t> buf(len + 1);
	src = str.c_str();
	state = std::mbstate_t();
	std::mbsrtowcs(buf.data(), &src, buf.size(), &state);
	return std::wstring(buf.data(), len);
}

#endif
