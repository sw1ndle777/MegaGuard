#include "pch.h"

MegaGuardIPCClient* g_ipcClient = nullptr; 

#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <cstring>
#include <chrono>
//#include <format>
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
#endif
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
//#include <stacktrace> // C++23 stacktrace header
//#include <StackWalker.h>

#include <ranges>

std::string dumpFile;

static std::atomic<bool> g_isFinal = false;
static std::atomic<bool> g_isFinalStep = false;
static std::atomic<bool> g_isClosing = false;
static _EXCEPTION_POINTERS* g_p{};
constexpr size_t g_crashStackSize = 1024 * 1024 * 1;
static char* g_crashStack{};
DWORD g_crashThread{};
DWORD g_traceThread{};

static std::string stackwalker_backtrace_json;
static std::string header_message;
static int crash_signal = 0; // 0 is not a valid signal id
static std::mutex mut;
static std::condition_variable cv;
static std::thread output_thread;

// Simple JSON formatting helper - no simdjson needed
static std::string format_crash_json(
	uint64_t code,
	uint64_t address,
	const std::string& stacktrace,
	DWORD ebp, DWORD eax, DWORD ecx, DWORD edx, DWORD ebx, DWORD esi, DWORD edi)
{
	std::ostringstream json;
	json << "{\n";
	json << "    \"code\": " << code << ",\n";
	json << "    \"address\": " << address << ",\n";
	json << "    \"stacktrace_ida\": " << stacktrace << ",\n";
	json << "    \"context\": {\n";
	json << "        \"ebp\": " << ebp << ",\n";
	json << "        \"eax\": " << eax << ",\n";
	json << "        \"ecx\": " << ecx << ",\n";
	json << "        \"edx\": " << edx << ",\n";
	json << "        \"ebx\": " << ebx << ",\n";
	json << "        \"esi\": " << esi << ",\n";
	json << "        \"edi\": " << edi << "\n";
	json << "    }\n";
	json << "}\n";
	return json.str();
}

std::string GetCurrentTimestamp()
{
	auto now = std::chrono::system_clock::now();
	auto timeT = std::chrono::system_clock::to_time_t(now);
	std::tm localTime;

	localtime_s(&localTime, &timeT);

	std::ostringstream oss;
	oss << std::put_time(&localTime, "%Y%m%d_%H%M%S");  // Format: YYYYMMDD_HHMMSS
	return oss.str();
}

#include <ctime>
#include <sstream>

std::string GetDirectory(const std::string& filePath)
{
	size_t pos = filePath.find_last_of("/\\");
	return (pos != std::string::npos) ? filePath.substr(0, pos) : "";
}

std::string GetFileNameWithoutExtension(const std::string& filePath)
{
	size_t start = filePath.find_last_of("/\\");
	size_t end = filePath.find_last_of(".");
	if (start == std::string::npos)
		start = 0;
	else
		start += 1;
	return (end != std::string::npos && end > start) ? filePath.substr(start, end - start) : filePath.substr(start);
}

std::string GetFileExtension(const std::string& filePath)
{
	size_t pos = filePath.find_last_of(".");
	return (pos != std::string::npos) ? filePath.substr(pos) : "";
}

std::string CombinePath(const std::string& directory, const std::string& fileName)
{
	if (directory.empty())
		return fileName;
	char separator = '\\';
	return directory + separator + fileName;
}

std::string GenerateCrashLogFilePath(const std::string& baseFileName)
{
	std::string directory = GetDirectory(baseFileName);
	std::string timestamp = GetCurrentTimestamp();
	std::string newFileName = GetFileNameWithoutExtension(baseFileName) + "_" + timestamp + "_crash.log";

	return CombinePath(directory, newFileName);
}
std::string GenerateStacktraceJsonFilePath(const std::string& baseFileName)
{
	std::string directory = GetDirectory(baseFileName);
	std::string timestamp = GetCurrentTimestamp();
	std::string newFileName = GetFileNameWithoutExtension(baseFileName) + "_" + timestamp + "_crash_stacktrace.json";

	return CombinePath(directory, newFileName);
}
std::string GenerateDumpFilePath(const std::string& baseFileName)
{
	std::string directory = GetDirectory(baseFileName);
	std::string timestamp = GetCurrentTimestamp();
	std::string newFileName = GetFileNameWithoutExtension(baseFileName) + "_" + timestamp + GetFileExtension(baseFileName);

	return CombinePath(directory, newFileName);
}

static const char* try_get_signal_name(int signal) {
	switch (signal)
	{
	case SIGTERM:
		return "SIGTERM";
	case SIGSEGV:
		return "SIGSEGV";
	case SIGINT:
		return "SIGINT";
	case SIGILL:
		return "SIGILL";
	case SIGABRT:
		return "SIGABRT";
	case SIGFPE:
		return "SIGFPE";
	}
	return "";
}

