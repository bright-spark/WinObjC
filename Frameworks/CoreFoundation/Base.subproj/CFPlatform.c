// clang-format off

// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//


/*  CFPlatform.c
    Copyright (c) 1999 - 2015 Apple Inc. and the Swift project authors
    Responsibility: Tony Parker
*/

#include "CFInternal.h"
#include "CFPriv.h"
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    #include <stdlib.h>
    #include <sys/stat.h>
    #include <string.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <pwd.h>
    #include <crt_externs.h>
    #include <mach-o/dyld.h>
#endif

#if DEPLOYMENT_TARGET_WINDOWS
#include <shellapi.h>
#include <shlobj.h>
#include <WinIoCtl.h>
#include <direct.h>
#define getcwd _NS_getcwd

// WINOBJC: includes for App Data folder.
#include <COMIncludes.h>
#include <wrl\client.h>
#include <wrl\wrappers\corewrappers.h>
#include <windows.storage.h>
#include <COMIncludes_End.h>

using namespace ABI::Windows::Storage;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;

// WINOBJC: File not available to appcontainer apps // #include <shfolder.h>
#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c

// WINOBJC: Export CFCopyHomeDirectoryURL declaration needed in this translation unit
#include "CFUtilities.h""

#endif

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI || DEPLOYMENT_TARGET_WINDOWS
#define kCFPlatformInterfaceStringEncoding  kCFStringEncodingUTF8
#else
#define kCFPlatformInterfaceStringEncoding  CFStringGetSystemEncoding()
#endif

extern void __CFGetUGIDs(uid_t *euid, gid_t *egid);

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
// CoreGraphics and LaunchServices are only projects (1 Dec 2006) that use these
char **_CFArgv(void) { return *_NSGetArgv(); }
int _CFArgc(void) { return *_NSGetArgc(); }
#endif


CF_PRIVATE Boolean _CFGetCurrentDirectory(char *path, int maxlen) {
    return getcwd(path, maxlen) != NULL;
}

#if DEPLOYMENT_TARGET_WINDOWS
// Returns the path to the CF DLL, which we can then use to find resources like char sets
bool bDllPathCached = false;
CF_PRIVATE const wchar_t *_CFDLLPath(void) {
    static wchar_t cachedPath[MAX_PATH+1];

    if (!bDllPathCached) {
#ifdef _DEBUG
        // might be nice to get this from the project file at some point
        // WINOBJC: WinobjC uses same dll name for both configurations. // wchar_t *DLLFileName = L"CoreFoundation_debug.dll";
        wchar_t *DLLFileName = L"CoreFoundation.dll";
#else
        wchar_t *DLLFileName = L"CoreFoundation.dll";
#endif

        HMODULE ourModule = GetModuleHandle(DLLFileName);
        
        CFAssert(ourModule, __kCFLogAssertion, "GetModuleHandle failed");

        DWORD wResult = GetModuleFileNameW(ourModule, cachedPath, MAX_PATH+1);
        CFAssert1(wResult > 0, __kCFLogAssertion, "GetModuleFileName failed: %d", GetLastError());
        CFAssert1(wResult < MAX_PATH+1, __kCFLogAssertion, "GetModuleFileName result truncated: %s", cachedPath);

        // strip off last component, the DLL name
        CFIndex idx;
        for (idx = wResult - 1; idx; idx--) {
            if ('\\' == cachedPath[idx]) {
                cachedPath[idx] = '\0';
                break;
            }
        }
        bDllPathCached = true;
    }
    return cachedPath;
}
#endif

static const char *__CFProcessPath = NULL;
static const char *__CFprogname = NULL;

const char **_CFGetProgname(void) {
    if (!__CFprogname)
        _CFProcessPath();       // sets up __CFprogname as a side-effect
    return &__CFprogname;
}

const char **_CFGetProcessPath(void) {
    if (!__CFProcessPath)
        _CFProcessPath();       // sets up __CFProcessPath as a side-effect
    return &__CFProcessPath;
}

#if DEPLOYMENT_TARGET_WINDOWS
const char *_CFProcessPath(void) {
    if (__CFProcessPath) return __CFProcessPath;
    wchar_t buf[CFMaxPathSize] = {0};
    DWORD rlen = GetModuleFileNameW(NULL, buf, sizeof(buf) / sizeof(buf[0]));
    if (0 < rlen) {
    char asciiBuf[CFMaxPathSize] = {0};
    int res = WideCharToMultiByte(CP_UTF8, 0, buf, rlen, asciiBuf, sizeof(asciiBuf) / sizeof(asciiBuf[0]), NULL, NULL);
    if (0 < res) {
        __CFProcessPath = strdup(asciiBuf);
        __CFprogname = strrchr(__CFProcessPath, PATH_SEP);
        __CFprogname = (__CFprogname ? __CFprogname + 1 : __CFProcessPath);
    }
    }
    if (!__CFProcessPath) {
    __CFProcessPath = "";
        __CFprogname = __CFProcessPath;
    }
    return __CFProcessPath;
}
#endif

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
const char *_CFProcessPath(void) {
    if (__CFProcessPath) return __CFProcessPath;
#if DEPLOYMENT_TARGET_MACOSX
    if (!__CFProcessIsRestricted()) {
    const char *path = (char *)__CFgetenv("CFProcessPath");
    if (path) {
        __CFProcessPath = strdup(path);
        __CFprogname = strrchr(__CFProcessPath, PATH_SEP);
        __CFprogname = (__CFprogname ? __CFprogname + 1 : __CFProcessPath);
        return __CFProcessPath;
    }
    }
#endif
    uint32_t size = CFMaxPathSize;
    char buffer[size];
    if (0 == _NSGetExecutablePath(buffer, &size)) {
    __CFProcessPath = strdup(buffer);
    __CFprogname = strrchr(__CFProcessPath, PATH_SEP);
    __CFprogname = (__CFprogname ? __CFprogname + 1 : __CFProcessPath);
    }
    if (!__CFProcessPath) {
    __CFProcessPath = "";
        __CFprogname = __CFProcessPath;
    }
    return __CFProcessPath;
}
#endif

#if DEPLOYMENT_TARGET_LINUX
#include <unistd.h>

