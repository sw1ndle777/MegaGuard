#include "../../../pch.h"
std::vector<GuardRegions*> GuardRegions::guarded_regions;

void GuardRegions::UpdateGuardedRegions()
{
    for (const auto instance : guarded_regions)
        instance->Guard();
}

LONG CALLBACK GuardRegions::VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
{
    if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_GUARD_PAGE_VIOLATION)
    {
        const PVOID page_address = reinterpret_cast<PVOID>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]);

        for (const auto instance : guarded_regions)
        {
            if (!(page_address >= instance->buffer && page_address < static_cast<PBYTE>(instance->buffer) + instance->size))
                continue;

            auto to = reinterpret_cast<std::uint32_t>(page_address);
            auto from = reinterpret_cast<std::uint32_t>(reinterpret_cast<PVOID>(exceptionInfo->ContextRecord->Eip));

            //MegaGuard::EventLog->Debug(std::source_location::current(), "Access to 0x{:08X} from 0x{:08X}", to, from);
            exceptionInfo->ContextRecord->EFlags |= 0x100;

            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    if (exceptionInfo->ExceptionRecord->ExceptionCode == STATUS_SINGLE_STEP)
    {
        UpdateGuardedRegions();
        return EXCEPTION_CONTINUE_EXECUTION;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void GuardRegions::Init()
{
    const PVOID value = AddVectoredExceptionHandler(1, &VectoredExceptionHandler);
    if (!value)
        throw std::exception("add vec exc fail");
}

GuardRegions::GuardRegions(const SIZE_T size) : size(size)
{
    buffer = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    if (!buffer)
        throw std::exception("alloc fail");

    guarded_regions.push_back(this);
}

GuardRegions::~GuardRegions()
{
    memset(buffer, 0, size);
    VirtualFree(buffer, 0, MEM_RELEASE);
    guarded_regions.erase(std::remove(guarded_regions.begin(), guarded_regions.end(), this), guarded_regions.end());
}

void GuardRegions::Guard() const
{
    DWORD oldProtect;
    if (!VirtualProtect(buffer, size, PAGE_READWRITE | PAGE_GUARD, &oldProtect))
        throw std::exception("set page protection fail");
}

PVOID GuardRegions::Data() const
{
    return buffer;
}