static void output_crash_log()
{
	auto path = GenerateCrashLogFilePath(dumpFile);
	auto json_path = GenerateStacktraceJsonFilePath(dumpFile);
	std::ofstream log(path);
	if (!header_message.empty()) log << header_message << std::endl;
	if (crash_signal != 0)
	{
		log << "Received signal " << crash_signal << " " << try_get_signal_name(crash_signal) << std::endl;
	}

	log.close();
	std::ofstream stacktraceLog(json_path);
	stacktraceLog << stackwalker_backtrace_json;
	stacktraceLog.close();
}

struct CallstackEntryJson
{
	std::uint32_t frame;
	std::uint64_t offset;
	std::string moduleName;
	std::optional<std::uint64_t> base;
};

/*
class XStackWalker : public StackWalker
{
public:
	XStackWalker() : StackWalker() {}

	std::ostringstream backtrace; // JSON will be built as a string
	std::uint32_t frame{};
	std::vector<std::uint32_t> printed_bases;

	auto has_base(const std::uintptr_t base) {
		return std::find(printed_bases.begin(), printed_bases.end(), base) != printed_bases.end();
	}

protected:
	virtual void OnOutput(LPCSTR) override {}

	virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry) override
	{
		if (frame >= 10) return;

		std::string moduleName = entry.moduleName;
		std::uintptr_t offset = entry.offset;
		std::uintptr_t base = entry.baseOfImage;

		if (frame == 0)
		{
			backtrace << "[\n";
		}
		else
		{
			backtrace << ",\n";
		}

		backtrace << "  {\n";
		backtrace << "    \"frame\": " << frame++ << ",\n";
		backtrace << "    \"offset\": " << offset << ",\n";
		backtrace << "    \"moduleName\": \"" << moduleName << "\"";

		if (!has_base(base))
		{
			backtrace << ",\n    \"base\": " << base;
			printed_bases.emplace_back(base);
		}

		backtrace << "\n  }";
	}

public:
	std::string get_backtrace() {
		if (frame > 0)
		{
			backtrace << "\n]";
		}
		return backtrace.str();
	}
};

static void save_to_file(const std::string& fileName, const std::string& data)
{
	HANDLE hFile = CreateFileA(fileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD bytesWritten = 0;
		WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr);
		CloseHandle(hFile);
	}
	else
	{
		DWORD errorCode = GetLastError();
		std::cerr << "Failed to open file: " << fileName << ", Error Code: " << errorCode << "\n";
	}
}

void NotifyAndTerminate(const wchar_t* message, const wchar_t* title)
{
	//if (!g_isClosing.exchange(true))
		MessageBoxW(0, message, title, MB_SYSTEMMODAL | MB_ICONERROR);
}

[[gnu::noinline, clang::optnone]] void on_exception_final()
{
	const auto threadId = GetCurrentThreadId();
	const auto* p = g_p;
	const auto thread = GetCurrentThread();

	ClipCursor(NULL);
	ShowCursor(1);

	XStackWalker sw;
	sw.ShowCallstack(thread, p->ContextRecord);

	std::string backtrace;
	g_traceThread = 1;
	std::thread([&]
		{
			g_traceThread = GetCurrentThreadId();
			const auto* rc = p->ContextRecord;

			// Format JSON directly without simdjson
			std::string jsonOutput = format_crash_json(
				static_cast<uint64_t>(p->ExceptionRecord->ExceptionCode),
				reinterpret_cast<uint64_t>(p->ExceptionRecord->ExceptionAddress),
				sw.get_backtrace(),
				rc->Ebp, rc->Eax, rc->Ecx, rc->Edx, rc->Ebx, rc->Esi, rc->Edi
			);

			if (static bool saved = false; !saved)
			{
				const auto dmpFile = GenerateStacktraceJsonFilePath(dumpFile);
				MegaGuard::EventLog->Debug(nostd::source_location::current(), "Stacktrace: %s", jsonOutput.c_str());
				save_to_file(dmpFile, jsonOutput);

				std::string dumpFilePathWithTimestamp = GenerateDumpFilePath(dumpFile);

				HANDLE hFile = CreateFileA(dumpFilePathWithTimestamp.c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

				if (hFile != INVALID_HANDLE_VALUE)
				{
					MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
					dumpInfo.ExceptionPointers = g_p;
					dumpInfo.ThreadId = GetCurrentThreadId();
					dumpInfo.ClientPointers = TRUE;

					MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);
					MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType, &dumpInfo, nullptr, nullptr);
					CloseHandle(hFile);
				}

				saved = true;
			}
			Sleep(1000);

			g_traceThread = 0;
		}).detach();

	while (g_traceThread) Sleep(1);
	NotifyAndTerminate(L"Encountered an error, exiting now.", L"[MVO] Crashed #1");
}

[[gnu::noinline, clang::optnone]] LONG __stdcall on_exception(_EXCEPTION_POINTERS* _p)
{
	g_p = _p;

	if (!g_isFinal)
	{
		switch (auto const* record = g_p->ExceptionRecord; record->ExceptionCode)
		{
		case EXCEPTION_PRIV_INSTRUCTION:
		case EXCEPTION_ACCESS_VIOLATION:
		case EXCEPTION_DATATYPE_MISALIGNMENT:
		case EXCEPTION_BREAKPOINT:
		case EXCEPTION_SINGLE_STEP:
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		case EXCEPTION_FLT_DENORMAL_OPERAND:
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		case EXCEPTION_FLT_INEXACT_RESULT:
		case EXCEPTION_FLT_INVALID_OPERATION:
		case EXCEPTION_FLT_OVERFLOW:
		case EXCEPTION_FLT_STACK_CHECK:
		case EXCEPTION_FLT_UNDERFLOW:
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
		case EXCEPTION_INT_OVERFLOW:
		case EXCEPTION_IN_PAGE_ERROR:
		case EXCEPTION_ILLEGAL_INSTRUCTION:
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		case EXCEPTION_STACK_OVERFLOW:
		case EXCEPTION_INVALID_DISPOSITION:
		case EXCEPTION_GUARD_PAGE:
		case EXCEPTION_INVALID_HANDLE:
			g_isFinal = true;
			break;

		default:
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	auto threadId = GetCurrentThreadId();

	if (g_isFinalStep.exchange(true))
	{
		if (g_traceThread == threadId)
		{
			g_traceThread = {};
		}

		if (g_crashThread == threadId)
		{
			std::fprintf(stderr, "DOUBLE FAULT!\r\n\r\n");
			while (true) {
				Sleep(100);
			}
			NotifyAndTerminate(L"Encountered a fatal error, exiting now.", L"[MVO] Crashed #2");
		}

		SuspendThread(GetCurrentThread());
	}
	g_crashThread = threadId;

	auto targetAddr = &g_crashStack[sizeof(g_crashStack)];
	__asm {
		mov esp, targetAddr;
		jmp on_exception_final;
	}
}

[[gnu::noinline, clang::optnone]] static LONG __stdcall on_final_exception(_EXCEPTION_POINTERS* p) {
	g_isFinal = true;
	on_exception(p);
	NotifyAndTerminate(L"Encountered a fatal error, exiting now.", L"[MVO] Crashed #3");
	return 0;
}

void InitializeExceptionHandlers() {
	g_crashStack = new char[g_crashStackSize];
	AddVectoredExceptionHandler(TRUE, on_exception);
	SetUnhandledExceptionFilter(on_final_exception);
}

#endif
*/

