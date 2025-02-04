#pragma once
#include "../pch.h"

#pragma region memory_pattern_convert
#define INRANGE( x, min, max ) (x >= min && x <= max) 
#define GETBITS( x ) ( INRANGE( ( x&( ~0x20 ) ), 'A', 'F' ) ? ( ( x&( ~0x20 ) ) - 'A' + 0xA) : ( INRANGE( x, '0', '9' ) ? x - '0' : 0 ) )
#define GETBYTE( x ) ( GETBITS( x[0] ) << 4 | GETBITS( x[1] ) )
#pragma endregion

/*
 * singleton implementation
 * restricts the instantiation of a class to one single class instance
 */
template <typename T>
class CSingleton
{
protected:
	CSingleton() { }
	~CSingleton() { }

	CSingleton(const CSingleton&) = delete;
	CSingleton& operator=(const CSingleton&) = delete;

	CSingleton(CSingleton&&) = delete;
	CSingleton& operator=(CSingleton&&) = delete;
public:
	static T& Get()
	{
		static T pInstance{ };
		return pInstance;
	}
};

/*
 * MEMORY
 * memory management functions
 */
namespace Memory
{
	/* checks is we have given section in given address */
	bool			GetSectionInfo(std::uintptr_t uBaseAddress, const std::string& szSectionName, std::uintptr_t& uSectionStart, std::uintptr_t& uSectionSize);

	// Check
	/* can we read/readwrite given memory region */
	bool			IsValidReadPtr(void* uAddress);
	bool			IsValidWritePtr(void* uAddress);

	/* returns vector filled with given value */
	template <typename T, std::size_t S>
	std::vector<T> GetFilledVector(const T& fill)
	{
		std::vector<T> vecTemp(S);
		std::fill(vecTemp.begin(), vecTemp.begin() + S, fill);
		return vecTemp;
	}
	template <typename T = void*>
	constexpr T GetVFunc(void* thisptr, std::size_t nIndex)
	{
		return (*static_cast<T**>(thisptr))[nIndex];
	}

	/*
	 * virtual function call implementation
	 * calls function of specified class at given index
	 * @note: doesnt adding references automatic and needs to add it manually!
	 */
	template <typename T, typename ... args_t>
	constexpr T CallVFunc(void* thisptr, std::size_t nIndex, args_t... argList)
	{
		using VirtualFn = T(__thiscall*)(void*, decltype(argList)...);
		return (*static_cast<VirtualFn**>(thisptr))[nIndex](thisptr, argList...);
	}
}
