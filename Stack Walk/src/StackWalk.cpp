#include "../include/StackWalk.h"

#include <exception>

#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")





#ifdef __cpp_attributes
	#if __has_cpp_attribute(unlikely)
		#define UNLIKELY_IF(expr) [[unlikely]]if(expr)
	#else
		#define UNLIKELY_IF(expr) if(expr)
	#endif
#else
	#define UNLIKELY_IF(expr) if(expr)
#endif







struct STACKTRACE {
	HANDLE m_hProcess;
	HANDLE m_hThread;
};

void __fastcall stacktrace_create(HSTACKTRACE* hStackTrace)
{
	try {
		*hStackTrace = reinterpret_cast<HSTACKTRACE>(new STACKTRACE());
	}
	catch (std::bad_alloc& e) {
		//Many proggrammers think that new keyword returns nullptr on fail but that is not the case.
		// new throws a bad_alloc exception.
		// This stems from the implementation of malloc wich does return nullptr and does not throw
		//Handle the exception somehow. This is a rarity doe. Only thrown when there is no more memory
	}

	reinterpret_cast<STACKTRACE*>(*hStackTrace)->m_hProcess = GetCurrentProcess();
	reinterpret_cast<STACKTRACE*>(*hStackTrace)->m_hThread = GetCurrentThread();
}

void __fastcall stacktrace_destroy(HSTACKTRACE hStackTrace)
{
	CloseHandle(reinterpret_cast<STACKTRACE*>(hStackTrace)->m_hProcess);
	CloseHandle(reinterpret_cast<STACKTRACE*>(hStackTrace)->m_hThread);
	delete reinterpret_cast<STACKTRACE*>(hStackTrace);
}

void __fastcall stacktrace_capture(HSTACKTRACE hStackTrace, std::vector<STACK_FRAME>* lpBuffer)
{
	//See https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-context for docs on CONTEXT struct
	CONTEXT l_Context;

#ifdef _M_IX86
	//Since on x86 architecture RtlCaptureContext does not work some asm code is required to set up the context
	//It does not say on the msdn docs that it does not work. Durring my experimentation it didn't.
	//This way yielded way more consistent results.
	//See https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturecontext for RtlCaptureContext
	ZeroMemory(&l_Context, sizeof(CONTEXT));

	l_Context.ContextFlags = CONTEXT_CONTROL;

	//
	// Those three registers are enough.
	// I dont really remember where I got this from exactly.
	// Was either self coded or I saw it somwhere online.
	// Either way this is the "correct" way of doing it.
	// Might be something "better"/cleaner that I dont know of.
	//
	__asm
	{
	Label:
		mov[l_Context.Ebp], ebp;
		mov[l_Context.Esp], esp;
		mov eax, [Label];
		mov[l_Context.Eip], eax;
	}
#else
	//Since there exists a function for a x86_64 architecture might aswell use it.
	//See https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlcapturecontext for RtlCaptureContext
	RtlCaptureContext(&l_Context);
#endif

	STACKFRAME64 l_StackFrame;
	DWORD l_dwMachineType;

	ZeroMemory(&l_StackFrame, sizeof(STACKFRAME64));
	//There are certain predefined macros if compiled with MSVC compiler.
	//See https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=msvc-170 for Predefined Macros
#ifdef _M_IX86
	//Setting up things for x86
	l_dwMachineType = IMAGE_FILE_MACHINE_I386;
	l_StackFrame.AddrPC.Offset = l_Context.Eip;
	l_StackFrame.AddrPC.Mode = AddrModeFlat;
	l_StackFrame.AddrFrame.Offset = l_Context.Ebp;
	l_StackFrame.AddrFrame.Mode = AddrModeFlat;
	l_StackFrame.AddrStack.Offset = l_Context.Esp;
	l_StackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
	//Setting up things for x86_64
	l_dwMachineType = IMAGE_FILE_MACHINE_AMD64;
	l_StackFrame.AddrPC.Offset = l_Context.Rip;
	l_StackFrame.AddrPC.Mode = AddrModeFlat;
	l_StackFrame.AddrFrame.Offset = l_Context.Rsp;
	l_StackFrame.AddrFrame.Mode = AddrModeFlat;
	l_StackFrame.AddrStack.Offset = l_Context.Rsp;
	l_StackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
	//Setting up things for x64 I think. I designed this a long while ago. Cant remember where I found this macro
	l_dwMachineType = IMAGE_FILE_MACHINE_IA64;
	l_StackFrame.AddrPC.Offset = l_Context.StIIP;
	l_StackFrame.AddrPC.Mode = AddrModeFlat;
	l_StackFrame.AddrFrame.Offset = l_Context.IntSp;
	l_StackFrame.AddrFrame.Mode = AddrModeFlat;
	l_StackFrame.AddrBStore.Offset = l_Context.RsBSP;
	l_StackFrame.AddrBStore.Mode = AddrModeFlat;
	l_StackFrame.AddrStack.Offset = l_Context.IntSp;
	l_StackFrame.AddrStack.Mode = AddrModeFlat;
#else
#error "StackTrace::Unsupported platform"
#endif

	//Clearing the buffer before use
	lpBuffer->clear();
	for (int i = 0;; i++) {
		//Calling the StackWalk64 windows function
		//See https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-stackwalk64
		//Storring the return value for later checks
		//Basically the function return false on fail
		BOOL l_StackWalkResult =
			StackWalk64(
				l_dwMachineType,
				reinterpret_cast<STACKTRACE*>(hStackTrace)->m_hProcess,
				reinterpret_cast<STACKTRACE*>(hStackTrace)->m_hThread,
				&l_StackFrame,
				&l_Context,
				NULL,
				NULL,
				NULL,
				NULL
			);

		//UNLIKELY_IF is a macro defined above.
		UNLIKELY_IF(l_StackWalkResult == false) {
			break;//Error of some sort. The documentation states that StackWalk64 does not set last error
		}


		if (l_StackFrame.AddrPC.Offset == 0) {
			//This signifies the end of the stack.
			break;//Nothing left to do than to break out of the loop.
		}

		//Setup of the STACK_FRAME structure.
		//Note:
		//Only the function pointer is currently storred with the STACK_FRAME structure.
		//It is possible to implement local variable "sniffing" aswell.
		//Shouldnt be hard with a bit of ReadProcessMemory() :)
		STACK_FRAME l_CurrentFrame;
		l_CurrentFrame.m_Offset = reinterpret_cast<void*>(l_StackFrame.AddrPC.Offset);

		lpBuffer->push_back(l_CurrentFrame);
	}
	return;
}