static void(__fastcall* ogProgFill)(std::uint32_t pInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t a3) = nullptr;
static void __fastcall hkProgFill(std::uint32_t pInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t a3) {
	//MegaGuard::EventLog->Debug(std::source_location::current(), "Was call Progress filler with info a2: %d a3: %d", a2, a3);
	//std::cout << "Was call Progress filler with info " << a2 << ' ' << a3 << '\n';
	ogProgFill(pInstance, edx, a2, a3);
}

std::uint32_t CalculateValueForPercentage(int x) {
	if (x <= 0 || x > 100) {
		//throw std::invalid_argument("Percentage must be greater than 0 and less than or equal to 100.");
	}
	if (x > 50) {
		return (100 - x + 50);
	}
	// Calculate the value based on the percentage
	std::uint32_t value = static_cast<std::uint32_t>(50 * std::pow(2, std::ceil(std::log2(100.0 / x))));
	return value;
}
std::uint32_t CalculatePercentage(int a, int b) {
	if (b == 0) {
		if (a == 0) return 50;
		return 100;
	}
	if (a < 0 || b < 0 || (a == 0 && b == 0)) {
		return 0; // Handle invalid inputs or divide by zero
	}

	double total = static_cast<double>(a + b);
	double percentage = (100.0 * static_cast<double>(a)) / total;
	std::uint32_t res = static_cast<std::uint32_t>(std::round(percentage));

	return (res > 100 ? 100 : res); // Ensure the result is between 0 and 100
}
static void(__fastcall* ogInfoHandler)(std::uint32_t infoInstance) = nullptr;
static void __fastcall hkInfoHandler(std::uint32_t infoInstance) {
	ogInfoHandler(infoInstance);

	//std::uint32_t c_instance = _call<std::uint32_t(__cdecl*)()>(0x4728D0);
	//std::uint32_t v220 = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(c_instance, 268), 148), 4);
	//std::cout << v220 << '\n';

	/*std::uint32_t base_prog = 0;
	std::cout << "Enter base progbar: ";
	std::cin >> base_prog;
	std::uint32_t curr_prog = 0;
	std::cout << "Enter value progbar: ";
	std::cin >> curr_prog;
	*/
	std::uint32_t c_instance = _call<std::uint32_t(__cdecl*)()>(0x4728D0);
	//FOR KILLDEATH
	//std::uint32_t kill = _rv<std::uint32_t>(_call<std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t)>(0x911590, c_instance, 0), 268);
	std::uint32_t stat_base = _call<std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t)>(0x911590, c_instance, 0);
	if (stat_base) {
		std::uint32_t death = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(stat_base, 268), 4), 3036);
		std::uint32_t kill = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(stat_base, 268), 4), 3032);
		//std::cout << "Kill/Death: " << kill << '/' << death << ' ' << CalculatePercentage(kill, death) << '\n';
		std::uint32_t v258 = 110210;
		auto v31 = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(0x8B4600, (infoInstance + 1712), &v258);
		std::uint32_t cprog1 = 50;
		_call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t, std::uint32_t) >(0xE84A30, _rv<std::uint32_t>(v31, 0), 0, CalculateValueForPercentage(CalculatePercentage(kill, death)));
		//FOR WINLOSE
		std::uint32_t win = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(stat_base, 268), 4), 3044);
		std::uint32_t lose = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(stat_base, 268), 4), 3048);
		//std::cout << "Win/Lose: " << win << '/' << lose << ' ' << CalculatePercentage(win, lose) << '\n';
		std::uint32_t v258_t = 110211;
		auto v31_t = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(0x8B4600, (infoInstance + 1712), &v258_t);
		std::uint32_t cprog2 = 50;
		_call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t, std::uint32_t) >(0xE84A30, _rv<std::uint32_t>(v31_t, 0), 0, CalculateValueForPercentage(CalculatePercentage(win, lose)));

		std::uint32_t zombi_base = _call<std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t)>(0x911590, c_instance, 0);
		std::uint32_t zombi_kill = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(zombi_base, 268), 4), 3136);
		std::uint32_t zombi_infect = _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(zombi_base, 268), 4), 3140);
		std::uint32_t zombi_record = (zombi_kill + zombi_infect);
		std::uint32_t zombi_record_valdlg = 110227;

		auto zombi_record_instance = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(0x8B4600, (infoInstance + 1712), &zombi_record_valdlg);
		char* z_count = new char[9];
		sprintf_s(z_count, sizeof(z_count), "%d", zombi_record);
		//MegaGuard::EventLog->Debug(std::source_location::current(), "Zombi kill: %d Zombi infected: %d", zombi_kill, zombi_infect);
		//std::cout << "Zombi kill: " << zombi_kill << " Zombi infected: " << zombi_infect << '\n';
		//std::cout << z_count << '\n';
		//std::cout << "Should call: " << _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(zombi_record_instance, 0), 0), 216) << '\n';
		_call < std::uint32_t(__thiscall*)(std::uint32_t, char*) >(MegaGuard::Addresses::Dialog::WriteStatic, _rv<std::uint32_t>(zombi_record_instance, 0), z_count);
		//Memory::CallVFunc<void>(_rv<std::uint32_t*>(zombi_record_instance, 0), 216, z_count);
		//_call < std::uint32_t(__thiscall*)(std::uint32_t, char *) >((_rv<std::uint32_t>(zombi_record_instance, 0)+216), _rv<std::uint32_t>(zombi_record_instance, 0), z_count);
	}
};