const char *_CFProcessPath(void) {
    if (__CFProcessPath) return __CFProcessPath;
    char buf[CFMaxPathSize + 1];
    
    ssize_t res = readlink("/proc/self/exe", buf, CFMaxPathSize);
    if (res > 0) {
        // null terminate, readlink does not
        buf[res] = 0;
        __CFProcessPath = strdup(buf);
        __CFprogname = strrchr(__CFProcessPath, PATH_SEP);
        __CFprogname = (__CFprogname ? __CFprogname + 1 : __CFProcessPath);
    } else {
        __CFProcessPath = "";
        __CFprogname = __CFProcessPath;
    }
    return __CFProcessPath;
}
#endif

CF_PRIVATE CFStringRef _CFProcessNameString(void) {
    static CFStringRef __CFProcessNameString = NULL;
    if (!__CFProcessNameString) {
        const char *processName = *_CFGetProgname();
        if (!processName) processName = "";
        CFStringRef newStr = CFStringCreateWithCString(kCFAllocatorSystemDefault, processName, kCFPlatformInterfaceStringEncoding);
        if (!OSAtomicCompareAndSwapPtrBarrier(NULL, (void *) newStr, (void * volatile *)& __CFProcessNameString)) {
            CFRelease(newStr);    // someone else made the assignment, so just release the extra string.
        }
    }
    return __CFProcessNameString;
}


#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD

#include <pwd.h>
#include <sys/param.h>

// Set the fallBackToHome parameter to true if we should fall back to the HOME environment variable if all else fails. Otherwise return NULL.
static CFURLRef _CFCopyHomeDirURLForUser(struct passwd *upwd, bool fallBackToHome) {
    const char *fixedHomePath = issetugid() ? NULL : __CFgetenv("CFFIXED_USER_HOME");
    const char *homePath = NULL;
    
    // Calculate the home directory we will use
    // First try CFFIXED_USER_HOME (only if not setugid), then fall back to the upwd, then fall back to HOME environment variable
    CFURLRef home = NULL;
    if (!issetugid() && fixedHomePath) home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)fixedHomePath, strlen(fixedHomePath), true);
    if (!home && upwd && upwd->pw_dir) home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)upwd->pw_dir, strlen(upwd->pw_dir), true);
    if (fallBackToHome && !home) homePath = __CFgetenv("HOME");
    if (fallBackToHome && !home && homePath) home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)homePath, strlen(homePath), true);
    
    return home;
}

#endif


#define CFMaxHostNameLength 256
#define CFMaxHostNameSize   (CFMaxHostNameLength+1)

CF_PRIVATE CFStringRef _CFStringCreateHostName(void) {
    char myName[CFMaxHostNameSize];
    __CFSocketInitializeWinSock(); // WINOBJC: ensure winsock is initialized, otherwise gethostname will fail.

    // return @"" instead of nil a la CFUserName() and Ali Ozer
    if (0 != gethostname(myName, CFMaxHostNameSize)) myName[0] = '\0';
    return CFStringCreateWithCString(kCFAllocatorSystemDefault, myName, kCFPlatformInterfaceStringEncoding);
}

/* These are sanitized versions of the above functions. We might want to eliminate the above ones someday.
   These can return NULL.
*/
CF_EXPORT CFStringRef CFGetUserName(void) {
    return CFCopyUserName();
}

/**
 @Status Caveat
 @Notes Returns "" as UserName
*/
CF_EXPORT CFStringRef CFCopyUserName(void) {
    CFStringRef result = NULL;
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
    uid_t euid;
    __CFGetUGIDs(&euid, NULL);
    struct passwd *upwd = getpwuid(euid ? euid : getuid());
    if (upwd && upwd->pw_name) {
        result = CFStringCreateWithCString(kCFAllocatorSystemDefault, upwd->pw_name, kCFPlatformInterfaceStringEncoding);
    } else {
        const char *cuser = __CFgetenv("USER");
        if (cuser) {
            result = CFStringCreateWithCString(kCFAllocatorSystemDefault, cuser, kCFPlatformInterfaceStringEncoding);
        }
    }
#elif DEPLOYMENT_TARGET_WINDOWS
    wchar_t username[1040];
    DWORD size = 1040;
    username[0] = 0;
    if (false) { // WINOBJC: Can't get username in appcontainer. // if (GetUserNameW(username, &size)) {
        // discount the extra NULL by decrementing the size
        result = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, (const UniChar *)username, size - 1);
    } else {
        const char *cname = __CFgetenv("USERNAME");
        if (cname) {
                result = CFStringCreateWithCString(kCFAllocatorSystemDefault, cname, kCFPlatformInterfaceStringEncoding);
            }
    }
#else
#error Dont know how to compute user name on this platform
#endif
    if (!result)
        result = (CFStringRef)CFRetain(CFSTR(""));
    return result;
}

// WINOBJC: helper to get path for Storage Folder.
Wrappers::HString GetAppDataPath(bool localAppData) {
    Wrappers::HString toReturn;
    ComPtr<IStorageFolder> folder;
    ComPtr<IApplicationDataStatics> applicationDataStatics;
    ComPtr<IApplicationData> applicationData;
    ComPtr<IStorageItem> storageItem;

    if (FAILED(GetActivationFactory(Wrappers::HStringReference(RuntimeClass_Windows_Storage_ApplicationData).Get(),
        &applicationDataStatics))) {
        return toReturn;
    }

    if (FAILED(applicationDataStatics->get_Current(&applicationData))) {
        return toReturn;
    }

    if (localAppData)
    {
        if (FAILED(applicationData->get_LocalFolder(&folder))) {
            return toReturn;
        }
    }
    else
    {
        if (FAILED(applicationData->get_RoamingFolder(&folder))) {
            return toReturn;
        }
    }

    if (FAILED(folder.As<IStorageItem>(&storageItem))) {
        return toReturn;
    }

    if (FAILED(storageItem->get_Path(toReturn.GetAddressOf())))
    {
        return toReturn;
    }

    return toReturn;
}

