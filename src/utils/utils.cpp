#include "../pch.h"

std::string Utils::UnicodeAscii(std::wstring_view wszUnicode)
{
	const int nLength = WideCharToMultiByte(CP_UTF8, 0UL, wszUnicode.data(), wszUnicode.length(), nullptr, 0, nullptr, nullptr);
	std::string szOutput = { };

	if (nLength > 0)
	{
		szOutput.resize(nLength);
		WideCharToMultiByte(CP_UTF8, 0UL, wszUnicode.data(), wszUnicode.length(), &szOutput[0], nLength, nullptr, nullptr);
	}

	return szOutput;
}

std::wstring Utils::AsciiUnicode(std::string_view szAscii)
{
	const int nLength = MultiByteToWideChar(CP_UTF8, 0UL, szAscii.data(), szAscii.length(), nullptr, 0);
	std::wstring wszOutput = { };

	if (nLength > 0)
	{
		wszOutput.resize(nLength);
		MultiByteToWideChar(CP_UTF8, 0UL, szAscii.data(), szAscii.length(), &wszOutput[0], nLength);
	}

	return wszOutput;
}