static void(__fastcall* ogInfoOtherHandler)(std::uint32_t infoInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t a3) = nullptr;
static void __fastcall hkInfoOtherHandler(std::uint32_t infoInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t a3) {
	ogInfoOtherHandler(infoInstance, edx, a2, a3);
	//std::cout << "OTHER Info Handler: " << a2 << '\n';
	std::uint32_t kill = _rv<std::uint32_t>(a2, 8);
	std::uint32_t death = _rv<std::uint32_t>(a2, 12);
	std::uint32_t v258 = 110210;
	auto v31 = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(0x8B4600, (infoInstance + 1712), &v258);
	std::uint32_t cprog1 = 50;
	_call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t, std::uint32_t) >(0xE84A30, _rv<std::uint32_t>(v31, 0), 0, CalculateValueForPercentage(CalculatePercentage(kill, death)));
	std::uint32_t win = _rv<std::uint32_t>(a2, 20);
	std::uint32_t lose = _rv<std::uint32_t>(a2, 24);
	std::uint32_t v258_t = 110211;
	auto v31_t = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(0x8B4600, (infoInstance + 1712), &v258_t);
	std::uint32_t cprog2 = 50;
	_call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t, std::uint32_t) >(0xE84A30, _rv<std::uint32_t>(v31_t, 0), 0, CalculateValueForPercentage(CalculatePercentage(win, lose)));

	std::uint32_t zombi_kill = _rv<std::uint32_t>(a2, 112);
	std::uint32_t zombi_infect = _rv<std::uint32_t>(a2, 116);
	std::uint32_t zombi_record = (zombi_kill + zombi_infect);
	std::uint32_t zombi_record_valdlg = 110227;

	auto zombi_record_instance = _call < std::uint32_t(__thiscall*)(std::uint32_t, std::uint32_t*) >(0x8B4600, (infoInstance + 1712), &zombi_record_valdlg);
	char* z_count = new char[9];
	sprintf_s(z_count, sizeof(z_count), "%d", zombi_record);
	//MegaGuard::EventLog->Debug(std::source_location::current(), "Zombi kill: %d Zombi infected: %d", zombi_kill, zombi_infect);
	//std::cout << "Zombi kill: " << zombi_kill << " Zombi infected: " << zombi_infect << '\n';

	//std::cout << z_count << '\n';
	//std::cout << "Should call: " << _rv<std::uint32_t>(_rv<std::uint32_t>(_rv<std::uint32_t>(zombi_record_instance, 0), 0), 216) << '\n';
	_call < std::uint32_t(__thiscall*)(std::uint32_t, char*) >(MegaGuard::Addresses::Dialog::WriteStatic, _rv<std::uint32_t>(zombi_record_instance, 0), z_count);
}
namespace Random
{
	std::random_device rd;
	std::mt19937 rng(rd());
	std::mt19937_64 rng64(rd());
	std::uniform_int_distribution<std::uint32_t> dist;
	std::uniform_int_distribution<std::uint64_t> dist64;
	std::uint32_t Gen()
	{
		return dist(rng);
	}
	std::uint64_t Gen64()
	{
		return dist64(rng64);
	}
	std::uint32_t CustomGen(const std::uint32_t min, const std::uint32_t max)
	{
		std::uniform_int_distribution<std::uint32_t> custom_dist(min, max);
		return custom_dist(rng);
	}
	std::uint64_t CustomGen64(const std::uint64_t min, const std::uint64_t max)
	{
		std::uniform_int_distribution<std::uint64_t> custom_dist64(min, max);
		return custom_dist64(rng64);
	}
}

