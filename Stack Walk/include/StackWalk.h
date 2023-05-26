#pragma once
#include <vector>
/// <summary>
/// A handle that describes a STACKTRACE instance.
/// Using handles is for flexibility.
/// For example depending on what the use case is a handle can be storred on the stack or on the heap or as a global
/// For this case it can not be storred on the stack since it could corrupt it hence the proggrammer is forced
/// to use a handle to the STACKTRACE object instead of the object itself.
/// </summary>
typedef void* HSTACKTRACE;

/// <summary>
/// A structure containing information about the stack frame.
/// A custom object is used instead of the Windows one purely out of portability.
/// That said there is no real reason not to use the windows STACKFRAME64 structure instead.
/// 
/// Note:
///		This structure is not feature compleete.
///		There are still lots of things that could be implemented and storred here.
/// </summary>
struct STACK_FRAME {
	/// <summary>
	/// The address of the called function.
	/// </summary>
	void* m_Offset;
};





/*

About __fastcall:
	You will see me use __fastcall calling convention here.
	This is due to the "HANDLE" way of implementing this.
	__fastcall passes the arguments that are of DWORD size and smaller into the ECX and EDX registers.
	Its quite simmilar to __thiscall wich is used in classes where the instance(this keyword) is passed in ECX(I think)

	See https://learn.microsoft.com/en-us/cpp/cpp/fastcall?view=msvc-170 for more info on __fastcall

	Note that __fastcall only "works" on x86. It will compile on x86_64 but it will be compiled as __stdcall
*/








/// <summary>
/// Creates a stack trace instance.
/// Internally it allocates space on the heap for a structure containing the current process handle and the current
/// thread handle. Those handles will be used later on to perform the trace.
/// </summary>
/// <param name="hStackTrace">[out]Pointer to a HSTACKTRACE handle</param>
/// <returns></returns>
void __fastcall stacktrace_create(HSTACKTRACE* hStackTrace);

/// <summary>
/// Destroys the stack trace instance.
/// Internally it closes the handles of the current process and thread and deletes the allocated structure.
/// </summary>
/// <param name="hStackTrace">[in]Handle to the stack trace instance.</param>
/// <returns></returns>
void __fastcall stacktrace_destroy(HSTACKTRACE hStackTrace);

/// <summary>
/// Performs a snapshot/capture of the stack on the process and thread that called stacktrace_create.
/// With the current design it isnt advisable to call this function from another thread other than the
/// thread that called stacktrace_create
/// </summary>
/// <param name="hStackTrace">[in]Handle to the stack trace instance.</param>
/// <param name="lpBuffer">[out]Pointer to a std::vector class used to store the information of the stack</param>
/// <returns></returns>
void __fastcall stacktrace_capture(HSTACKTRACE hStackTrace, std::vector<STACK_FRAME>* lpBuffer);