#include "../src/pch.h"
#include "assembler.h"

namespace midhook
{
	Assembler::Assembler(Arch arch)
	{
		m_environment.setArch(arch);
		m_code.init(m_environment);
		m_assembler = std::make_unique<asmjit::x86::Assembler>(&m_code);
	}
}