// WINOBJC: export for foundation use
CF_EXPORT CFURLRef CFCopyHomeDirectoryURL(void) {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
    uid_t euid;
    __CFGetUGIDs(&euid, NULL);
    struct passwd *upwd = getpwuid(euid ? euid : getuid());
    return _CFCopyHomeDirURLForUser(upwd, true);
#elif DEPLOYMENT_TARGET_WINDOWS
    CFURLRef retVal = NULL;
    CFIndex len = 0;
    CFStringRef str = NULL;
   
    UniChar pathChars[MAX_PATH];
    // WINOBJC: App container applications cannot acces the "Home Directory" normally. Instead default to return the App Data folder.
    /*
    if (S_OK == SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, (wchar_t *)pathChars)) {        
        len = (CFIndex)wcslen((wchar_t *)pathChars);
        str = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, pathChars, len);
        retVal = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLWindowsPathStyle, true);
        CFRelease(str);
    }
    */

    Wrappers::HString path = GetAppDataPath(true);

    if(path.IsValid()) {
        unsigned int rawLength;
        const wchar_t* rawPath = WindowsGetStringRawBuffer(path.Get(), &rawLength);
        str = CFStringCreateWithBytes(kCFAllocatorSystemDefault, reinterpret_cast<const uint8_t *>(rawPath), rawLength * sizeof(wchar_t), kCFStringEncodingUTF16, NO);
        retVal = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLWindowsPathStyle, true);
        CFRelease(str);
    }

    if (!retVal) {
        // Fall back to environment variable, but this will not be unicode compatible
        const char *cpath = __CFgetenv("HOMEPATH");
        const char *cdrive = __CFgetenv("HOMEDRIVE");
        if (cdrive && cpath) {
            char fullPath[CFMaxPathSize];
            strlcpy(fullPath, cdrive, sizeof(fullPath));
            strlcat(fullPath, cpath, sizeof(fullPath));
            str = CFStringCreateWithCString(kCFAllocatorSystemDefault, fullPath, kCFPlatformInterfaceStringEncoding);
            retVal = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLWindowsPathStyle, true);
            CFRelease(str);
        }
    }

    if (!retVal) {
        // Last resort: We have to get "some" directory location, so fall-back to the processes current directory.
        UniChar currDir[MAX_PATH];
        DWORD dwChars = GetCurrentDirectoryW(MAX_PATH + 1, (wchar_t *)currDir);
        if (dwChars > 0) {
            len = (CFIndex)wcslen((wchar_t *)currDir);
            str = CFStringCreateWithCharacters(kCFAllocatorDefault, currDir, len);
            retVal = CFURLCreateWithFileSystemPath(NULL, str, kCFURLWindowsPathStyle, true);
            CFRelease(str);
        }
    }

    // We could do more here (as in KB Article Q101507). If that article is to be believed, we should only run into this case on Win95, or through user error.
    CFStringRef testPath = CFURLCopyFileSystemPath(retVal, kCFURLWindowsPathStyle);
    if (CFStringGetLength(testPath) == 0) {
        CFRelease(retVal);
        retVal = NULL;
    }
    if (testPath) CFRelease(testPath);

    return retVal;
#else
#error Dont know how to compute users home directories on this platform
#endif
}

CF_EXPORT CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName) {
#if TARGET_IPHONE_SIMULATOR
    if (!uName) { // TODO: Handle other cases here? See <rdar://problem/18504645> SIM: CFCopyHomeDirectoryURLForUser should not call getpwuid
        static CFURLRef home;
        static dispatch_once_t once = 0;
        
        dispatch_once(&once, ^{
            const char *env = getenv("CFFIXED_USER_HOME");
            if (!env) {
                env = getenv("HOME");
            }
            if (env) {
                CFStringRef str = CFStringCreateWithFileSystemRepresentation(kCFAllocatorSystemDefault, env);
                if (str) {
                    home = CFURLCreateWithFileSystemPath(kCFAllocatorSystemDefault, str, kCFURLPOSIXPathStyle, true);
                    CFRelease(str);
                }
            }
        });
        
        if (home) {
            return static_cast<CFURLRef>(CFRetain(home));
        }
    }
#endif
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
    if (!uName) {
        uid_t euid;
        __CFGetUGIDs(&euid, NULL);
        struct passwd *upwd = getpwuid(euid ? euid : getuid());
        return _CFCopyHomeDirURLForUser(upwd, true);
    } else {
        struct passwd *upwd = NULL;
        char buf[128], *user;
        SInt32 len = CFStringGetLength(uName), size = CFStringGetMaximumSizeForEncoding(len, kCFPlatformInterfaceStringEncoding);
        CFIndex usedSize;
        if (size < 127) {
            user = buf;
        } else {
            user = CFAllocatorAllocate(kCFAllocatorSystemDefault, size+1, 0);
        }
        if (CFStringGetBytes(uName, CFRangeMake(0, len), kCFPlatformInterfaceStringEncoding, 0, true, (uint8_t *)user, size, &usedSize) == len) {
            user[usedSize] = '\0';
            upwd = getpwnam(user);
        }
        if (buf != user) {
            CFAllocatorDeallocate(kCFAllocatorSystemDefault, user);
        }
        return _CFCopyHomeDirURLForUser(upwd, false);
    }
#elif DEPLOYMENT_TARGET_WINDOWS
    // This code can only get the directory for the current user
    CFStringRef userName = uName ? CFCopyUserName() : NULL;
    if (uName && !CFEqual(uName, userName)) {
        CFLog(kCFLogLevelError, CFSTR("CFCopyHomeDirectoryURLForUser(): Unable to get home directory for other user"));
        if (userName) CFRelease(userName);
        return NULL;
    }
    if (userName) CFRelease(userName);
    return CFCopyHomeDirectoryURL();
#else
#error Dont know how to compute users home directories on this platform
#endif
}


#undef CFMaxHostNameLength
#undef CFMaxHostNameSize

#if DEPLOYMENT_TARGET_WINDOWS
CF_INLINE CFIndex strlen_UniChar(const UniChar* p) {
    CFIndex result = 0;
    while ((*p++) != 0)
        ++result;
    return result;
}

//#include <shfolder.h>
/*
 * _CFCreateApplicationRepositoryPath returns the path to the application's
 * repository in a CFMutableStringRef. The path returned will be:
 *     <nFolder_path>\Apple Computer\<bundle_name>\
 * or if the bundle name cannot be obtained:
 *     <nFolder_path>\Apple Computer\
 * where nFolder_path is obtained by calling SHGetFolderPath with nFolder
 * (for example, with CSIDL_APPDATA or CSIDL_LOCAL_APPDATA).
 *
 * The CFMutableStringRef result must be released by the caller.
 *
 * If anything fails along the way, the result will be NULL.  
 */
