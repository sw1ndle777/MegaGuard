#include "../pch.h"
bool Memory::GetSectionInfo(std::uintptr_t uBaseAddress, const std::string& szSectionName, std::uintptr_t& uSectionStart, std::uintptr_t& uSectionSize)
{
	const auto pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(uBaseAddress);
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return false;

	const auto pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS32>(uBaseAddress + pDosHeader->e_lfanew);
	if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE)
		return false;

	IMAGE_SECTION_HEADER* pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
	std::uint16_t uNumberOfSections = pNtHeaders->FileHeader.NumberOfSections;

	while (uNumberOfSections > 0U)
	{
		// if we're at the right section
		if (!strcmp(szSectionName.c_str(), reinterpret_cast<const char*>(pSectionHeader->Name)))
		{
			uSectionStart = uBaseAddress + pSectionHeader->VirtualAddress;
			uSectionSize = pSectionHeader->SizeOfRawData;
			return true;
		}

		pSectionHeader++;
		uNumberOfSections--;
	}

	return false;
}


bool Memory::IsValidReadPtr(void* uAddress)
{
	if (uAddress == 0U)
		return false;

	MEMORY_BASIC_INFORMATION memInfo = { };

	if (VirtualQuery(reinterpret_cast<LPCVOID>(uAddress), &memInfo, sizeof(memInfo)) == 0U)
		return false;

	if (memInfo.State == MEM_COMMIT && !(memInfo.Protect & PAGE_NOACCESS))
		return true;

	return true;
}
#define MEM_WRITE (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)
bool Memory::IsValidWritePtr(void* uAddress)
{
	if (uAddress == 0U)
		return false;

	MEMORY_BASIC_INFORMATION memInfo = { };

	if (VirtualQuery(reinterpret_cast<LPCVOID>(uAddress), &memInfo, sizeof(memInfo)) == 0U)
		return false;

	if (memInfo.State == MEM_COMMIT && (memInfo.Protect & MEM_WRITE))
		return true;

	return true;
}

