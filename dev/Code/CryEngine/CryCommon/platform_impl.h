/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : This file should only be included Once in DLL module.


#pragma once

#include <platform.h>
#include <StringUtils.h>
#include <ISystem.h>
#include <ITestSystem.h>
#include <CryExtension/Impl/RegFactoryNode.h>
#include <CryExtension/Impl/ICryFactoryRegistryImpl.h>
#include <IComponentFactory.h>
#include <UnicodeFunctions.h>

#include <AzCore/Debug/Profiler.h>
#include <AzCore/Debug/ProfileModuleInit.h>
#include <AzCore/Module/Environment.h>

// Section dictionary
#if defined(AZ_RESTRICTED_PLATFORM)
#define PLATFORM_IMPL_H_SECTION_TRAITS 1
#define PLATFORM_IMPL_H_SECTION_CRYLOWLATENCYSLEEP 2
#define PLATFORM_IMPL_H_SECTION_CRYGETFILEATTRIBUTES 3
#define PLATFORM_IMPL_H_SECTION_CRYSETFILEATTRIBUTES 4
#define PLATFORM_IMPL_H_SECTION_CRY_SYSTEM_FUNCTIONS 5
#define PLATFORM_IMPL_H_SECTION_VIRTUAL_ALLOCATORS 6
#endif

#if !defined(AZ_MONOLITHIC_BUILD)
SC_API struct SSystemGlobalEnvironment* gEnv = NULL;
ComponentFactoryCreationNode* ComponentFactoryCreationNode::sm_head = nullptr;
size_t ComponentFactoryCreationNode::sm_size = 0;
#endif //AZ_MONOLITHIC_BUILD

// Traits
#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION BITFIDDLING_H_SECTION_TRAITS
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/platform_impl_h_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/platform_impl_h_provo.inl"
    #endif
#elif defined(LINUX) || defined(APPLE)
#define PLATFORM_IMPL_H_TRAIT_DEFINE_GLOBAL_SREGFACTORYNODE 1
#endif

#if defined(_LAUNCHER) && (defined(_RELEASE) || PLATFORM_IMPL_H_TRAIT_DEFINE_GLOBAL_SREGFACTORYNODE) || !defined(AZ_MONOLITHIC_BUILD)
//The reg factory is used for registering the different modules along the whole project
struct SRegFactoryNode* g_pHeadToRegFactories = 0;
#endif

#if !defined(AZ_MONOLITHIC_BUILD) || defined(_LAUNCHER)
//////////////////////////////////////////////////////////////////////////
// If not in static library.
#include <CryThreadImpl.h>
#include <CryCommon.cpp>

#if defined(WIN32) || defined(WIN64)
void CryPureCallHandler()
{
    CryFatalError("Pure function call");
}

void CryInvalidParameterHandler(
    const wchar_t* expression,
    const wchar_t* function,
    const wchar_t* file,
    unsigned int line,
    uintptr_t pReserved)
{
    //size_t i;
    //char sFunc[128];
    //char sExpression[128];
    //char sFile[128];
    //wcstombs_s( &i,sFunc,sizeof(sFunc),function,_TRUNCATE );
    //wcstombs_s( &i,sExpression,sizeof(sExpression),expression,_TRUNCATE );
    //wcstombs_s( &i,sFile,sizeof(sFile),file,_TRUNCATE );
    //CryFatalError( "Invalid parameter detected in function %s. File: %s Line: %d, Expression: %s",sFunc,sFile,line,sExpression );
    CryFatalError("Invalid parameter detected in CRT function\n");
}

void InitCRTHandlers()
{
    _set_purecall_handler(CryPureCallHandler);
    _set_invalid_parameter_handler(CryInvalidParameterHandler);
}
#else
void InitCRTHandlers() {}
#endif

