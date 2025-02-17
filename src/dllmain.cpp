#include "pch.h"

#define PRINTD(_Format,...) printf(_Format,__VA_ARGS__)

#include <iostream>
#include <string>
#include <fstream>
#include <csignal>
#include <cstring>
#include <chrono>
#include <format>
#include <Windows.h>
#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1

#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
//#include <stacktrace> // C++23 stacktrace header
#include <StackWalker.h>

#include <ranges>




std::string dumpFile;
static std::string stackwalker_backtrace_json;
//static std::stacktrace trace;
static std::string header_message;
static int crash_signal = 0; // 0 is not a valid signal id
static std::mutex mut;
static std::condition_variable cv;
static std::thread output_thread;
enum class program_status
{
    running = 0,
    crashed = 1,
    ending = 2,
    normal_exit = 3,
};
static std::atomic<program_status> status = program_status::running;

static void json_pretty_print(std::ostream& os, const simdjson::dom::element& element, std::string* indent = nullptr)
{
    std::string indent_;
    if (!indent)
        indent = &indent_;

    switch (element.type())
    {
        case simdjson::dom::element_type::OBJECT:
        { // Handle JSON object
            os << "{\n";
            indent->append(4, ' ');
            bool first = true;
            for (auto [key, value] : element.get_object())
            {
                if (!first)
                {
                    os << ",\n";
                }
                first = false;
                os << *indent << "\"" << key << "\" : ";
                json_pretty_print(os, value, indent);
            }
            os << "\n";
            indent->resize(indent->size() - 4);
            os << *indent << "}";
            break;
        }

        case simdjson::dom::element_type::ARRAY:
        { // Handle JSON array
            os << "[\n";
            indent->append(4, ' ');
            bool first = true;
            for (const auto& item : element.get_array())
            {
                if (!first)
                {
                    os << ",\n";
                }
                first = false;
                os << *indent;
                json_pretty_print(os, item, indent);
            }
            os << "\n";
            indent->resize(indent->size() - 4);
            os << *indent << "]";
            break;
        }

        case simdjson::dom::element_type::STRING:
        { // Handle string values
            os << "\"" << element.get_string() << "\"";
            break;
        }

        case simdjson::dom::element_type::UINT64:
            os << "0x" << std::hex << element.get_uint64() << std::dec;
            break;

        case simdjson::dom::element_type::INT64:
            os << "0x" << std::hex << element.get_int64() << std::dec;
            break;

        case simdjson::dom::element_type::DOUBLE:
            os << element.get_double();
            break;

        case simdjson::dom::element_type::BOOL:
            os << (element.get_bool() ? "true" : "false");
            break;

        case simdjson::dom::element_type::NULL_VALUE:
            os << "null";
            break;
    }

    if (indent->empty())
    {
        os << "\n"; // Final newline after complete JSON
    }
}

/*
static void json_pretty_print(std::ostream& os, const Json::Value& jv, std::string* indent = nullptr)
{
    std::string indent_;
    if (!indent)
        indent = &indent_;

    switch (jv.type())
    {
        case Json::objectValue:
        { // Handle JSON object
            os << "{\n";
            indent->append(4, ' ');
            bool first = true;
            for (const auto& key : jv.getMemberNames())
            {
                if (!first)
                {
                    os << ",\n";
                }
                first = false;
                os << *indent << "\"" << key << "\" : ";
                json_pretty_print(os, jv[key], indent);
            }
            os << "\n";
            indent->resize(indent->size() - 4);
            os << *indent << "}";
            break;
        }

        case Json::arrayValue:
        { // Handle JSON array
            os << "[\n";
            indent->append(4, ' ');
            bool first = true;
            for (const auto& item : jv)
            {
                if (!first)
                {
                    os << ",\n";
                }
                first = false;
                os << *indent;
                json_pretty_print(os, item, indent);
            }
            os << "\n";
            indent->resize(indent->size() - 4);
            os << *indent << "]";
            break;
        }

        case Json::stringValue:
        { // Handle string values
            os << "\"" << jv.asString() << "\"";
            break;
        }

        case Json::uintValue: // Handle unsigned integers
            os << "0x" << std::hex << jv.asUInt64() << std::dec;
            break;

        case Json::intValue: // Handle signed integers
            os << "0x" << std::hex << jv.asInt64() << std::dec;
            break;

        case Json::realValue: // Handle double/float
            os << jv.asDouble();
            break;

        case Json::booleanValue: // Handle booleans
            os << (jv.asBool() ? "true" : "false");
            break;

        case Json::nullValue: // Handle null values
            os << "null";
            break;
    }

    if (indent->empty())
    {
        os << "\n"; // Final newline after complete JSON
    }
}
*/


