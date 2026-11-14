#pragma once
#ifdef __cplusplus
// Older sources relied on transitive STL includes from Visual Studio 2017.
#include <memory>
#include <algorithm>
#include <limits>
#include <functional>
#include <string>
#include <iterator>
#include <vector>
#include <windows.h>
// .NET Framework 4 CLR host class ID (not declared by the CoreCLR-only header).
inline constexpr GUID CLSID_CLRRuntimeHost = {0x90F1A06E,0x7712,0x4762,{0x86,0xB5,0x7A,0x5E,0xBA,0x6B,0xDB,0x02}};
extern "C" HRESULT __stdcall GetRequestedRuntimeVersion(LPWSTR,LPWSTR,DWORD,DWORD*);
#endif