CF_EXPORT CFMutableStringRef _CFCreateApplicationRepositoryPath(CFAllocatorRef alloc, int nFolder) {
    CFMutableStringRef result = NULL;
    CFStringRef str = NULL;

    // WINOBJC: make sure that nFolder is CSIDL_APPDATA or CSIDL_LOCAL_APPDATA and return the app data folder for the app.
    Wrappers::HString path = GetAppDataPath(nFolder == CSIDL_LOCAL_APPDATA);
    if (path.IsValid()) {
        unsigned int rawLength;
        const wchar_t* rawPath = WindowsGetStringRawBuffer(path.Get(), &rawLength);
        str = CFStringCreateWithBytes(kCFAllocatorSystemDefault, reinterpret_cast<const uint8_t *>(rawPath), rawLength * sizeof(wchar_t), kCFStringEncodingUTF16, NO);
        result = CFStringCreateMutableCopy(kCFAllocatorSystemDefault, 0, str);
        CFRelease(str);
    }

    CFStringAppend(result, _CFGetSlashStr());
    return ( result );

}
#endif

#pragma mark -
#pragma mark Thread Functions

#if DEPLOYMENT_TARGET_WINDOWS

// This code from here:
// http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx

const DWORD MS_VC_EXCEPTION=0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

CF_EXPORT void _NS_pthread_setname_np(const char *name) {
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;

    __try
    {
        RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

CF_EXPORT int _NS_pthread_main_np() {
    static pthread_t __initialPthread = pthread_self();
    pthread_t me = pthread_self();

    return (pthread_equal(__initialPthread, me));
}

#endif

#pragma mark -
#pragma mark Thread Local Data

// If slot >= CF_TSD_MAX_SLOTS, the SPI functions will crash at NULL + slot address.
// If thread data has been torn down, these functions should crash on CF_TSD_BAD_PTR + slot address.
#define CF_TSD_MAX_SLOTS 70


// Windows and Linux, not sure how many times the destructor could get called; CF_TSD_MAX_DESTRUCTOR_CALLS could be 1

#define CF_TSD_BAD_PTR ((void *)0x1000)

typedef void (*tsdDestructor)(void *);

// Data structure to hold TSD data, cleanup functions for each
typedef struct __CFTSDTable {
    uint32_t destructorCount;
    uintptr_t data[CF_TSD_MAX_SLOTS];
    tsdDestructor destructors[CF_TSD_MAX_SLOTS];
} __CFTSDTable;

static void __CFTSDFinalize(void *arg);

#if DEPLOYMENT_TARGET_WINDOWS

static DWORD __CFTSDIndexKey = 0xFFFFFFFF;

// Called from CFRuntime's startup code, on Windows only
CF_PRIVATE void __CFTSDWindowsInitialize() {
    __CFTSDIndexKey = TlsAlloc();
}

// Called from CFRuntime's cleanup code, on Windows only
CF_PRIVATE void __CFTSDWindowsCleanup() {
    TlsFree(__CFTSDIndexKey);
}

// Called for each thread as it exits, on Windows only
CF_PRIVATE void __CFFinalizeWindowsThreadData() {
    // Normally, this should call the finalizer several times to emulate the behavior of pthreads on Windows. However, a few bugs keep us from doing this:
    // <rdar://problem/8989063> REGRESSION(CF-610-CF-611): Crash closing Safari in BonjourDB destructor (Windows)
    // <rdar://problem/9326814> SyncUIHandler crashes after conflict is resolved and we do SyncNow
    //  and a bug in dispatch keeps us from using pthreadsWin32 directly, because it does not deal with the case of a dispatch_async happening during process exit (it attempts to create a thread, but that is illegal on Win32 and causes a hang).
    // So instead we just finalize once, which is the behavior pre-Airwolf anyway
    __CFTSDFinalize(TlsGetValue(__CFTSDIndexKey));
}

#endif


#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI || DEPLOYMENT_TARGET_LINUX || DEPLOYMENT_TARGET_FREEBSD
static pthread_key_t __CFTSDIndexKey;
#endif

CF_PRIVATE void __CFTSDInitialize() {
#if DEPLOYMENT_TARGET_LINUX
    (void)pthread_key_create(&__CFTSDIndexKey, __CFTSDFinalize);
#else
    (void)pthread_key_create((pthread_key_t*)(&__CFTSDIndexKey), __CFTSDFinalize);
#endif
}

static void __CFTSDSetSpecific(void *arg) {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    pthread_setspecific(__CFTSDIndexKey, arg);
#elif DEPLOYMENT_TARGET_LINUX
    pthread_setspecific(__CFTSDIndexKey, arg);
#elif DEPLOYMENT_TARGET_WINDOWS
    TlsSetValue(__CFTSDIndexKey, arg);
#endif
}

static void *__CFTSDGetSpecific() {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
    return pthread_getspecific(__CFTSDIndexKey);
#elif DEPLOYMENT_TARGET_LINUX
    return pthread_getspecific(__CFTSDIndexKey);
#elif DEPLOYMENT_TARGET_WINDOWS
    return TlsGetValue(__CFTSDIndexKey);
#endif
}




static void __CFTSDFinalize(void *arg) {
    // Set our TSD so we're called again by pthreads. It will call the destructor PTHREAD_DESTRUCTOR_ITERATIONS times as long as a value is set in the thread specific data. We handle each case below.
    __CFTSDSetSpecific(arg);

    if (!arg || arg == CF_TSD_BAD_PTR) {
        // We've already been destroyed. The call above set the bad pointer again. Now we just return.
        return;
    }
    
    __CFTSDTable *table = (__CFTSDTable *)arg;
    table->destructorCount++;
        
    // On first calls invoke destructor. Later we destroy the data.
    // Note that invocation of the destructor may cause a value to be set again in the per-thread data slots. The destructor count and destructors are preserved.  
    // This logic is basically the same as what pthreads does. We just skip the 'created' flag.
    for (int32_t i = 0; i < CF_TSD_MAX_SLOTS; i++) {
        if (table->data[i] && table->destructors[i]) {
            uintptr_t old = table->data[i];
            table->data[i] = (uintptr_t)NULL;
            table->destructors[i]((void *)(old));
        }
    }
    
    if (table->destructorCount == PTHREAD_DESTRUCTOR_ITERATIONS - 1) {    // On PTHREAD_DESTRUCTOR_ITERATIONS-1 call, destroy our data
        free(table);
        
        // Now if the destructor is called again we will take the shortcut at the beginning of this function.
        __CFTSDSetSpecific(CF_TSD_BAD_PTR);
        return;
    }
}

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
extern int pthread_key_init_np(int, void (*)(void *));
#endif

// Get or initialize a thread local storage. It is created on demand.
static __CFTSDTable *__CFTSDGetTable() {
    __CFTSDTable *table = (__CFTSDTable *)__CFTSDGetSpecific();
    // Make sure we're not setting data again after destruction.
    if (table == CF_TSD_BAD_PTR) {
        return NULL;
    }
    // Create table on demand
    if (!table) {
        // This memory is freed in the finalize function
        table = (__CFTSDTable *)calloc(1, sizeof(__CFTSDTable));
        // Windows and Linux have created the table already, we need to initialize it here for other platforms. On Windows, the cleanup function is called by DllMain when a thread exits. On Linux the destructor is set at init time.
        __CFTSDSetSpecific(table);
    }
    
    return table;
}


// For the use of CF and Foundation only
CF_EXPORT void *_CFGetTSD(uint32_t slot) {
    if (slot > CF_TSD_MAX_SLOTS) {
        _CFLogSimple(kCFLogLevelError, "Error: TSD slot %d out of range (get)", slot);
        HALT;
    }
    __CFTSDTable *table = __CFTSDGetTable();
    if (!table) {
        // Someone is getting TSD during thread destruction. The table is gone, so we can't get any data anymore.
        _CFLogSimple(kCFLogLevelWarning, "Warning: TSD slot %d retrieved but the thread data has already been torn down.", slot);
        return NULL;
    }
    uintptr_t *slots = (uintptr_t *)(table->data);
    return (void *)slots[slot];
}

// For the use of CF and Foundation only
CF_EXPORT void *_CFSetTSD(uint32_t slot, void *newVal, tsdDestructor destructor) {
    if (slot > CF_TSD_MAX_SLOTS) {
        _CFLogSimple(kCFLogLevelError, "Error: TSD slot %d out of range (set)", slot);
        HALT;
    }
    __CFTSDTable *table = __CFTSDGetTable();
    if (!table) {
        // Someone is setting TSD during thread destruction. The table is gone, so we can't get any data anymore.
        _CFLogSimple(kCFLogLevelWarning, "Warning: TSD slot %d set but the thread data has already been torn down.", slot);
        return NULL;
    }

    void *oldVal = (void *)table->data[slot];
    
    table->data[slot] = (uintptr_t)newVal;
    table->destructors[slot] = destructor;
    
    return oldVal;
}


#pragma mark -
#pragma mark Windows Wide to UTF8 and UTF8 to Wide

#if DEPLOYMENT_TARGET_WINDOWS
/* On Windows, we want to use UTF-16LE for path names to get full unicode support. Internally, however, everything remains in UTF-8 representation. These helper functions stand between CF and the Microsoft CRT to ensure that we are using the right representation on both sides. */

#include <sys/stat.h>
#include <share.h>

// Creates a buffer of wchar_t to hold a UTF16LE version of the UTF8 str passed in. Caller must free the buffer when done. If resultLen is non-NULL, it is filled out with the number of characters in the string.
static wchar_t *createWideFileSystemRepresentation(const char *str, CFIndex *resultLen) {
    // Get the real length of the string in UTF16 characters
    CFStringRef cfStr = CFStringCreateWithCString(kCFAllocatorSystemDefault, str, kCFStringEncodingUTF8);
    CFIndex strLen = CFStringGetLength(cfStr);
    
    // Allocate a wide buffer to hold the converted string, including space for a NULL terminator
    wchar_t *wideBuf = (wchar_t *)malloc((strLen + 1) * sizeof(wchar_t));
    
    // Copy the string into the buffer and terminate
    CFStringGetCharacters(cfStr, CFRangeMake(0, strLen), (UniChar *)wideBuf);
    wideBuf[strLen] = 0;
    
    CFRelease(cfStr);
    if (resultLen) *resultLen = strLen;
    return wideBuf;
}

// Copies a UTF16 buffer into a supplied UTF8 buffer. 
static void copyToNarrowFileSystemRepresentation(const wchar_t *wide, CFIndex dstBufSize, char *dstbuf) {
    // Get the real length of the wide string in UTF8 characters
    CFStringRef cfStr = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, (const UniChar *)wide, wcslen(wide));
    CFIndex strLen = CFStringGetLength(cfStr);
    CFIndex bytesUsed;
    
    // Copy the wide string into the buffer and terminate
    CFStringGetBytes(cfStr, CFRangeMake(0, strLen), kCFStringEncodingUTF8, 0, false, (uint8_t *)dstbuf, dstBufSize, &bytesUsed);
    dstbuf[bytesUsed] = 0;
    
    CFRelease(cfStr);
}

CF_EXPORT int _NS_stat(const char *name, struct _stat *st) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    int res = _wstat(wide, st);
    free(wide);
    return res;
}