/*
template <typename T>
void glaze_pretty_print(std::ostream& os, const T& value, std::string* indent = nullptr)
{
    std::string indent_;
    if (!indent)
        indent = &indent_;

    if constexpr (glz::is_object<T>::value)
    {
        os << "{\n";
        indent->append(4, ' ');
        bool first = true;

        glz::for_each(value, [&](auto&& member, auto&& name) {
            if (!first)
            {
                os << ",\n";
            }
            first = false;
            os << *indent << "\"" << name << "\" : ";
            glaze_pretty_print(os, member, indent);
        });

        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "}";
    }
    else if constexpr (glz::is_array<T>::value)
    {
        os << "[\n";
        indent->append(4, ' ');
        bool first = true;

        for (const auto& item : value)
        {
            if (!first)
            {
                os << ",\n";
            }
            first = false;
            os << *indent;
            glaze_pretty_print(os, item, indent);
        }

        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "]";
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        os << "\"" << value << "\"";
    }
    else if constexpr (std::is_integral_v<T>)
    {
        os << "0x" << std::hex << value << std::dec;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        os << value;
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        os << (value ? "true" : "false");
    }
    else
    {
        os << "null"; // Handle unsupported or unknown types as null
    }

    if (indent->empty())
    {
        os << "\n"; // Final newline after complete JSON
    }
}
*/

std::string GetCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;

    localtime_s(&localTime, &timeT);

    // Use std::ostringstream to build the timestamp string
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%Y%m%d_%H%M%S");  // Format: YYYYMMDD_HHMMSS
    return oss.str();
}

#include <iostream>
#include <string>
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
void set_crashlog_header_message(std::string message)
{
    std::unique_lock<std::mutex> lk(mut);
    if (status != program_status::running) return;
    header_message = message;
}
std::string get_crashlog_header_message()
{
    std::unique_lock<std::mutex> lk(mut);
    return header_message;
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
    //auto path = GenerateCrashLogFilePath(dumpFile);
    auto json_path = GenerateStacktraceJsonFilePath(dumpFile);
    //std::ofstream log(path);
    //if (!header_message.empty()) log << header_message << std::endl;
    //if (crash_signal != 0)
    //{
    //    log << "Received signal " << crash_signal << " " << try_get_signal_name(crash_signal) << std::endl;
    //}

    //log << trace;
    //log.close();
    std::ofstream stacktraceLog(json_path);
    stacktraceLog << stackwalker_backtrace_json;
    stacktraceLog.close();
}

static void crash_handler_thread()
{
    //wait for the program to crash or exit normally
    std::unique_lock<std::mutex> lk(mut);
    cv.wait(lk, [] { return status != program_status::running; });
    lk.unlock();

    //if it crashed, output the crash log
    if (status == program_status::crashed)
    {
        output_crash_log();
    }

    //alert the crashing thread we're done with the crash log so it can finish crashing
    status = program_status::ending;
    cv.notify_one();
}

struct CallstackEntryJson
{
    uint32_t frame;
    uint64_t offset;
    std::string moduleName;
    std::optional<uint64_t> base;
};


class XStackWalker : public StackWalker
{
public:
    XStackWalker() : StackWalker() {}

    std::ostringstream backtrace; // JSON will be built as a string
    uint32_t frame{};
    std::vector<uint32_t> printed_bases;

    auto has_base(const uintptr_t base) {
        return std::find(printed_bases.begin(), printed_bases.end(), base) != printed_bases.end();
    }

protected:
    virtual void OnOutput(LPCSTR) override {}

