// =============================================================================
// Assembler - asmjit wrapper (x86)
// =============================================================================
#pragma once

#include <memory>
#include "../../deps/asmjit/asmjit.h"

namespace mg {

class Assembler {
public:
    explicit Assembler(asmjit::Arch arch);
    ~Assembler() = default;

    Assembler(const Assembler&) = delete;
    Assembler& operator=(const Assembler&) = delete;

    asmjit::x86::Assembler* assembler() { return assembler_.get(); }
    asmjit::x86::Assembler* operator->() { return assembler_.get(); }

protected:
    asmjit::Environment environment_;
    asmjit::CodeHolder code_;
    std::unique_ptr<asmjit::x86::Assembler> assembler_;
};

} // namespace mg