CF_EXPORT int _NS_mkdir(const char *name) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    int res = _wmkdir(wide);
    free(wide);
    return res;
}

CF_EXPORT int _NS_rmdir(const char *name) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    int res = _wrmdir(wide);
    free(wide);
    return res;
}

CF_EXPORT int _NS_chmod(const char *name, int mode) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    
    // Convert mode
    int newMode = 0;
    if (mode | 0400) newMode |= _S_IREAD;
    if (mode | 0200) newMode |= _S_IWRITE;
    if (mode | 0100) newMode |= _S_IEXEC;
    
    int res = _wchmod(wide, newMode);
    free(wide);
    return res;
}

CF_EXPORT int _NS_unlink(const char *name) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    int res = _wunlink(wide);
    free(wide);
    return res;
}

// Warning: this doesn't support dstbuf as null even though 'getcwd' does
CF_EXPORT char *_NS_getcwd(char *dstbuf, size_t size) {
    if (!dstbuf) {
    CFLog(kCFLogLevelWarning, CFSTR("CFPlatform: getcwd called with null buffer"));
    return 0;
    }
    
    wchar_t *buf = _wgetcwd(NULL, 0);
    if (!buf) {
        return NULL;
    }
        
    // Convert result to UTF8
    copyToNarrowFileSystemRepresentation(buf, (CFIndex)size, dstbuf);
    free(buf);
    return dstbuf;
}

CF_EXPORT char *_NS_getenv(const char *name) {
    // todo: wide getenv
    // We have to be careful what happens here, because getenv is called during cf initialization, and things like cfstring may not be working yet
    // WINOBJC: can't get env variables in an appcontainer. // return getenv(name);
    return nullptr;
}

