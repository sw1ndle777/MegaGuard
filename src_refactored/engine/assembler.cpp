// =============================================================================
// Assembler - Implementation
// =============================================================================
#include "pch.hpp"
#include "engine/assembler.hpp"

namespace mg {

Assembler::Assembler(asmjit::Arch arch) {
    environment_.setArch(arch);
    code_.init(environment_);
    assembler_ = std::make_unique<asmjit::x86::Assembler>(&code_);
}

} // namespace mg
