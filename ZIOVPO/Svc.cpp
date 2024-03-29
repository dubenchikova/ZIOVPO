#include <windows.h>
#include <WtsApi32.h>
#include <tchar.h>
#include <strsafe.h>
#include "../sample.h"

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("ZIOVPO")

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

VOID SvcInstall(void);
VOID WINAPI SvcCtrlHandlerEx(DWORD dwCtrl, DWORD dwEventType, LPVOID dwEventData, LPVOID dwContent);
VOID WINAPI SvcMain(DWORD, LPTSTR*);
BOOL StartUiProcessInSession(DWORD wtsSession, DWORD& dwBytes);
VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);

int __cdecl _tmain(int argc, TCHAR* argv[])
{

    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }

    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { (wchar_t*)SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        
    }
}

VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szUnquotedPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    TCHAR szPath[MAX_PATH];
    StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);

    schSCManager = OpenSCManager(
        NULL,
        NULL, 
        SC_MANAGER_ALL_ACCESS);

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    
    schService = CreateService(
        schSCManager,                      
        SVCNAME,
        SVCNAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        szPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL);                     
    if (schService == NULL)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    
    gSvcStatusHandle = RegisterServiceCtrlHandlerEx(SVCNAME, (LPHANDLER_FUNCTION_EX)SvcCtrlHandlerEx, NULL);
    gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE;

    if (!gSvcStatusHandle)
    {
 
        return;
    }

    
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    PWTS_SESSION_INFO wtsSessions;
    PROCESS_INFORMATION processInfo;
    DWORD sessionCount, dwBytes = NULL;
    if (!WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &wtsSessions, &sessionCount)) {
       
        return;
    }
    for (auto i = 0; i < sessionCount; ++i)
        if (wtsSessions[i].SessionId == 0) continue;
        else StartUiProcessInSession(wtsSessions[i].SessionId, dwBytes);

    SvcInit(dwArgc, lpszArgv);
}

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
                
        
    ghSvcStopEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL);   
    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    
    while (1)
    {
        
        WaitForSingleObject(ghSvcStopEvent, INFINITE);

        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        return;
    }
}

VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    
    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

        SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}


VOID WINAPI SvcCtrlHandlerEx(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContent)
{

    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);


        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    case SERVICE_CONTROL_SESSIONCHANGE: {
        DWORD dwBytes;
        WTSSESSION_NOTIFICATION* sessionNotification = static_cast<WTSSESSION_NOTIFICATION*>(lpEventData);
        if (dwEventType == WTS_SESSION_LOGOFF) {
            break;
        }
        else if (dwEventType == WTS_SESSION_LOGON) {
            StartUiProcessInSession(sessionNotification->dwSessionId, dwBytes); //Логика запуска UI
        }
        break;
    }
                                      
    }
}
    BOOL StartUiProcessInSession(DWORD wtsSession, DWORD & dwBytes)
    {
    HANDLE userToken;
    PROCESS_INFORMATION pi{};
    STARTUPINFO si{};
    WCHAR path[] = L"C:\\Windows\\System32\\notepad.exe";
    WTSQueryUserToken(wtsSession, &userToken);
    if (!userToken)
    return FALSE;
    if (!CreateProcessAsUser(userToken, NULL, path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    return FALSE;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
    }