#ifndef SOFTCODE
//////////////////////////////////////////////////////////////////////////
// This is an entry to DLL initialization function that must be called for each loaded module
//////////////////////////////////////////////////////////////////////////
extern "C" AZ_DLL_EXPORT void ModuleInitISystem(ISystem* pSystem, const char* moduleName)
{
    if (gEnv) // Already registered.
    {
        return;
    }

    InitCRTHandlers();

    if (pSystem) // DONT REMOVE THIS. ITS FOR RESOURCE COMPILER!!!!
    {
        gEnv = pSystem->GetGlobalEnvironment();
        assert(gEnv);
        
        if (!AZ::Environment::IsReady() || (AZ::Environment::GetInstance() != gEnv->pSharedEnvironment))
        {
            AZ::Environment::Attach(gEnv->pSharedEnvironment);
        }
        AZ::Debug::ProfileModuleInit();

#if !defined(AZ_MONOLITHIC_BUILD)
        ICryFactoryRegistryImpl* pCryFactoryImpl = static_cast<ICryFactoryRegistryImpl*>(pSystem->GetCryFactoryRegistry());
        if (pCryFactoryImpl)
        {
            pCryFactoryImpl->RegisterFactories(g_pHeadToRegFactories);
        }

        AZ_Error("System", pCryFactoryImpl, "Failed to successfully load factory for %s.  You may have a missing or stale DLL that needs to be recompiled.", moduleName);
#endif
    } // if pSystem
}

extern "C" AZ_DLL_EXPORT void ModuleShutdownISystem(ISystem* pSystem)
{
    // Unregister with AZ environment.
    AZ::Environment::Detach();
}

extern "C" AZ_DLL_EXPORT void InjectEnvironment(void* env)
{
    static bool injected = false;
    if (!injected)
    {
        AZ::Environment::Attach(reinterpret_cast<AZ::EnvironmentInstance>(env));
        injected = true;
    }
}

extern "C" AZ_DLL_EXPORT void DetachEnvironment()
{
    AZ::Environment::Detach();
}

#endif // !defined(SOFTCODE)

bool g_bProfilerEnabled = false;
int g_iTraceAllocations = 0;

//////////////////////////////////////////////////////////////////////////
// global random number generator used by cry_random functions
CRndGen CryRandom_Internal::g_random_generator;


//////////////////////////////////////////////////////////////////////////

// when using STL Port _STLP_DEBUG and _STLP_DEBUG_TERMINATE - avoid actually
// crashing (default terminator seems to kill the thread, which isn't nice).
#ifdef _STLP_DEBUG_TERMINATE

# ifdef __stl_debug_terminate
#  undef __stl_debug_terminate
# endif

void __stl_debug_terminate(void)
{
    assert(0 && "STL Debug Error");
}
#endif

#ifdef _STLP_DEBUG_MESSAGE

# ifdef __stl_debug_message
#  undef __stl_debug_message
# endif

void __stl_debug_message(const char* format_str, ...)
{
    va_list __args;
    va_start(__args, format_str);
#ifdef WIN32
    char __buffer [4096];
    azvsnprintf(__buffer, sizeof(__buffer) / sizeof(char), format_str, __args);
    OutputDebugStringA(__buffer);
#endif
    gEnv->pLog->LogV(ILog::eErrorAlways, format_str, __args);
    va_end(__args);
}
#endif //_STLP_DEBUG_MESSAGE


#if defined(WIN32) || defined(WIN64)
#include <intrin.h>
#endif

// If we use cry memory manager this should be also included in every module.
#if defined(USING_CRY_MEMORY_MANAGER)
#include <CryMemoryManager_impl.h>
#endif

#if defined(APPLE) || defined(LINUX)
#include "CryAssert_impl.h"
#endif

#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION PLATFORM_IMPL_H_SECTION_CRY_SYSTEM_FUNCTIONS
#if defined(AZ_PLATFORM_XENIA)
#include "Xenia/platform_impl_h_xenia.inl"
#elif defined(AZ_PLATFORM_PROVO)
#include "Provo/platform_impl_h_provo.inl"
#endif
#endif

#if defined (_WIN32)

#include "CryAssert_impl.h"

