#pragma once

#include "../pch.h"

namespace Utils
{
	/* converts from unicode to ascii string */
	std::string UnicodeAscii(std::wstring_view wszUnicode);
	/* converts from ascii to unicode string */
	std::wstring AsciiUnicode(std::string_view szAscii);
}