/*
#pragma optimize("", off)
#pragma section(".mg", execute, read, write)
#pragma comment(linker,"/SECTION:.mg,ERW")
#pragma code_seg(push, ".mg")

namespace memory_protection
{
	inline std::mutex guard_mutex;
	inline std::vector<std::uintptr_t> pending_pages;
	enum class PageState { ENCRYPTED, DECRYPTED, PENDING_REENCRYPTION, HANDLING };
	struct PageStateData
	{
		PageState state;
		ULONG old_protect;
	};
	inline boost::unordered_flat_map<std::uintptr_t, PageStateData> page_info;
	inline std::atomic<bool> running{ true };
	constexpr std::chrono::milliseconds reencrypt_interval{ 100 };

	bool section_name_equals(const char* a, const char* b)
	{
		for (int i = 0; i < 8; ++i)
		{
			if (a[i] != b[i]) return false;
			if (a[i] == '\0' && b[i] == '\0') return true;
		}
		return true;
	}

	PIMAGE_SECTION_HEADER get_section_by_name(const char* name)
	{
		auto module_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_base);
		auto nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(module_base + dos_header->e_lfanew);
		auto section = IMAGE_FIRST_SECTION(nt_headers);
		for (WORD i = 0; i < nt_headers->FileHeader.NumberOfSections; ++i, ++section)
			if (section_name_equals(reinterpret_cast<const char*>(section->Name), name))
				return section;

		return nullptr;
	}
	bool find_eip_in_module(std::uint32_t eip)
	{
		if (eip >= MegaGuard::Globals::g_AntiCheatModuleBase && eip < MegaGuard::Globals::g_AntiCheatModuleBase + MegaGuard::Globals::g_AntiCheatModuleSize)
			return true;

		if (eip >= MegaGuard::Globals::g_GameModuleBase && eip < MegaGuard::Globals::g_GameModuleBase + MegaGuard::Globals::g_GameModuleSize)
			return true;
		return false;
	};

	void encrypt_section(PIMAGE_SECTION_HEADER section)
	{
		auto module_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		const std::size_t page_count = (section->Misc.VirtualSize + 0xFFF) / 0x1000;

		std::lock_guard lock(guard_mutex);
		for (std::size_t i = 0; i < page_count; ++i)
		{
			std::uintptr_t page_addr = module_base + section->VirtualAddress + i * 0x1000;

			if (!find_eip_in_module(page_addr)) continue;

			//ULONG old_protect;
			//auto page_ptr = reinterpret_cast<PVOID>(page_addr);
			//SIZE_T page_size = 0x1000;
			//MegaGuard::CloneNTSyscall<NtProtectVirtualMemory_t>("NtProtectVirtualMemory")(GetCurrentProcess(), &page_ptr, &page_size, PAGE_NOACCESS, &old_protect);
			DWORD old_protect;
			VirtualProtect(reinterpret_cast<LPVOID>(page_addr), 0x1000, PAGE_NOACCESS, &old_protect);
			page_info[page_addr] = { PageState::ENCRYPTED , old_protect };
		}
	}
	LONG NTAPI vectored_handler(PEXCEPTION_POINTERS ex_info)
	{
		auto ex_code = ex_info->ExceptionRecord->ExceptionCode;
	#if defined(_M_X64) || defined(__x86_64__)
		auto retn_addr = static_cast<std::uint64_t>(ex_info->ContextRecord->Rip);
	#else
		auto retn_addr = static_cast<std::uint32_t>(ex_info->ContextRecord->Eip);
	#endif
		if (ex_code == EXCEPTION_ACCESS_VIOLATION)
		{
			std::uintptr_t page_addr = ex_info->ExceptionRecord->ExceptionInformation[1] & ~0xFFFuLL;
			std::lock_guard lock(guard_mutex);
			if (!page_info.contains(page_addr))  return EXCEPTION_CONTINUE_SEARCH;

			if (page_info[page_addr].state == PageState::HANDLING || page_info[page_addr].state == PageState::PENDING_REENCRYPTION)
				return EXCEPTION_CONTINUE_SEARCH;

			if (!find_eip_in_module(page_addr)) return EXCEPTION_CONTINUE_SEARCH;

			page_info[page_addr].state = PageState::HANDLING;

		   // ULONG old_protect;
		   // auto page_ptr = reinterpret_cast<PVOID>(page_addr);
		   // SIZE_T page_size = 0x1000;
		   // MegaGuard::CloneNTSyscall<NtProtectVirtualMemory_t>("NtProtectVirtualMemory")(GetCurrentProcess(), &page_ptr, &page_size, page_info[page_addr].old_protect, &old_protect);

			DWORD old_protect;
			VirtualProtect(reinterpret_cast<LPVOID>(page_addr), 0x1000, page_info[page_addr].old_protect, &old_protect);
			pending_pages.push_back(page_addr);
			page_info[page_addr].state = PageState::DECRYPTED;
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		return EXCEPTION_CONTINUE_SEARCH;
	}
	void reencrypt_worker()
	{
		std::vector<std::uintptr_t> local_batch;
		local_batch.reserve(1024);
		while (running.load(std::memory_order_relaxed))
		{
			std::this_thread::sleep_for(reencrypt_interval);
			std::lock_guard lock(guard_mutex);
			pending_pages.swap(local_batch);
			for (std::uintptr_t page_addr : local_batch)
			{
				if (!page_info.contains(page_addr)) continue;

				if (!find_eip_in_module(page_addr)) continue;

				if (page_info[page_addr].state == PageState::HANDLING || page_info[page_addr].state == PageState::ENCRYPTED)
					continue;

				page_info[page_addr].state = PageState::PENDING_REENCRYPTION;

				//ULONG old_protect;
				//auto page_ptr = reinterpret_cast<PVOID>(page_addr);
				//SIZE_T page_size = 0x1000;
				//MegaGuard::CloneNTSyscall<NtProtectVirtualMemory_t>("NtProtectVirtualMemory")(GetCurrentProcess(), &page_ptr, &page_size, PAGE_NOACCESS, &old_protect);
			   DWORD old_protect;
			   VirtualProtect(reinterpret_cast<LPVOID>(page_addr), 0x1000, PAGE_NOACCESS, &old_protect);

				page_info[page_addr] = { PageState::ENCRYPTED , old_protect };
			}
			local_batch.clear();
		}
	}
	void initialize_protection(const char* section_name)
	{
		AddVectoredExceptionHandler(1, vectored_handler);

		encrypt_section(get_section_by_name(section_name));

		//for (int i = 0; i < (std::uint32_t)memory_protection::find_eip_in_module - (std::uint32_t)memory_protection::encrypt_section; i += 0x1)
		//    *(std::uint8_t*)((std::uint32_t)memory_protection::encrypt_section + i) = 0;

		std::thread(reencrypt_worker).detach();
	}
}

#pragma code_seg(pop, ".mg")
#pragma optimize("", on)
*/