CF_EXPORT int _NS_rename(const char *oldName, const char *newName) {
    wchar_t *oldWide = createWideFileSystemRepresentation(oldName, NULL);
    wchar_t *newWide = createWideFileSystemRepresentation(newName, NULL);
    // _wrename on Windows does not behave exactly as rename() on Mac OS -- if the file exists, the Windows one will fail whereas the Mac OS version will replace
    // To simulate the Mac OS behavior, we use the Win32 API then fill out errno if something goes wrong
    BOOL winRes = MoveFileExW(oldWide, newWide, MOVEFILE_REPLACE_EXISTING);
    DWORD error = GetLastError();
    if (!winRes) {
        switch (error) {
            case ERROR_SUCCESS:
                errno = 0;
                break;
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
            case ERROR_OPEN_FAILED:
                errno = ENOENT;
                break;
            case ERROR_ACCESS_DENIED:
                errno = EACCES;
                break;
            default:
                errno = error;
        }
    }
    free(oldWide);
    free(newWide);
    return (winRes ? 0 : -1);
}

CF_EXPORT int _NS_open(const char *name, int oflag, int pmode) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    int fd;
    _wsopen_s(&fd, wide, oflag, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    free(wide);
    return fd;
}

CF_EXPORT int _NS_chdir(const char *name) {
    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    int res = _wchdir(wide);
    free(wide);
    return res;
}

CF_EXPORT int _NS_access(const char *name, int amode) {
    // execute is always true
    if (amode == 1) return 0;

    wchar_t *wide = createWideFileSystemRepresentation(name, NULL);
    // we only care about the read-only (04) and write-only (02) bits, so mask octal 06
    int res = _waccess(wide, amode & 06);
    free(wide);
    return res;
}

// This is a bit different than the standard 'mkstemp', because the size parameter is needed so we know the size of the UTF8 buffer
// Also, we don't avoid the race between creating a temporary file name and opening it on Windows like we do on Mac
CF_EXPORT int _NS_mkstemp(char *name, int bufSize) {
    CFIndex nameLen;
    wchar_t *wide = createWideFileSystemRepresentation(name, &nameLen);
    
    // First check to see if the directory that this new temporary file will be created in exists. If not, set errno to ENOTDIR. This mimics the behavior of mkstemp on MacOS more closely.
    // Look for the last '\' in the path
    wchar_t *lastSlash = wcsrchr(wide, '\\');
    if (!lastSlash) {
    free(wide);
    return -1;
    }
    
    // Set the last slash to NULL temporarily and use it for _wstat
    *lastSlash = 0;
    struct _stat dirInfo;
    int res = _wstat(wide, &dirInfo);
    if (res < 0) {
    if (errno == ENOENT) {
        errno = ENOTDIR;
    }
    free(wide);
    return -1;
    }
    // Restore the last slash
    *lastSlash = '\\';
    
    errno_t err = _wmktemp_s(wide, nameLen + 1);
    if (err != 0) {
        free(wide);
        return 0;
    }
    
    int fd;
    _wsopen_s(&fd, wide, _O_RDWR | _O_CREAT | CF_OPENFLGS, _SH_DENYNO, _S_IREAD | _S_IWRITE);
    
    // Convert the wide name back into the UTF8 buffer the caller supplied
    copyToNarrowFileSystemRepresentation(wide, bufSize, name);
    free(wide);
    return fd;    
}


// Utilities to convert from a volume name to a drive letter

Boolean _isAFloppy(char driveLetter)
{
/*
    HANDLE h;
    TCHAR tsz[8];
    Boolean retval = false;
    int iDrive;
    
    if (driveLetter >= 'a' && driveLetter <= 'z') {
        driveLetter = driveLetter - 'a' + 'A';
    }
    
    if ((driveLetter < 'A') || (driveLetter > 'Z')) {
        // invalid driveLetter; I guess it's not a floppy...
        return false;
    }
    
    iDrive = driveLetter - 'A' + 1;
    
    // On Windows NT, use the technique described in the Knowledge Base article Q115828 and in the "FLOPPY" SDK sample.
    wprintf_s(tsz, TEXT("\\\\.\\%c:"), TEXT('@') + iDrive);
    h = CreateFile2(tsz, 0, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (h != INVALID_HANDLE_VALUE)
    {
        DISK_GEOMETRY Geom[20];
        DWORD cb;
        
        if (DeviceIoControl (h, IOCTL_DISK_GET_MEDIA_TYPES, 0, 0,
                             Geom, sizeof(Geom), &cb, 0)
            && cb > 0)
        {
            switch (Geom[0].MediaType)
            {
                case F5_1Pt2_512: // 5.25 1.2MB floppy
                case F5_360_512:  // 5.25 360K  floppy
                case F5_320_512:  // 5.25 320K  floppy
                case F5_320_1024: // 5.25 320K  floppy
                case F5_180_512:  // 5.25 180K  floppy
                case F5_160_512:  // 5.25 160K  floppy
                case F3_1Pt44_512: // 3.5 1.44MB floppy
                case F3_2Pt88_512: // 3.5 2.88MB floppy
                case F3_20Pt8_512: // 3.5 20.8MB floppy
                case F3_720_512:   // 3.5 720K   floppy
                    retval = true;
                    break;
            }
        }
        
        CloseHandle(h);
    }
    */
    // WINOBJC: Was trying to use desktop only apis. Just assume no one uses / has a floppy drive anymore.
    return false;
}


