// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "stdafx.h"

#define SVCNAME L"MidiSrv"
#define SVCDISPLAYNAME L"Microsoft Windows MIDI Service"
#define SVCDESCRIPTION L"Microsoft Windows MIDI Services core service for MIDI 1.0 and MIDI 2.0 message routing and device discovery / enumeration"

//#ifdef _DEBUG
//#define SVCDESCRIPTION TEXT("Windows MIDI Services core service (Debug Build)")
//#else
//#define SVCDESCRIPTION TEXT("Windows MIDI Services core service for MIDI 2.0 and MIDI 1.0 protocols")
//#endif


#define SVC_ERROR       ((DWORD)0xC0020001L)

// wait up to 30 seconds for the service to stop for uninstall
#define SERVICE_STOP_TIMEOUT 30000

SERVICE_STATUS          g_SvcStatus; 
SERVICE_STATUS_HANDLE   g_SvcStatusHandle; 
wil::unique_handle      g_SvcStopEvent;
std::unique_ptr<CMidiSrv> g_MidiService;

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
//
VOID ReportSvcStatus(
    _In_ DWORD dwCurrentState,
    _In_ DWORD dwWin32ExitCode,
    _In_ DWORD dwWaitHint)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );


    static DWORD checkPoint = 1;

    // Fill in the SERVICE_STATUS structure.
    g_SvcStatus.dwCurrentState = dwCurrentState;
    g_SvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_SvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
    {
        g_SvcStatus.dwControlsAccepted = 0;
    }
    else
    {
        g_SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }

    if ((dwCurrentState == SERVICE_RUNNING) || 
        (dwCurrentState == SERVICE_STOPPED))
    {
        g_SvcStatus.dwCheckPoint = 0;
    }
    else
    {
        g_SvcStatus.dwCheckPoint = checkPoint++;
    }

    // Report the status of the service to the SCM.
    if (!SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus))
    {
        LOG_LAST_ERROR_MSG("Failed to set the service status.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Failed to set the service status.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }
}

//
// Purpose: 
//   Installs a service in the SCM database
//
VOID SvcInstall()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );

    wil::unique_schandle serviceControlManager;
    wil::unique_schandle service;
    TCHAR unquotedPath[MAX_PATH] = {0};
    TCHAR quotedPath[MAX_PATH] = {0};
    HRESULT hr = S_OK;

    if (!GetModuleFileName(NULL, unquotedPath, MAX_PATH))
    {
        LOG_LAST_ERROR_MSG("Cannot install service. Failed to retrieve module file name.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot install service. Failed to retrieve module file name.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    // In case the path contains a space, it must be quoted
    hr = StringCbPrintf(quotedPath, MAX_PATH, TEXT("\"%s\""), unquotedPath);
    if (FAILED(hr))
    {
        LOG_HR_MSG(hr, "Cannot install service. Failed to compose quoted path.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot install service. Failed to compose quoted path.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    // Get a handle to the SCM database. 
    serviceControlManager.reset(OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!serviceControlManager)
    {
        LOG_LAST_ERROR_MSG("Cannot install service. OpenSCManager failed. This must be run as Administrator.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot install service. OpenSCManager failed. This must be run as Administrator.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    // Create the service
    service.reset(CreateService( 
        serviceControlManager.get(),
        SVCNAME,
        SVCDISPLAYNAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,           // SERVICE_AUTO_START
        SERVICE_ERROR_NORMAL,
        quotedPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL));

    if (!service)
    {
        LOG_LAST_ERROR_MSG("Cannot install service. CreateService failed.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot install service. CreateService failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    // Set the service description so it's not just blank. This should be localized
    // with "@[path]dllname,-strID" and a .rc file

    std::wstring serviceDescription{ SVCDESCRIPTION  };
     
    SERVICE_DESCRIPTION info{ };
    info.lpDescription = (LPWSTR)(serviceDescription.c_str());

    if (!ChangeServiceConfig2(service.get(), SERVICE_CONFIG_DESCRIPTION, &info))
    {
        // non-fatal
        LOG_LAST_ERROR_MSG("Changing service description failed.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Changing service description failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );
    }




    SERVICE_TRIGGER_INFO triggerInfo{ 0 };
    SERVICE_TRIGGER trigger{ 0 };
    SERVICE_TRIGGER_SPECIFIC_DATA_ITEM triggerData{ 0 };


 // tried the RPC trigger and just couldn't get it to work
#ifdef USE_RPC_DEMAND_TRIGGER
    // Set the RPC call to triggers waking up this service
    std::wstring rpcGuidString{ L"64839251-9daf-4e79-aa4e-9771c86ffbc1" }; // RPC Interface. should use __uuidof

    triggerData.dwDataType = SERVICE_TRIGGER_DATA_TYPE_STRING;
    triggerData.pData = (PBYTE)(rpcGuidString.c_str());
    triggerData.cbData = (DWORD)(rpcGuidString.length() + 1) * sizeof(wchar_t);

    auto triggerSubType = RPC_INTERFACE_EVENT_GUID;

    trigger.dwAction = SERVICE_TRIGGER_ACTION_SERVICE_START;
    trigger.dwTriggerType = SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT;
    trigger.pTriggerSubtype = (LPGUID)&triggerSubType;
    trigger.pDataItems = &triggerData;
    trigger.cDataItems = 1;

    triggerInfo.pTriggers = &trigger;
    triggerInfo.cTriggers = 1;
#else
    // use ETW trigger. This worked right away
    // The problem with the ETW trigger is there's a bit of a delay, so need to wait on it
    // 6e9d2090-31a4-5a2c-da35-fd606d7d6ac3
    GUID midiSrvAbstractionEtwProvider{ 0x6e9d2090, 0x31a4, 0x5a2c, 0xda, 0x35, 0xfd, 0x60, 0x6d, 0x7d, 0x6a, 0xc3 };
    
    //triggerData.dwDataType = SERVICE_TRIGGER_DATA_TYPE_STRING;
    //triggerData.pData = (PBYTE)(&midiSrvAbstractionEtwProvider);
    //triggerData.cbData = (DWORD)(sizeof(GUID));

    trigger.dwAction = SERVICE_TRIGGER_ACTION_SERVICE_START;
    trigger.dwTriggerType = SERVICE_TRIGGER_TYPE_CUSTOM;
    trigger.pTriggerSubtype = (LPGUID)&midiSrvAbstractionEtwProvider;
    //trigger.pDataItems = &triggerData;
    trigger.cDataItems = 0;

    triggerInfo.pTriggers = &trigger;
    triggerInfo.cTriggers = 1;
#endif


    // Should we also trigger on SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL? We are the 
    // interface enumerator so I assume it would trigger on *all* audio devices, which would 
    // be pointless?
    // 
    // Also, there are situations when the system may have only network or BLE or something
    // MIDI devices, so then it would make no sense as it would trigger only for USB.
    //
    // So triggering on RPC call. Musicians are likely going to want to change the service
    // to automatic start, so I'll put an option in the settings app to enable that
    // for them. Otherwise, there's a delay between the service spinning up and all
    // devices being enumerated (and protocol negotiation complete)

    if (!ChangeServiceConfig2(service.get(), SERVICE_CONFIG_TRIGGER_INFO, &triggerInfo))
    {
        // non-fatal
        LOG_LAST_ERROR_MSG("Changing service trigger failed.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Changing service trigger failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    // TODO: Have this called only if a reg key is present. The settings app will write that key
    //if (!StartService(service.get(), 0, nullptr))
    //{
    //    LOG_LAST_ERROR_MSG("Starting the service failed during service install.");
    //    return;
    //}

    //printf("\n\rMidiSrv: Service installed.\n\r");

    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingWideString(L"Service install complete.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
    );
}

//
// Purpose: 
//   Stops and uninstalls midisrv
//
VOID SvcUninstall()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );

    wil::unique_schandle serviceControlManager;
    wil::unique_schandle service;
    SERVICE_STATUS_PROCESS status{0};
    DWORD size_needed{0};

    // Get a handle to the SCM database. 
    serviceControlManager.reset(OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS));
    if (!serviceControlManager)
    {
        LOG_LAST_ERROR_MSG("Cannot uninstall service. OpenSCManager failed");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot uninstall service. OpenSCManager failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    service.reset(OpenService(serviceControlManager.get(), SVCNAME, SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE));
    if (!service)
    {
        LOG_LAST_ERROR_MSG("Cannot uninstall service. OpenService failed");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot uninstall service. OpenService failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    if (FALSE == QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &size_needed))
    {
        LOG_LAST_ERROR_MSG("Cannot uninstall service. QueryServiceStatusEx failed");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot uninstall service. QueryServiceStatusEx failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    // if the service is already stopped, all that is needed is to delete.
    if (status.dwCurrentState != SERVICE_STOPPED)
    {
        SERVICE_STATUS ss{0};

        if (FALSE == ControlService(service.get(), SERVICE_CONTROL_STOP, &ss))
        {
            LOG_LAST_ERROR_MSG("Cannot uninstall service. Service is not stopped and SERVICE_CONTROL_STOP failed");

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Cannot uninstall service. Service is not stopped and SERVICE_CONTROL_STOP failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

            return;
        }

        ULONGLONG startTime = GetTickCount64();
        ULONGLONG currentTime = startTime;
        do
        {
            Sleep(500);

            currentTime = GetTickCount64();

            memset(&status, 0, sizeof(status));
            size_needed = 0;
            if (FALSE == QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&status), sizeof(status), &size_needed))
            {
                LOG_LAST_ERROR_MSG("Cannot uninstall service. QueryServiceStatusEx after SERVICE_CONTROL_STOP failed.");

                TraceLoggingWrite(
                    MidiSrvTelemetryProvider::Provider(),
                    MIDI_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingWideString(L"Cannot uninstall service. QueryServiceStatusEx after SERVICE_CONTROL_STOP failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
                );

                return;
            }
        }
        while (status.dwCurrentState != SERVICE_STOPPED && (currentTime - startTime < SERVICE_STOP_TIMEOUT));
        
        if (status.dwCurrentState != SERVICE_STOPPED)
        {
            LOG_LAST_ERROR_MSG("Service did not stop during uninstall. Calling DeleteService anyway, but deletion will pend.");

            TraceLoggingWrite(
                MidiSrvTelemetryProvider::Provider(),
                MIDI_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Service did not stop during uninstall. Calling DeleteService anyway, but deletion will pend.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
            );

        }
    }

    // Service is stopped, delete it from the database
    if (FALSE == DeleteService(service.get()))
    {
        LOG_LAST_ERROR_MSG("Cannot uninstall service. DeleteService failed.");

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot uninstall service. DeleteService failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    //printf("\n\rMidiSrv: Service uninstalled.\n\r");
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
//
VOID SvcInit()
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );

    // service control event
    g_SvcStopEvent.reset(CreateEvent(NULL, TRUE, FALSE, NULL));
    if (NULL == g_SvcStopEvent)
    {
        LOG_LAST_ERROR_MSG("Cannot init service. stop event creation failed.");
        ReportSvcStatus( SERVICE_STOPPED, GetLastError(), 0 );

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"Cannot init service. stop event creation failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );


        return;
    }

    g_MidiService = std::make_unique<CMidiSrv>();
    if (NULL == g_MidiService.get())
    {
        LOG_IF_NULL_ALLOC(g_MidiService);
        ReportSvcStatus( SERVICE_STOPPED, ERROR_NOT_ENOUGH_MEMORY, 0 );

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"g_MidiService is nullptr.", MIDI_TRACE_EVENT_MESSAGE_FIELD)
        );

        return;
    }

    HRESULT hr = g_MidiService->Initialize();
    if (FAILED(hr))
    {
        LOG_IF_FAILED(hr);
        ReportSvcStatus( SERVICE_STOPPED, hr, 0 );

        TraceLoggingWrite(
            MidiSrvTelemetryProvider::Provider(),
            MIDI_TRACE_EVENT_ERROR,
            TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
            TraceLoggingWideString(L"MidiService initialization failed.", MIDI_TRACE_EVENT_MESSAGE_FIELD),
            TraceLoggingHResult(hr, MIDI_TRACE_EVENT_HRESULT_FIELD)
        );

        return;
    }

    // Report running status when initialization is complete.
    ReportSvcStatus( SERVICE_RUNNING, NO_ERROR, 0 );

    // Check whether to stop the service.
    WaitForSingleObject(g_SvcStopEvent.get(), INFINITE);

    hr = g_MidiService->Cleanup();
    LOG_IF_FAILED(hr);
    ReportSvcStatus( SERVICE_STOPPED, hr, 0 );

    return;
}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
VOID SvcReportEvent(_In_z_ LPCSTR )
{ 
// TODO: create service providers using public api's rather than
// event logs.
#if 0
    wil::unique_any_handle_null<decltype(&::DeregisterEventSource), ::DeregisterEventSource> 
        eventSource(RegisterEventSource(NULL, SVCNAME));

    LOG_LAST_ERROR_MSG(Operation);

    if (!eventSource)
    {
        LOG_LAST_ERROR_MSG("Failed to register event source");
    }
    else
    {
        LPCTSTR lpszStrings[2];
        TCHAR Buffer[80];

        StringCchPrintf(Buffer, 80, TEXT("%S failed with %d"), Operation, GetLastError());

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEvent(eventSource.get(), EVENTLOG_ERROR_TYPE, 0, SVC_ERROR, NULL, 2, 0, lpszStrings, NULL);
    }
#endif
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
//
VOID WINAPI SvcCtrlHandler(_In_ DWORD Ctrl)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingUInt32(Ctrl, "ctrl")
    );

   // Handle the requested control code. 
   switch(Ctrl) 
   {  
      case SERVICE_CONTROL_STOP: 
         ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

         // Signal the service to stop.
         SetEvent(g_SvcStopEvent.get());

         ReportSvcStatus(g_SvcStatus.dwCurrentState, NO_ERROR, 0);
         return;
      case SERVICE_CONTROL_INTERROGATE:
         break;
      default:
         break;
   }
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
//
VOID WINAPI SvcMain(_In_ DWORD , _In_ LPTSTR *)
{
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );

    // Register the handler function for the service
    g_SvcStatusHandle = RegisterServiceCtrlHandler(SVCNAME, SvcCtrlHandler);
    if (!g_SvcStatusHandle)
    {
        SvcReportEvent("RegisterServiceCtrlHandler");
        return; 
    } 

    // These SERVICE_STATUS members remain as set here
    g_SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
    g_SvcStatus.dwServiceSpecificExitCode = 0;    

    // Report initial status to the SCM
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.
    SvcInit();
}

int __cdecl _tmain(_In_ int ArgC, _In_reads_(ArgC) TCHAR *ArgV[]) 
{ 
    TraceLoggingWrite(
        MidiSrvTelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO)
    );

    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.
    wil::SetResultTelemetryFallback(MidiSrvTelemetryProvider::FallbackTelemetryCallback);

    winrt::init_apartment();

    if (ArgC >= 2 && lstrcmpi(ArgV[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }

    if (ArgC >= 2 && lstrcmpi(ArgV[1], TEXT("uninstall")) == 0)
    {
        SvcUninstall();
        return 0;
    }

    SERVICE_TABLE_ENTRY DispatchTable[] = 
    { 
        { (LPWSTR) SVCNAME, (LPSERVICE_MAIN_FUNCTION) SvcMain }, 
        { NULL, NULL } 
    }; 
 
    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.
    if (!StartServiceCtrlDispatcher(DispatchTable)) 
    { 
        SvcReportEvent("StartServiceCtrlDispatcher");
    }
    return 0;
} 