typedef NTSTATUS(WINAPI* NtCreateThreadEx_t)(
	OUT PHANDLE ThreadHandle,
	IN ACCESS_MASK DesiredAccess,
	IN PVOID ObjectAttributes,
	IN HANDLE ProcessHandle,
	IN PVOID StartRoutine,
	IN PVOID Argument,
	IN ULONG CreateFlags,
	IN SIZE_T ZeroBits,
	IN SIZE_T StackSize,
	IN SIZE_T MaximumStackSize,
	IN PVOID AttributeList
	);

typedef NTSTATUS(WINAPI* NtClose_t)(HANDLE Handle);
typedef NTSTATUS(WINAPI* NtSetInformationThread_t)(
	HANDLE ThreadHandle,
	THREAD_INFORMATION_CLASS ThreadInformationClass,
	PVOID ThreadInformation,
	ULONG ThreadInformationLength
	);

//APIENTRY __attribute((__annotate__(("flattening,indirectcall,indirectbr,aliasaccess,boguscfg,linearmba"))))

struct EH4_SCOPETABLE_RECORD
{
	int EnclosingLevel;
	void* FilterFunc;
	void* HandlerFunc;
};

struct EH4_SCOPETABLE
{
	int GSCookieOffset;
	int GSCookieXOROffset;
	int EHCookieOffset;
	int EHCookieXOROffset;
	struct EH4_SCOPETABLE_RECORD ScopeRecord[];
};

