#pragma once
class GuardRegions
{
private:
    static std::vector<GuardRegions*> guarded_regions;
    void* buffer;
    SIZE_T size;

public:
    static void UpdateGuardedRegions();
    static LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionInfo);
    static void Init();

    GuardRegions(SIZE_T size);
    ~GuardRegions();

    void Guard() const;
    PVOID Data() const;
};