    virtual void OnCallstackEntry(CallstackEntryType eType, CallstackEntry& entry) override
    {
        if (frame >= 10) return;

        std::string moduleName = entry.moduleName;
        uintptr_t offset = entry.offset;
        uintptr_t base = entry.baseOfImage;

        // Start building JSON entry
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


static inline void crash_handler()
{
    //if we crashed during a crash... ignore lol
    if (status != program_status::running) return;
    
    XStackWalker sw;
    sw.ShowCallstack();

    std::stringstream ss;

    simdjson::dom::parser parser;
    simdjson::dom::element backtraceElement;

    auto error = parser.parse(sw.get_backtrace()).get(backtraceElement);
    json_pretty_print(ss, backtraceElement);

    stackwalker_backtrace_json = ss.str();
    
    //save the stacktrace
    //trace = std::stacktrace::current();

    //resume the monitoring thread
    status = program_status::crashed;
    cv.notify_one();

    //wait for the crash log to finish writing
    std::unique_lock<std::mutex> lk(mut);
    cv.wait(lk, [] { return status != program_status::crashed; });
}

static inline void signal_handler(int signal) {
    crash_signal = signal;
    crash_handler();
    RaiseException(EXCEPTION_BREAKPOINT, 0, 1, (ULONG_PTR*)&signal);
    //std::quick_exit(1);
}
static inline void terminator() {
    crash_handler();
    //std::quick_exit(1);
}
__declspec(noinline) static LONG WINAPI exception_handler(EXCEPTION_POINTERS* exceptionInfo)
{
    crash_handler();
    std::string dumpFilePathWithTimestamp = GenerateDumpFilePath(dumpFile);

    HANDLE hFile = CreateFileA(dumpFilePathWithTimestamp.c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ExceptionPointers = exceptionInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ClientPointers = TRUE;

        MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory);
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType, &dumpInfo, nullptr, nullptr);
        CloseHandle(hFile);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void __cdecl invalid_parameter_handler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    crash_handler();
    abort();
}

//callback needed during a normal exit to shut down the thread
static inline void normal_exit() {
    status = program_status::normal_exit;
    cv.notify_one();
    output_thread.join();
}

#endif


static void(__fastcall* ogProgFill)(std::uint32_t pInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t a3) = nullptr;
static void __fastcall hkProgFill(std::uint32_t pInstance, std::uint32_t edx, std::uint32_t a2, std::uint32_t a3) {
	//MegaGuard::EventLog->Debug(std::source_location::current(), "Was call Progress filler with info a2: %d a3: %d", a2, a3);
	//std::cout << "Was call Progress filler with info " << a2 << ' ' << a3 << '\n';
	ogProgFill(pInstance, edx, a2, a3);
}

std::uint32_t CalculateValueForPercentage(int x) {
	if (x <= 0 || x > 100) {
		throw std::invalid_argument("Percentage must be greater than 0 and less than or equal to 100.");
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
		_call < std::uint32_t(__thiscall*)(std::uint32_t, char *) >(MegaGuard::Addresses::Dialog::WriteStatic, _rv<std::uint32_t>(zombi_record_instance, 0), z_count);
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
	//std::cout << "called other info handler! " << kill << ' ' << death << ' ' << win << ' ' << lose;

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

//#pragma optimize("", off)
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
//#pragma optimize("", on)


//APIENTRY __attribute((__annotate__(("flattening,indirectcall,indirectbr,aliasaccess,boguscfg,linearmba"))))



BOOL APIENTRY  DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
        //GrdMem::Init();
        MegaGuard::Globals::InitializeDllRegion(hModule);
    #if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
        dumpFile = "megaguard/crash/megaguard.dmp";
        output_thread = std::thread(crash_handler_thread);
        SetUnhandledExceptionFilter(exception_handler);
        std::signal(SIGABRT, signal_handler);
        std::signal(SIGSEGV, signal_handler);
        std::signal(SIGILL, signal_handler);
        std::signal(SIGTERM, signal_handler);
        std::signal(SIGFPE, signal_handler);
        std::signal(SIGINT, signal_handler);
        std::set_terminate(terminator);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        _set_purecall_handler(terminator);
        _set_invalid_parameter_handler(&invalid_parameter_handler);
        std::atexit(normal_exit);
    #endif

	#if DEBUG_CONSOLE_LOG == 1 || DEBUG_FILE_LOG == 1
		MegaGuard::EventLog->Initialize("megaguard/debug.log", false);
	#endif

	#if DEBUG_CONSOLE_LOG == 1
		if (!AllocConsole())
			throw std::runtime_error("Failed to allocate console.");

		if (!std::freopen("CONOUT$", "w", stdout))
			throw std::runtime_error("Failed to redirect stdout.");
		if (!std::freopen("CONIN$", "r", stdin))
			throw std::runtime_error("Failed to redirect stdin.");

		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut == INVALID_HANDLE_VALUE)
			throw std::runtime_error("Failed to get standard output handle.");

		DWORD dwMode = 0;
		if (!GetConsoleMode(hOut, &dwMode))
			throw std::runtime_error("Failed to get console mode.");

		dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
		if (!SetConsoleMode(hOut, dwMode))
			throw std::runtime_error("Failed to set console mode for virtual terminal processing.");

		std::ios::sync_with_stdio(true);
	#endif

        
		MegaGuard::HooksMgr::InitializeAllHooks();
        memory_protection::initialize_protection(".text");
        

		return TRUE;
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		MegaGuard::HooksMgr::RemoveAllHooks();
	}
	return FALSE;
}