struct EH4_EXCEPTION_REGISTRATION_RECORD
{
	void* SavedESP;
	EXCEPTION_POINTERS* ExceptionPointers;
	EXCEPTION_REGISTRATION_RECORD SubRecord;
	EH4_SCOPETABLE* EncodedScopeTable; //Xored with the __security_cookie
	unsigned int TryLevel;
};

//to access the global PE variables:
extern "C" LPVOID __ImageBase;
//extern "C" ULONG_PTR __security_cookie;

DWORD GetSizeOfImage(void);

void* g_ImageStartAddr = nullptr;
void* g_ImageEndAddr = nullptr;

//the exception handler function above works for other exceptions but is labed only for SEH4 since its easier to understand that way
//so this is a more generically named function
LONG NTAPI ExceptionHandler(_EXCEPTION_POINTERS* ExceptionInfo)
{
	//making sure to only process exceptions from the manual mapped code:
	PVOID ExceptionAddress = ExceptionInfo->ExceptionRecord->ExceptionAddress;
	if (ExceptionAddress < g_ImageStartAddr || ExceptionAddress > g_ImageEndAddr)
		return EXCEPTION_CONTINUE_SEARCH;

	DWORD RegisterESP = ExceptionInfo->ContextRecord->Esp;
	EXCEPTION_REGISTRATION_RECORD* pFs = (EXCEPTION_REGISTRATION_RECORD*)__readfsdword(0); // mov pFs, large fs:0 ; <= reading the segment register
	if ((DWORD_PTR)pFs > (RegisterESP - 0x10000) && (DWORD_PTR)pFs < (RegisterESP + 0x10000)) //validate pointer
	{
		EXCEPTION_ROUTINE* ExceptionHandlerRoutine = pFs->Handler;
		if (ExceptionHandlerRoutine > g_ImageStartAddr && ExceptionHandlerRoutine < g_ImageEndAddr) //validate pointer
		{
			EXCEPTION_DISPOSITION ExceptionDisposition = ExceptionHandlerRoutine(ExceptionInfo->ExceptionRecord, pFs, ExceptionInfo->ContextRecord, nullptr);
			if (ExceptionDisposition == ExceptionContinueExecution)
				return EXCEPTION_CONTINUE_EXECUTION;
		}
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

DWORD GetSizeOfImage(void)
{
	IMAGE_DOS_HEADER* ImageDosHeader = (IMAGE_DOS_HEADER*)&__ImageBase;
	if (!ImageDosHeader || ImageDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return NULL;

	IMAGE_NT_HEADERS* ImageNtHeaders = (PIMAGE_NT_HEADERS)((char*)ImageDosHeader + ImageDosHeader->e_lfanew);
	if (ImageNtHeaders->Signature != IMAGE_NT_SIGNATURE)
		return NULL;

	return ImageNtHeaders->OptionalHeader.SizeOfImage;
}

#pragma region _EHStructs

struct SCOPETABLE_ENTRY
{
	unsigned int enclosing_level;
	unsigned int filter;
	unsigned int specific_handler;
};

struct EXCEPTION_REGISTRATION_COMMON
{
	BYTE gap0[8];
	unsigned int scopetable;
	unsigned int trylevel;
};

struct EH3_EXCEPTION_REGISTRATION
{
	struct EH3_EXCEPTION_REGISTRATION* Next;
	EXCEPTION_ROUTINE* ExceptionHandler;
	SCOPETABLE_ENTRY* ScopeTable;
	DWORD TryLevel;
};

struct CPPEH_RECORD
{
	DWORD old_esp;
	EXCEPTION_POINTERS* exc_ptr;
	EH3_EXCEPTION_REGISTRATION registration;
};

struct EXCEPTION_REGISTRATION
{
	unsigned int prev;
	unsigned int handler;
};
#pragma endregion EHStructs

DWORD WINAPI HiddenThreadRoutine(LPVOID lpParam)
{
	g_ImageStartAddr = reinterpret_cast<void*>(MegaGuard::Globals::g_AntiCheatModuleBase);
	g_ImageEndAddr = reinterpret_cast<char*>(g_ImageStartAddr) + MegaGuard::Globals::g_AntiCheatModuleSize;

	AddVectoredExceptionHandler(1, ExceptionHandler);

	// Start splash in its own thread (non-blocking)
	//MegaGuard::Splash::StartSplash();

	// Continue with whitelist setup - this doesn't block
	MegaGuard::add_return_adresses(MegaGuard::Globals::g_GameModuleBase,
		MegaGuard::Globals::g_GameModuleSize,
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::Get.get(),
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Room::whitelist_return_addres);

	MegaGuard::add_return_adresses(MegaGuard::Globals::g_GameModuleBase,
		MegaGuard::Globals::g_GameModuleSize,
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::Get.get(),
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitContainer::whitelist_return_addres);

	MegaGuard::add_return_adresses(MegaGuard::Globals::g_GameModuleBase,
		MegaGuard::Globals::g_GameModuleSize,
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::Get.get(),
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::UnitMgr::whitelist_return_addres);

	MegaGuard::add_return_adresses(MegaGuard::Globals::g_GameModuleBase,
		MegaGuard::Globals::g_GameModuleSize,
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::Get.get(),
		MegaGuard::Addresses::Hooks::Anticheat::GameManagers::Dynamics::whitelist_return_addres);

	return 0;
}

void start_thread()
{
	/*
	static auto fnNtCreateThreadEx = MegaGuard::CloneNTSyscall<NtCreateThreadEx_t>("NtCreateThreadEx");
	static auto fnNtClose = MegaGuard::CloneNTSyscall<NtClose_t>("NtClose");
	static auto fnNtSetInformationThread = MegaGuard::CloneNTSyscall<NtSetInformationThread_t>("NtSetInformationThread");
	HANDLE hThread = NULL;
	NTSTATUS status = CreateThreadEx(
		&hThread,                     // Thread handle
		THREAD_ALL_ACCESS,             // Desired access
		NULL,                          // ObjectAttributes
		(HANDLE)-1,           // Target process handle
		(PVOID)HiddenThreadRoutine,    // Thread start address
		NULL,                          // Thread argument
		FALSE,                         // Create suspended
		0, 0, 0, NULL                  // Other parameters
	);
	MessageBoxA(0, "Thread created, setting information...", "Info", MB_OK);
	if (NT_SUCCESS(status))
	{
		if (hThread)
		{
			status = fnNtSetInformationThread(hThread, (THREAD_INFORMATION_CLASS)0x11, NULL, 0);
			fnNtClose(hThread);
		}
	}
	*/

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)HiddenThreadRoutine, NULL, 0, NULL);
}

struct MAPPED_REGION_INFO {
	void* base;
	DWORD size;
};
BOOL APIENTRY  DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1

		MegaGuard::EventLog = std::make_unique<MegaGuard::CLog>();
		MegaGuard::EventLog->Initialize("megaguard/debug.log", false);
#endif

		if (lpReserved) {
			// Manual mapped - use provided region info
			auto* regionInfo = reinterpret_cast<MAPPED_REGION_INFO*>(lpReserved);
			void* mappedBase = regionInfo->base;
			DWORD mappedSize = regionInfo->size;
			MegaGuard::Globals::InitializeDllRegion(hModule, mappedBase, mappedSize);
		}
		else {
			// Normal LoadLibrary - get size from PE headers
			MODULEINFO modInfo = {};
			if (GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo)))
			{
				MegaGuard::Globals::InitializeDllRegion(hModule, hModule, modInfo.SizeOfImage);
			}
			else
			{
				// Fallback: parse PE headers manually
				auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hModule);
				auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)hModule + dosHeader->e_lfanew);
				MegaGuard::Globals::InitializeDllRegion(hModule, hModule, ntHeaders->OptionalHeader.SizeOfImage);
			}
		}

		if (g_ptr) delete g_ptr;
		g_ptr = new pointer_encryption();
		start_thread();

		//ActiveBreach_launch();

		//auto handle = GetProcessHandleByName_NTDLL(L"notepad++.exe");
		//TerminateProcess(handle, 0);

		//GrdMem::Init();