//////////////////////////////////////////////////////////////////////////
void CryDebugBreak()
{
#if defined(WIN32) && !defined(RELEASE)
    if (IsDebuggerPresent())
#endif
    {
        DebugBreak();
    }
}

//////////////////////////////////////////////////////////////////////////
void CrySleep(unsigned int dwMilliseconds)
{
    AZ_PROFILE_FUNCTION_IDLE(AZ::Debug::ProfileCategory::System);
    Sleep(dwMilliseconds);
}

//////////////////////////////////////////////////////////////////////////
void CryLowLatencySleep(unsigned int dwMilliseconds)
{
    AZ_PROFILE_FUNCTION(AZ::Debug::ProfileCategory::System);
#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION PLATFORM_IMPL_H_SECTION_CRYLOWLATENCYSLEEP
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/platform_impl_h_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/platform_impl_h_provo.inl"
    #endif
#endif
#if defined(AZ_RESTRICTED_SECTION_IMPLEMENTED)
#undef AZ_RESTRICTED_SECTION_IMPLEMENTED
#else
    CrySleep(dwMilliseconds);
#endif
}

//////////////////////////////////////////////////////////////////////////
int CryMessageBox(const char* lpText, const char* lpCaption, unsigned int uType)
{
#ifdef WIN32
#if !defined(RESOURCE_COMPILER)
    ICVar* const pCVar = gEnv && gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_no_crash_dialog") : NULL;
    if ((pCVar && pCVar->GetIVal() != 0) || (gEnv && gEnv->bNoAssertDialog))
    {
        return 0;
    }
#endif
    wstring wideText, wideCaption;
    Unicode::Convert(wideText, lpText);
    Unicode::Convert(wideCaption, lpCaption);
    return MessageBoxW(NULL, wideText.c_str(), wideCaption.c_str(), uType);
#else
    return 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
void CryGetCurrentDirectory(unsigned int nBufferLength, char* lpBuffer)
{
    if (nBufferLength <= 0 || !lpBuffer)
    {
        return;
    }

    *lpBuffer = 0;

    // Get directory in UTF-16
    std::vector<wchar_t> buffer;
    {
        const size_t requiredLength = ::GetCurrentDirectoryW(0, 0);

        if (requiredLength <= 0)
        {
            return;
        }

        buffer.resize(requiredLength, 0);

        if (::GetCurrentDirectoryW(requiredLength, &buffer[0]) != requiredLength - 1)
        {
            return;
        }
    }

    // Convert to UTF-8
    if (Unicode::Convert<Unicode::eEncoding_UTF16, Unicode::eEncoding_UTF8>(lpBuffer, nBufferLength, &buffer[0]) > nBufferLength)
    {
        *lpBuffer = 0;
    }
}

// Initializes root folder of the game, optionally returns exe and path name.
void InitRootDir(char szExeFileName[] = 0, uint nExeSize = 0, char szExeRootName[] = 0, uint nRootSize = 0)
{
    WCHAR szExePathName[_MAX_PATH];
    size_t nLen = GetModuleFileNameW(GetModuleHandle(NULL), szExePathName, _MAX_PATH);
    assert(nLen < _MAX_PATH && "The path to the current executable exceeds the expected length");

    // Find path above exe name and deepest folder.
    int nCount = 0;
    for (size_t n = nLen - 1; n > 0; n--)
    {
        PREFAST_SUPPRESS_WARNING(6385);
        PREFAST_SUPPRESS_WARNING(6102);
        if (szExePathName[n] == '\\')
        {
            nLen = n;
            if (++nCount == 2)
            {
                break;
            }
        }
    }
    if (nCount > 0)
    {
        PREFAST_SUPPRESS_WARNING(6386);
        szExePathName[nLen++] = 0;

        // Switch to upper folder.
        SetCurrentDirectoryW(szExePathName);

        // Return exe path
        if (szExeRootName)
        {
            Unicode::Convert(szExeRootName, nRootSize, szExePathName);
        }

        // Return exe name
        if (szExeFileName)
        {
            Unicode::Convert(szExeFileName, nExeSize, szExePathName + nLen);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
short CryGetAsyncKeyState(int vKey)
{
#ifdef WIN32
    return GetAsyncKeyState(vKey);
#else
    return 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
LONG  CryInterlockedIncrement(int volatile* lpAddend)
{
    return InterlockedIncrement((volatile LONG*)lpAddend);
}

//////////////////////////////////////////////////////////////////////////
LONG  CryInterlockedDecrement(int volatile* lpAddend)
{
    return InterlockedDecrement((volatile LONG*)lpAddend);
}

//////////////////////////////////////////////////////////////////////////
LONG  CryInterlockedExchangeAdd(LONG volatile* lpAddend, LONG Value)
{
    return InterlockedExchangeAdd(lpAddend, Value);
}

LONG  CryInterlockedOr(LONG volatile* Destination, LONG Value)
{
    return InterlockedOr(Destination, Value);
}

LONG  CryInterlockedCompareExchange(LONG volatile* dst, LONG exchange, LONG comperand)
{
    return InterlockedCompareExchange(dst, exchange, comperand);
}

void* CryInterlockedCompareExchangePointer(void* volatile* dst, void* exchange, void* comperand)
{
    return InterlockedCompareExchangePointer(dst, exchange, comperand);
}

void* CryInterlockedExchangePointer(void* volatile* dst, void* exchange)
{
    return InterlockedExchangePointer(dst, exchange);
}

void CryInterlockedAdd(volatile size_t* pVal, ptrdiff_t iAdd)
{
#if defined (PLATFORM_64BIT)
    size_t v = (size_t)InterlockedAdd64((volatile int64*)pVal, iAdd);
#else
    size_t v = (size_t)CryInterlockedExchangeAdd((volatile long*)pVal, (long)iAdd);
    v += iAdd;
#endif
    assert((iAdd == 0) || (iAdd < 0 && v < v - (size_t)iAdd) || (iAdd > 0 && v > v - (size_t)iAdd));
}

//////////////////////////////////////////////////////////////////////////
void* CryCreateCriticalSection()
{
    CRITICAL_SECTION* pCS = new CRITICAL_SECTION;
    InitializeCriticalSection(pCS);
    return pCS;
}

void  CryCreateCriticalSectionInplace(void* pCS)
{
    InitializeCriticalSection((CRITICAL_SECTION*)pCS);
}
//////////////////////////////////////////////////////////////////////////
void  CryDeleteCriticalSection(void* cs)
{
    CRITICAL_SECTION* pCS = (CRITICAL_SECTION*)cs;
    if (pCS->LockCount >= 0)
    {
        CryFatalError("Critical Section hanging lock");
    }
    DeleteCriticalSection(pCS);
    delete pCS;
}

//////////////////////////////////////////////////////////////////////////
void  CryDeleteCriticalSectionInplace(void* cs)
{
    CRITICAL_SECTION* pCS = (CRITICAL_SECTION*)cs;
    if (pCS->LockCount >= 0)
    {
        CryFatalError("Critical Section hanging lock");
    }
    DeleteCriticalSection(pCS);
}

//////////////////////////////////////////////////////////////////////////
void  CryEnterCriticalSection(void* cs)
{
    EnterCriticalSection((CRITICAL_SECTION*)cs);
}

//////////////////////////////////////////////////////////////////////////
bool  CryTryCriticalSection(void* cs)
{
    return TryEnterCriticalSection((CRITICAL_SECTION*)cs) != 0;
}

//////////////////////////////////////////////////////////////////////////
void  CryLeaveCriticalSection(void* cs)
{
    LeaveCriticalSection((CRITICAL_SECTION*)cs);
}

//////////////////////////////////////////////////////////////////////////
uint32 CryGetFileAttributes(const char* lpFileName)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    BOOL res;
#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION PLATFORM_IMPL_H_SECTION_CRYGETFILEATTRIBUTES
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/platform_impl_h_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/platform_impl_h_provo.inl"
    #endif
#endif
#if defined(AZ_RESTRICTED_SECTION_IMPLEMENTED)
#undef AZ_RESTRICTED_SECTION_IMPLEMENTED
#else
    res = GetFileAttributesEx(lpFileName, GetFileExInfoStandard, &data);
#endif
    return res ? data.dwFileAttributes : -1;
}

//////////////////////////////////////////////////////////////////////////
bool CrySetFileAttributes(const char* lpFileName, uint32 dwFileAttributes)
{
#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION PLATFORM_IMPL_H_SECTION_CRYSETFILEATTRIBUTES
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/platform_impl_h_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/platform_impl_h_provo.inl"
    #endif
#endif
#if defined(AZ_RESTRICTED_SECTION_IMPLEMENTED)
#undef AZ_RESTRICTED_SECTION_IMPLEMENTED
#else
    return SetFileAttributes(lpFileName, dwFileAttributes) != 0;
#endif
}

//////////////////////////////////////////////////////////////////////////
threadID CryGetCurrentThreadId()
{
    return GetCurrentThreadId();
}

#endif // _WIN32

#if defined(AZ_RESTRICTED_PLATFORM) && defined(AZ_PLATFORM_PROVO)

//////////////////////////////////////////////////////////////////////////
uint32 CryGetFileAttributes(const char* lpFileName)
{
    assert(0);
    return -1;
}


//////////////////////////////////////////////////////////////////////////
bool CrySetFileAttributes(const char* lpFileName, uint32 dwFileAttributes)
{
    assert(0);
    return false;
}
#endif // defined(AZ_RESTRICTED_PLATFORM) && defined(AZ_PLATFORM_PROVO)

#endif //AZ_MONOLITHIC_BUILD

#if defined(AZ_PLATFORM_WINDOWS) && (!defined(AZ_MONOLITHIC_BUILD) || defined(_LAUNCHER))
int64 CryGetTicks()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

int64 CryGetTicksPerSec()
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    return li.QuadPart;
}
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Threads implementation. For static linking it must be declared inline otherwise creating multiple symbols
////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_MONOLITHIC_BUILD) && !defined(_LAUNCHER)
#define THR_INLINE inline
#else
#define THR_INLINE
#endif

//////////////////////////////////////////////////////////////////////////
inline void CryDebugStr(const char* format, ...)
{
    /*
     #ifdef CRYSYSTEM_EXPORTS
     va_list    ArgList;
     char       szBuffer[65535];
     va_start(ArgList, format);
     _vsnprintf_c(szBuffer,sizeof(szBuffer)-1, format, ArgList);
     va_end(ArgList);
     cry_strcat(szBuffer,"\n");
     OutputDebugString(szBuffer);
     #endif
     */
}

// load implementation of platform profile marker
#if !defined(AZ_MONOLITHIC_BUILD)
#include <CryProfileMarker_impl.h>
#endif

#if !defined(AZ_MONOLITHIC_BUILD) || defined(_LAUNCHER)
_MS_ALIGN(64) uint32  BoxSides[0x40 * 8] = {
    0, 0, 0, 0, 0, 0, 0, 0, //00
    0, 4, 6, 2, 0, 0, 0, 4, //01
    7, 5, 1, 3, 0, 0, 0, 4, //02
    0, 0, 0, 0, 0, 0, 0, 0, //03
    0, 1, 5, 4, 0, 0, 0, 4, //04
    0, 1, 5, 4, 6, 2, 0, 6, //05
    7, 5, 4, 0, 1, 3, 0, 6, //06
    0, 0, 0, 0, 0, 0, 0, 0, //07
    7, 3, 2, 6, 0, 0, 0, 4, //08
    0, 4, 6, 7, 3, 2, 0, 6, //09
    7, 5, 1, 3, 2, 6, 0, 6, //0a
    0, 0, 0, 0, 0, 0, 0, 0, //0b
    0, 0, 0, 0, 0, 0, 0, 0, //0c
    0, 0, 0, 0, 0, 0, 0, 0, //0d
    0, 0, 0, 0, 0, 0, 0, 0, //0e
    0, 0, 0, 0, 0, 0, 0, 0, //0f
    0, 2, 3, 1, 0, 0, 0, 4, //10
    0, 4, 6, 2, 3, 1, 0, 6, //11
    7, 5, 1, 0, 2, 3, 0, 6, //12
    0, 0, 0, 0, 0, 0, 0, 0, //13
    0, 2, 3, 1, 5, 4, 0, 6, //14
    1, 5, 4, 6, 2, 3, 0, 6, //15
    7, 5, 4, 0, 2, 3, 0, 6, //16
    0, 0, 0, 0, 0, 0, 0, 0, //17
    0, 2, 6, 7, 3, 1, 0, 6, //18
    0, 4, 6, 7, 3, 1, 0, 6, //19
    7, 5, 1, 0, 2, 6, 0, 6, //1a
    0, 0, 0, 0, 0, 0, 0, 0, //1b
    0, 0, 0, 0, 0, 0, 0, 0, //1c
    0, 0, 0, 0, 0, 0, 0, 0, //1d
    0, 0, 0, 0, 0, 0, 0, 0, //1e
    0, 0, 0, 0, 0, 0, 0, 0, //1f
    7, 6, 4, 5, 0, 0, 0, 4, //20
    0, 4, 5, 7, 6, 2, 0, 6, //21
    7, 6, 4, 5, 1, 3, 0, 6, //22
    0, 0, 0, 0, 0, 0, 0, 0, //23
    7, 6, 4, 0, 1, 5, 0, 6, //24
    0, 1, 5, 7, 6, 2, 0, 6, //25
    7, 6, 4, 0, 1, 3, 0, 6, //26
    0, 0, 0, 0, 0, 0, 0, 0, //27
    7, 3, 2, 6, 4, 5, 0, 6, //28
    0, 4, 5, 7, 3, 2, 0, 6, //29
    6, 4, 5, 1, 3, 2, 0, 6, //2a
    0, 0, 0, 0, 0, 0, 0, 0, //2b
    0, 0, 0, 0, 0, 0, 0, 0, //2c
    0, 0, 0, 0, 0, 0, 0, 0, //2d
    0, 0, 0, 0, 0, 0, 0, 0, //2e
    0, 0, 0, 0, 0, 0, 0, 0, //2f
    0, 0, 0, 0, 0, 0, 0, 0, //30
    0, 0, 0, 0, 0, 0, 0, 0, //31
    0, 0, 0, 0, 0, 0, 0, 0, //32
    0, 0, 0, 0, 0, 0, 0, 0, //33
    0, 0, 0, 0, 0, 0, 0, 0, //34
    0, 0, 0, 0, 0, 0, 0, 0, //35
    0, 0, 0, 0, 0, 0, 0, 0, //36
    0, 0, 0, 0, 0, 0, 0, 0, //37
    0, 0, 0, 0, 0, 0, 0, 0, //38
    0, 0, 0, 0, 0, 0, 0, 0, //39
    0, 0, 0, 0, 0, 0, 0, 0, //3a
    0, 0, 0, 0, 0, 0, 0, 0, //3b
    0, 0, 0, 0, 0, 0, 0, 0, //3c
    0, 0, 0, 0, 0, 0, 0, 0, //3d
    0, 0, 0, 0, 0, 0, 0, 0, //3e
    0, 0, 0, 0, 0, 0, 0, 0, //3f
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION PLATFORM_IMPL_H_SECTION_VIRTUAL_ALLOCATORS
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/platform_impl_h_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/platform_impl_h_provo.inl"
    #endif
#endif
#if defined(AZ_RESTRICTED_SECTION_IMPLEMENTED)
#undef AZ_RESTRICTED_SECTION_IMPLEMENTED
#endif

#endif // !AZ_MONOLITHIC_BUILD || _LAUNCHER