extern CFStringRef CFCreateWindowsDrivePathFromVolumeName(CFStringRef volNameStr) {
    // WINOBJC: can't use a lot of these desktop APIS. return nullptr as File System is locked down much more in app.
    /*
    if (!volNameStr) return NULL;
    
    // This code is designed to match as closely as possible code from QuickTime's library
    CFIndex strLen = CFStringGetLength(volNameStr);
    if (strLen == 0) {
    return NULL;
    }
    
    // Get drive names
    long length, result;
    wchar_t *driveNames = NULL;
    
    // Get the size of the buffer to store the list of drives
    length = GetLogicalDriveStringsW(0, 0);
    if (!length) {
        return NULL;
    }
    
    driveNames = (wchar_t *)malloc((length + 1) * sizeof(wchar_t));
    result = GetLogicalDriveStringsW(length, driveNames);
    
    if (!result || result > length) {
        free(driveNames);
        return NULL;
    }
    
    // Get the volume name string into a wide buffer
    wchar_t *theVolumeName = (wchar_t *)malloc((strLen + 1) * sizeof(wchar_t));
    CFStringGetCharacters(volNameStr, CFRangeMake(0, strLen), (UniChar *)theVolumeName);
    theVolumeName[strLen] = 0;
    
    // lowercase volume name
    _wcslwr(theVolumeName);
    
    // Iterate through the drive names, looking for something that matches
    wchar_t *drivePtr = driveNames;
    CFStringRef drivePathResult = NULL;

    while (*drivePtr) {
        _wcslwr(drivePtr);
        
        if (!_isAFloppy((char)*drivePtr)) {
            UINT                oldErrorMode;
            DWORD               whoCares1, whoCares2;
            BOOL                getVolInfoSucceeded;
            UniChar             thisVolumeName[MAX_PATH];
            
            // Convert this drive string into a volume name
            oldErrorMode = SetErrorMode(0x0001); // SEM_FAILCRITICALERRORS
            getVolInfoSucceeded = GetVolumeInformationW(drivePtr, (LPWSTR)thisVolumeName, sizeof(thisVolumeName), NULL, &whoCares1, &whoCares2, NULL, 0);
            SetErrorMode(oldErrorMode);
            
            if (getVolInfoSucceeded) {
                _wcslwr((wchar_t *)thisVolumeName);
                
                // If the volume corresponding to this drive matches the input volume
                // then this drive is the winner.
                if (!wcscmp((const wchar_t *)thisVolumeName, theVolumeName) || 
                    (*thisVolumeName == 0x00 && (CFStringCompare(volNameStr, CFSTR("NONAME"), 0) == kCFCompareEqualTo))) {
                    drivePathResult = CFStringCreateWithCharacters(kCFAllocatorSystemDefault, (const UniChar *)drivePtr, wcslen(drivePtr));
                    break;
                }
            }
        }
        
        drivePtr += wcslen(drivePtr) + 1;
    }

    
    free(driveNames);
    free(theVolumeName);
    return drivePathResult;
    */
    return nullptr;
}

struct timezone {
    int tz_minuteswest; /* minutes west of Greenwich */
    int tz_dsttime; /* type of dst correction */
};

CF_PRIVATE int _NS_gettimeofday(struct timeval *tv, struct timezone *tz) {
    if (tv) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        unsigned __int64 t = 0;
        t |= ft.dwHighDateTime;
        t <<= 32;
        t |= ft.dwLowDateTime;
        
        // Convert to microseconds
        t /= 10;
        
        // Difference between 1/1/1970 and 1/1/1601
        t -= 11644473600000000Ui64;
        
        // Convert microseconds to seconds
        tv->tv_sec = (long)(t / 1000000UL);
        tv->tv_usec = (long)(t % 1000000UL);
    }
    
    // We don't support tz
    return 0;
}

bool OSAtomicCompareAndSwapPtr(void *oldp, void *newp, void *volatile *dst) 
{
    return (::InterlockedCompareExchangePointer(dst, newp, oldp) == oldp);
}

bool OSAtomicCompareAndSwapLong(long oldl, long newl, long volatile *dst) 
{ 
    return (::InterlockedCompareExchange(dst, newl, oldl) == oldl);
}

bool OSAtomicCompareAndSwapPtrBarrier(void *oldp, void *newp, void *volatile *dst) 
{ 
    return (::InterlockedCompareExchangePointer(dst, newp, oldp) == oldp);
}

int32_t OSAtomicAdd32Barrier( int32_t theAmount, volatile int32_t *theValue ) {
    return ::InterlockedAdd(reinterpret_cast<volatile long*>(theValue), theAmount);
}

bool OSAtomicCompareAndSwap32Barrier(int32_t oldValue, int32_t newValue, volatile int32_t *theValue) {
    return (::InterlockedCompareExchange(reinterpret_cast<volatile long*>(theValue), newValue, oldValue) == oldValue);
}

bool OSAtomicCompareAndSwap64Barrier(int64_t oldValue, int64_t newValue, volatile int64_t *theValue) {
    return (::InterlockedCompareExchange64(theValue, newValue, oldValue) == oldValue);
}

int32_t OSAtomicDecrement32Barrier(volatile int32_t *dst)
{
    return OSAtomicAdd32Barrier(-1, dst);
}

int32_t OSAtomicIncrement32Barrier(volatile int32_t *dst)
{
    return OSAtomicAdd32Barrier(1, dst);
}

int32_t OSAtomicAdd32( int32_t theAmount, volatile int32_t *theValue ) {
    return OSAtomicAdd32Barrier(theAmount, theValue);
}

int32_t OSAtomicIncrement32(volatile int32_t *theValue) {
    return OSAtomicIncrement32Barrier(theValue);
}

int32_t OSAtomicDecrement32(volatile int32_t *theValue) {
    return OSAtomicDecrement32Barrier(theValue);
}

void OSMemoryBarrier() {
    MemoryBarrier();
}

#endif // DEPLOYMENT_TARGET_WINDOWS

#pragma mark -
#pragma mark Linux OSAtomic

#if defined(DEPLOYMENT_TARGET_LINUX) || defined(DEPLOYMENT_TARGET_FREEBSD)

bool OSAtomicCompareAndSwapPtr(void *oldp, void *newp, void *volatile *dst) 
{ 
    return __sync_bool_compare_and_swap(dst, oldp, newp);
}

bool OSAtomicCompareAndSwapLong(long oldl, long newl, long volatile *dst) 
{ 
    return __sync_val_compare_and_swap(dst, oldl, newl);
}

bool OSAtomicCompareAndSwapPtrBarrier(void *oldp, void *newp, void *volatile *dst) 
{ 
    return __sync_bool_compare_and_swap(dst, oldp, newp);
}

int32_t OSAtomicAdd32Barrier( int32_t theAmount, volatile int32_t *theValue ) {
    return __sync_fetch_and_add(theValue, theAmount) + theAmount;
}

bool OSAtomicCompareAndSwap32Barrier(int32_t oldValue, int32_t newValue, volatile int32_t *theValue) {
    return __sync_bool_compare_and_swap(theValue, oldValue, newValue);
}

bool OSAtomicCompareAndSwap64Barrier(int64_t oldValue, int64_t newValue, volatile int64_t *theValue) {
    return __sync_bool_compare_and_swap(theValue, oldValue, newValue);
}

int32_t OSAtomicDecrement32Barrier(volatile int32_t *dst)
{
    return OSAtomicAdd32Barrier(-1, dst);
}

int32_t OSAtomicIncrement32Barrier(volatile int32_t *dst)
{
    return OSAtomicAdd32Barrier(1, dst);
}