#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
		dumpFile = "megaguard/crash/megaguard.dmp";
		//output_thread = std::thread(crash_handler_thread);
		//InitializeExceptionHandlers();

#endif

#if DEBUG_CONSOLE_LOG == 1
		if (!AllocConsole())
			MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to allocate console, error code: %d", GetLastError());

		if (!std::freopen("CONOUT$", "w", stdout))
			MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to redirect stdout, error code: %d", GetLastError());
		if (!std::freopen("CONIN$", "r", stdin))
			MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to redirect stdin, error code: %d", GetLastError());

		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE)
			MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to get standard output handle, error code: %d", GetLastError());

		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode))
			MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to get console mode, error code: %d", GetLastError());

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(hOut, dwMode))
			MegaGuard::EventLog->Debug(nostd::source_location::current(), "Failed to set console mode for virtual terminal processing, error code: %d", GetLastError());

		std::ios::sync_with_stdio(true);
#endif

		auto nation_index = _rv<std::uint32_t>(MegaGuard::Addresses::Features::NationIndex.get());
		if (nation_index == 20)
		{
			MegaGuard::Addresses::Features::NationIndexIsRom = true;
			_wv<std::uint32_t>(MegaGuard::Addresses::Features::NationIndex.get(), 0, 0);
		}
		else
		{
			MegaGuard::Addresses::Features::NationIndexIsRom = false;
			_wv<std::uint32_t>(MegaGuard::Addresses::Features::NationIndex.get(), 0, 3);
		}
		g_ipcClient = new MegaGuardIPCClient();
		if (g_ipcClient->Open()) {
			g_ipcClient->SetTotalManagers(4);
		}
		MegaGuard::HooksMgr::InitializeAllHooks();

		//memory_protection::initialize_protection(".text");

		return TRUE;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		if (g_ipcClient) g_ipcClient->Close();
		//MegaGuard::HooksMgr::RemoveAllHooks();
	}
	return FALSE;
}