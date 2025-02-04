#pragma once

#include <memory>
#include "../../deps/asmjit/asmjit.h"
namespace midhook
{
	using namespace asmjit;

	class Assembler
	{
	public:
		/// <summary>
		/// Create's an asmjit assembler
		/// </summary>
		/// <param name="arch">architecture</param>
		Assembler(Arch arch);

	public:
		/// <summary>
		/// pointer to underlying assembler object
		/// </summary>
		/// <returns></returns>
		inline x86::Assembler* assembler() { return m_assembler.get(); }
		/// <summary>
		/// pointer to underlying assembler object
		/// </summary>
		/// <returns></returns>
		inline x86::Assembler* operator ->() { return m_assembler.get(); }

	private:
		Assembler(const Assembler&) = delete;
		Assembler& operator=(const Assembler&) = delete;

	protected:
		Environment m_environment;
		CodeHolder m_code;
		std::unique_ptr<asmjit::x86::Assembler> m_assembler;
	};
}