int32_t OSAtomicAdd32( int32_t theAmount, volatile int32_t *theValue ) {
    return OSAtomicAdd32Barrier(theAmount, theValue);
}

int32_t OSAtomicIncrement32(volatile int32_t *theValue) {
    return OSAtomicIncrement32Barrier(theValue);
}

int32_t OSAtomicDecrement32(volatile int32_t *theValue) {
    return OSAtomicDecrement32Barrier(theValue);
}

void OSMemoryBarrier() {
    __sync_synchronize();
}

#endif // DEPLOYMENT_TARGET_LINUX

#pragma mark -
#pragma mark Dispatch Replacements

#if !__HAS_DISPATCH__

#include <semaphore.h>

typedef struct _CF_sema_s {
    sem_t sema;
} * _CF_sema_t;

CF_INLINE void _CF_sem_signal(_CF_sema_t s) {
    sem_post(&s->sema);
}

CF_INLINE void _CF_sem_wait(_CF_sema_t s) {
    sem_wait(&s->sema);
}

static void _CF_sem_destroy(_CF_sema_t s) {
    free(s);
}

CF_INLINE pthread_key_t _CF_thread_sem_key() {
    static pthread_key_t key = 0;
    static OSSpinLock lock = OS_SPINLOCK_INIT;
    if (key == 0) {
        OSSpinLockLock(&lock);
        if (key == 0) {
            pthread_key_create(&key, (void (*)(void *))&_CF_sem_destroy);
        }
        OSSpinLockUnlock(&lock);
    }
    return key;
}

CF_INLINE _CF_sema_t _CF_get_thread_semaphore() {
    pthread_key_t key = _CF_thread_sem_key();
    _CF_sema_t s = (_CF_sema_t)pthread_getspecific(key);
    if (s == NULL) {
        s = malloc(sizeof(struct _CF_sema_s));
        pthread_setspecific(key, s);
    }
    return s;
    
}

CF_INLINE void _CF_put_thread_semaphore(_CF_sema_t s) {
    pthread_setspecific(_CF_thread_sem_key(), s);
}

#define CF_DISPATCH_ONCE_DONE ((_CF_dispatch_once_waiter_t)~0l)

typedef struct _CF_dispatch_once_waiter_s {
    volatile struct _CF_dispatch_once_waiter_s *volatile dow_next;
    _CF_sema_t dow_sema;
    pthread_t dow_thread;
} *_CF_dispatch_once_waiter_t;

#if defined(__x86_64__) || defined(__i386__)
#define _CF_hardware_pause() __asm__("pause")
#elif (defined(__arm__) && defined(_ARM_ARCH_7) && defined(__thumb__)) || \
defined(__arm64__)
#define _CF_hardware_pause() __asm__("yield")
#else
#define _CF_hardware_pause() __asm__("")
#endif

void _CF_dispatch_once(dispatch_once_t *predicate, void (^block)(void)) {
    _CF_dispatch_once_waiter_t volatile *vval = (_CF_dispatch_once_waiter_t*)predicate;
    struct _CF_dispatch_once_waiter_s dow = { NULL };
    _CF_dispatch_once_waiter_t tail = &dow, next, tmp;
    _CF_sema_t sema;
    if (__sync_bool_compare_and_swap(vval, NULL, tail)) {
        dow.dow_thread = pthread_self();
        block();
        __sync_synchronize();
        next = (_CF_dispatch_once_waiter_t)__sync_swap((vval), (CF_DISPATCH_ONCE_DONE));
        while (next != tail) {
            while (!(tmp = (_CF_dispatch_once_waiter_t)next->dow_next)) {
                _CF_hardware_pause();
            }
            sema = next->dow_sema;
            next = tmp;
            _CF_sem_signal(sema);
        }
    } else {
        dow.dow_sema = _CF_get_thread_semaphore();
        next = *vval;
        for (;;) {
            if (next == CF_DISPATCH_ONCE_DONE) {
                break;
            }
            if (__sync_bool_compare_and_swap(vval, next, tail, &next)) {
                dow.dow_thread = next->dow_thread;
                dow.dow_next = next;
                _CF_sem_wait(dow.dow_sema);
                break;
            }
        }
        _CF_put_thread_semaphore(dow.dow_sema);
    }
}

#endif

#pragma mark -
#pragma mark Windows and Linux Helpers

#if DEPLOYMENT_TARGET_WINDOWS || DEPLOYMENT_TARGET_LINUX

#include <stdio.h>

CF_PRIVATE int asprintf(char **ret, const char *format, ...) {
    va_list args;
    size_t sz = 1024;
    *ret = (char *) malloc(sz * sizeof(char));
    if (!*ret) return -1;
    va_start(args, format);
    int cnt = vsnprintf(*ret, sz, format, args);
    va_end(args);
    if (cnt < sz - 1) return cnt;
    sz = cnt + 8;
    char *oldret = *ret;
    *ret = (char *) realloc(*ret, sz * sizeof(char));
    if (!*ret && oldret) free(oldret);
    if (!*ret) return -1;
    va_start(args, format);
    cnt = vsnprintf(*ret, sz, format, args);
    va_end(args);
    if (cnt < sz - 1) return cnt;
    free(*ret);
    *ret = NULL;
    return -1;
}

#endif

#if DEPLOYMENT_RUNTIME_SWIFT
#include <sys/socket.h>
CF_EXPORT int32_t _CF_SOCK_STREAM() { return SOCK_STREAM; }
#endif

#if DEPLOYMENT_RUNTIME_SWIFT
#include <fcntl.h>
int _CFOpenFileWithMode(const char *path, int opts, mode_t mode) {
    return open(path, opts, mode);
}
int _CFOpenFile(const char *path, int opts) {
    return open(path, opts);
}
#endif

#if DEPLOYMENT_RUNTIME_SWIFT
CF_PRIVATE char **_CFEnviron(void) {
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED
    return *_NSGetEnviron();
#elif DEPLOYMENT_TARGET_WINDOWS
    return _environ;
#else
    return environ;
#endif
}
#endif

void *_CFReallocf(void *ptr, size_t size) {
#if DEPLOYMENT_TARGET_WINDOWS | DEPLOYMENT_TARGET_LINUX
    void *mem = realloc(ptr, size);
    if (mem == NULL && ptr != NULL && size != 0) {
        free(ptr);
    }
    return mem;
#else
    return reallocf(ptr, size);
#endif
}

// clang-format on