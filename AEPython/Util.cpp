#include "Util.h"

std::u16string toU16String(const std::string& str)
{
	std::u16string dst;
	dst.reserve(str.size());
	const size_t n = str.size();
	size_t i = 0;
	while (i < n)
	{
		const unsigned char b0 = static_cast<unsigned char>(str[i]);
		char32_t c = 0xFFFD;
		size_t len = 1;
		if (b0 < 0x80)
		{
			c = b0;
		}
		else if ((b0 >> 5) == 0x6 && i + 1 < n)
		{
			c = ((b0 & 0x1F) << 6) | (str[i + 1] & 0x3F);
			len = 2;
		}
		else if ((b0 >> 4) == 0xE && i + 2 < n)
		{
			c = ((b0 & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
			len = 3;
		}
		else if ((b0 >> 3) == 0x1E && i + 3 < n)
		{
			c = ((b0 & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
			len = 4;
		}
		if (c < 0x10000)
		{
			dst += static_cast<char16_t>(c);
		}
		else
		{
			const char32_t v = c - 0x10000;
			dst += static_cast<char16_t>(0xD800 | (v >> 10));
			dst += static_cast<char16_t>(0xDC00 | (v & 0x3FF));
		}
		i += len;
	}
	return dst;
}

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

std::string toString(const std::wstring& wstr, UINT)
{
	std::string dst;
	dst.reserve(wstr.size() * 4);
	for (const wchar_t wc : wstr)
	{
		const char32_t c = static_cast<char32_t>(wc);
		if (c < 0x80)
		{
			dst += static_cast<char>(c);
		}
		else if (c < 0x800)
		{
			dst += static_cast<char>(0xC0 | (c >> 6));
			dst += static_cast<char>(0x80 | (c & 0x3F));
		}
		else if (c < 0x10000)
		{
			dst += static_cast<char>(0xE0 | (c >> 12));
			dst += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			dst += static_cast<char>(0x80 | (c & 0x3F));
		}
		else
		{
			dst += static_cast<char>(0xF0 | (c >> 18));
			dst += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
			dst += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
			dst += static_cast<char>(0x80 | (c & 0x3F));
		}
	}
	return dst;
}

std::wstring toWString(const std::string& str, UINT)
{
	std::wstring dst;
	dst.reserve(str.size());
	const size_t n = str.size();
	size_t i = 0;
	while (i < n)
	{
		const unsigned char b0 = static_cast<unsigned char>(str[i]);
		char32_t c = 0xFFFD;
		size_t len = 1;
		if (b0 < 0x80)
		{
			c = b0;
		}
		else if ((b0 >> 5) == 0x6 && i + 1 < n)
		{
			c = ((b0 & 0x1F) << 6) | (str[i + 1] & 0x3F);
			len = 2;
		}
		else if ((b0 >> 4) == 0xE && i + 2 < n)
		{
			c = ((b0 & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
			len = 3;
		}
		else if ((b0 >> 3) == 0x1E && i + 3 < n)
		{
			c = ((b0 & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
			len = 4;
		}
		dst += static_cast<wchar_t>(c);
		i += len;
	}
	return dst;
}

#endif
