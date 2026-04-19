// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "MidiSrvPorts.h"
#include <ntverp.h>
#include <ks.h>

#include "Feature_Servicing_MIDI2DevCaps2.h"
#include "Feature_Servicing_MIDI2NumDevsPerf.h"

using unique_hdevinfo = wil::unique_any_handle_invalid<decltype(&::SetupDiDestroyDeviceInfoList), ::SetupDiDestroyDeviceInfoList>;

// via https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_WDMAUD2 ((HINSTANCE)&__ImageBase)

DWORD CMEventCallback
(
    HCMNOTIFICATION,
    PVOID Context,
    CM_NOTIFY_ACTION  Action,
    PCM_NOTIFY_EVENT_DATA EventData,
    DWORD EventDataSize
)
{
    // context must be valid, this must be for a midi in or midi out device interface,
    // it must be an arrival or removal, and the notify event data must be valid.
    if (Context != nullptr &&
        EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE &&
        (EventData->u.DeviceInterface.ClassGuid == DEVINTERFACE_MIDI_OUTPUT || EventData->u.DeviceInterface.ClassGuid == DEVINTERFACE_MIDI_INPUT) &&
        (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL || Action == CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL) &&
        EventDataSize >= sizeof(CM_NOTIFY_EVENT_DATA))
    {
        CMidiPorts * pThis = (CMidiPorts *) Context;
        MidiFlow flow = (EventData->u.DeviceInterface.ClassGuid == DEVINTERFACE_MIDI_OUTPUT)?MidiFlowOut:MidiFlowIn;
        bool isArrival = (Action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL);
        pThis->MidiInterfaceChange(flow, isArrival, EventData->u.DeviceInterface.SymbolicLink);
    }

    return 0;
}

HRESULT
CMidiPorts::RegisterNotifications()
{
    CONFIGRET cr;
    CM_NOTIFY_FILTER notifyFilter = {};

    auto lock = m_Lock.lock();

    notifyFilter.cbSize = sizeof(notifyFilter);
    notifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    notifyFilter.u.DeviceInterface.ClassGuid = DEVINTERFACE_MIDI_OUTPUT;
    cr = CM_Register_Notification(&notifyFilter, this, &CMEventCallback, &m_NotifyMidiOut);
    if (cr != CR_SUCCESS)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)));
    }
    LOG_IF_FAILED(RefreshPortsForFlow(MidiFlowOut));

    notifyFilter.cbSize = sizeof(notifyFilter);
    notifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    notifyFilter.u.DeviceInterface.ClassGuid = DEVINTERFACE_MIDI_INPUT;
    cr = CM_Register_Notification(&notifyFilter, this, &CMEventCallback, &m_NotifyMidiIn);
    if (cr != CR_SUCCESS)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)));
    }
    LOG_IF_FAILED(RefreshPortsForFlow(MidiFlowIn));

    return S_OK;
}

HRESULT
CMidiPorts::MidiInterfaceChange
(
    MidiFlow Flow,
    bool IsArrival,
    WCHAR *SymbolicLink
)
{
    std::wstring notifiedInterface = WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(SymbolicLink);

    auto lock = m_Lock.lock();

    // search the flow for the interface id
    for (auto const& [portNum, port] : m_MidiPortInfo[Flow])
    {
        if (port.InterfaceId == notifiedInterface)
        {
            // if found and is removal, refresh to remove,
            // else it's already present so noop.
            if (!IsArrival)
            {
                RETURN_IF_FAILED(RefreshPortsForFlow(Flow));

                for (auto const& [portHandle, openPort] : m_OpenPorts)
                {
                    openPort->NotifyInterfaceRemoval(notifiedInterface);
                }
            }

            // it was found, so we are finished (there will only ever be 1
            // instance of an interface id in the map)
            return S_OK;
        }
    }

    // if not found and is arrival, refresh to add,
    // else it's already removed so noop
    if (IsArrival)
    {
        RETURN_IF_FAILED(RefreshPortsForFlow(Flow));
    }

    return S_OK;
}

CMidiPorts::CMidiPorts()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));
}

CMidiPorts::~CMidiPorts()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(g_ProcessIsTerminating, "ProcessIsTerminating"));

    if (!g_ProcessIsTerminating)
    {
        Shutdown();
    }
    else
    {
        // unsafe to release COM objects while process is terminating.
        static_cast<void>(m_MidisrvTransport.reset());
    }
}

HRESULT
CMidiPorts::RuntimeClassInitialize()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),        
        TraceLoggingUInt32(GetCurrentProcessId(), "Process id")
        );

    RETURN_IF_FAILED(UuidCreate(&m_SessionId));


    WCHAR sessionName[MIDI_MAX_SESSION_NAME_LENGTH];
    ::LoadStringW(HINST_WDMAUD2, IDS_MIDI_DEFAULT_WINMM_SESSION_NAME, sessionName, MIDI_MAX_SESSION_NAME_LENGTH);
    m_SessionName = sessionName;

    std::unique_ptr<CMidi2MidiSrv> midiSrv(new (std::nothrow) CMidi2MidiSrv());
    RETURN_IF_NULL_ALLOC(midiSrv);

    RETURN_IF_FAILED(midiSrv->Initialize());
    m_MidisrvTransport = std::move(midiSrv);

    RETURN_IF_FAILED(m_MidisrvTransport->AddClientSession(m_SessionId, m_SessionName.c_str()));

    if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
    {
        RETURN_IF_FAILED(RegisterNotifications());
    }

    return S_OK;        
}

HRESULT
CMidiPorts::Shutdown()
{
    TraceLoggingWrite(
        WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"));

    if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
    {
        // first, shut down notifications, as it may hold the lock
        m_NotifyMidiOut.reset();
        m_NotifyMidiIn.reset();
    }

    auto lock = m_Lock.lock();

    m_MidiPortInfo[MidiFlowIn].clear();
    m_MidiPortInfo[MidiFlowOut].clear();

    for (auto const& port : m_OpenPorts)
    {
        port.second->Shutdown();
    }
    m_OpenPorts.clear();

    if (m_MidisrvTransport)
    {
        m_MidisrvTransport->RemoveClientSession(m_SessionId);
        m_MidisrvTransport.reset();
    }

    return S_OK;
}

_Use_decl_annotations_
DWORD APIENTRY 
CMidiPorts::MidMessage(UINT deviceID, UINT msg, DWORD_PTR user, DWORD_PTR param1, DWORD_PTR param2)
{
    HRESULT hr = S_OK;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(deviceID, "deviceID"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(user, "user"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    switch(msg)
    {
        case MIDM_GETNUMDEVS:
            {
                if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
                {
                    auto lock = m_Lock.lock();
                    UINT32 deviceCount = m_MidiPortCount[MidiFlowIn];
                    return (MAKELONG(deviceCount, MMSYSERR_NOERROR));
                }
                else
                {
                    UINT32 deviceCount{ };
                    hr = GetMidiDeviceCount(MidiFlowIn, deviceCount);
                    if (SUCCEEDED(hr))
                    {
                        return (MAKELONG(deviceCount, MMSYSERR_NOERROR));
                    }
                }
            }
            break;
        case MIDM_GETDEVCAPS:
            hr = GetDevCaps(MidiFlowIn, deviceID, param1, param2);
            break;
        case MIDM_OPEN:
            hr = Open(MidiFlowIn, deviceID, (MIDIOPENDESC *) param1, param2, (MidiPortHandle*) user);
            break;
        case MIDM_CLOSE:
            // Forward the message to the open port, if one exists, giving the port an opportunity to
            // communicate that it can not be closed.
            if (SUCCEEDED(hr = ForwardMidMessage(msg, (MidiPortHandle) user, param1, param2)))
            {
                hr = Close(MidiFlowIn, (MidiPortHandle) user);
            }
            break;
        case DRV_QUERYDEVICEINTERFACE:
        case DRV_QUERYDEVICEINTERFACESIZE:
            hr = GetDeviceInterface(MidiFlowIn, deviceID, msg, param1, param2);
            break;
        default:
            hr = ForwardMidMessage(msg, (MidiPortHandle) user, param1, param2);
    }

    return MMRESULT_FROM_HRESULT(hr);
}

_Use_decl_annotations_
DWORD APIENTRY
CMidiPorts::ModMessage(UINT deviceID, UINT msg, DWORD_PTR user, DWORD_PTR param1, DWORD_PTR param2)
{
    HRESULT hr = S_OK;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(deviceID, "deviceID"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(user, "user"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    switch(msg)
    {
        case MODM_GETNUMDEVS:
            {
                if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
                {
                    auto lock = m_Lock.lock();
                    UINT32 deviceCount = m_MidiPortCount[MidiFlowOut];
                    return (MAKELONG(deviceCount, MMSYSERR_NOERROR));
                }
                else
                {
                    UINT32 deviceCount{ };
                    hr = GetMidiDeviceCount(MidiFlowOut, deviceCount);
                    if (SUCCEEDED(hr))
                    {
                        return (MAKELONG(deviceCount, MMSYSERR_NOERROR));
                    }
                }
            }
            break;
        case MODM_GETDEVCAPS:
            hr = GetDevCaps(MidiFlowOut, deviceID, param1, param2);
            break;
        case MODM_OPEN:
            hr = Open(MidiFlowOut, deviceID, (MIDIOPENDESC *) param1, param2, (MidiPortHandle*) user);
            break;
        case MODM_CLOSE:
            // Forward the message to the open port, if one exists, giving the port an opportunity to
            // communicate that it can not be closed.
            if (SUCCEEDED(hr = ForwardModMessage(msg, (MidiPortHandle) user, param1, param2)))
            {
                hr = Close(MidiFlowOut, (MidiPortHandle) user);
            }
            break;
        case DRV_QUERYDEVICEINTERFACE:
        case DRV_QUERYDEVICEINTERFACESIZE:
            hr = GetDeviceInterface(MidiFlowOut, deviceID, msg, param1, param2);
            break;
        default:
            hr = ForwardModMessage(msg, (MidiPortHandle) user, param1, param2);
    }

    return MMRESULT_FROM_HRESULT(hr);
}

// Start remove with Feature_Servicing_MIDI2NumDevsPerf
_Use_decl_annotations_
HRESULT
CMidiPorts::GetMidiDeviceCount(MidiFlow flow, UINT32& count)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"));

    count = 0;
    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlowIn && flow != MidiFlowOut);

    auto lock = m_Lock.lock();

    // We return the largest valid port number here, the client
    // is then responsible to retrieve the dev caps for these ports
    // to determine which ports are present at runtime.
    UINT highestPortNumber {0};

    // throw away old structure for this flow
    // build up new structure of all active devices for the requested flow, return
    // maximum port number, our port numbers are from 1->, max port number is 0->
    // (because port 0 is reserved for the synth, which will eventually be in midisrv)           
    m_MidiPortInfo[flow].clear();
    m_MidiPortCount[flow] = 0;

    SP_DEVINFO_DATA device = { sizeof(SP_DEVINFO_DATA) };
    const GUID* interfaceCategory = (flow == MidiFlowOut) ? &DEVINTERFACE_MIDI_OUTPUT : &DEVINTERFACE_MIDI_INPUT;
    auto devInfo = unique_hdevinfo{ SetupDiGetClassDevs(interfaceCategory, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT) };
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !devInfo);

    for (DWORD deviceIndex = 0; SetupDiEnumDeviceInfo(devInfo.get(), deviceIndex, &device); deviceIndex++)
    {
        SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { sizeof(SP_DEVICE_INTERFACE_DATA) };
        for (DWORD deviceInterfaceIndex = 0; SetupDiEnumDeviceInterfaces(devInfo.get(), &device, interfaceCategory, deviceInterfaceIndex, &deviceInterfaceData); deviceInterfaceIndex++ )
        {
            DEVPROPTYPE propType = DEVPROP_TYPE_NULL;
            WCHAR deviceName[MAXPNAMELEN] = {0};
            DWORD servicePortNum {0};
            DWORD requiredSize {0};
            WCHAR deviceDriverInterfaceId[MAX_PATH] = {0};
            std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA> interfaceDetailData;
            KSCOMPONENTID ksComponentId {0};
            DWORD ksComponentIdSize {0};

            // retrieve the device interface id string
            if (!SetupDiGetDeviceInterfaceDetail(
                devInfo.get(),
                &deviceInterfaceData,
                nullptr,
                0,
                &requiredSize,
                nullptr))
            {
                if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
                {
                    interfaceDetailData.reset((PSP_DEVICE_INTERFACE_DETAIL_DATA)(new (std::nothrow) BYTE[requiredSize]));
                    RETURN_IF_NULL_ALLOC(interfaceDetailData);
                    memset(interfaceDetailData.get(), 0, requiredSize);
                    interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
                    SetupDiGetDeviceInterfaceDetail(
                                    devInfo.get(),
                                    &deviceInterfaceData,
                                    interfaceDetailData.get(),
                                    requiredSize,
                                    nullptr,
                                    nullptr);
                }
            }
            if (nullptr == interfaceDetailData)
            {
                continue;
            }

            // retrieve the assigned port number for this interface
            // if not present, then this may be the synth, which currently
            // doesn't go through midisrv.
            if (!SetupDiGetDeviceInterfaceProperty(
                devInfo.get(),
                &deviceInterfaceData,
                &PKEY_MIDI_ServiceAssignedPortNumber,
                &propType,
                (PBYTE) &servicePortNum,
                sizeof(DWORD),
                nullptr,
                0))
            {
                continue;
            }
            if (propType != DEVPROP_TYPE_UINT32)
            {
                continue;
            }

            // retrieve the friendly name for the port
            if (!SetupDiGetDeviceInterfaceProperty(
                devInfo.get(),
                &deviceInterfaceData,
                &DEVPKEY_DeviceInterface_FriendlyName,
                &propType,
                (PBYTE) &deviceName,
                sizeof(deviceName),
                &requiredSize,
                0))
            {
                continue;
            }
            if (propType != DEVPROP_TYPE_STRING ||
                requiredSize < sizeof(WCHAR))
            {
                continue;
            }

            // to support DRV_QUERYDEVICEINTERFACE
            if (!SetupDiGetDeviceInterfaceProperty(
                devInfo.get(),
                &deviceInterfaceData,
                &PKEY_MIDI_DriverDeviceInterface,
                &propType,
                (PBYTE)&deviceDriverInterfaceId,
                sizeof(deviceDriverInterfaceId),
                &requiredSize,
                0))
            {
                continue;
            }
            if (propType != DEVPROP_TYPE_STRING ||
                requiredSize < sizeof(WCHAR))
            {
                continue;
            }

            if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
            {
                // Retrieve the KS component id information, if available,
                // from the SWD.
                if (SetupDiGetDeviceInterfaceProperty(
                    devInfo.get(),
                    &deviceInterfaceData,
                    &PKEY_MIDI_KsComponentId,
                    &propType,
                    (PBYTE) &ksComponentId,
                    sizeof(KSCOMPONENTID),
                    &ksComponentIdSize,
                    0))
                {
                    if (propType != DEVPROP_TYPE_BINARY ||
                        ksComponentIdSize != sizeof(KSCOMPONENTID))
                    {
                        continue;
                    }
                }
            }

            // our port numbers start with 1 because they're the "global" port numbers
            // that the user should see and port 0 is reserved for the synth.
            // So, a service port number of 1 indicates that we have 1 port, and so on.
            // This means that the port number count is simply the largest port number
            // we are aware of.
            if (servicePortNum > highestPortNumber)
            {
                highestPortNumber = servicePortNum;
            }

            // save the port information to the array.
            m_MidiPortInfo[flow][servicePortNum].PortNumber = servicePortNum;
            m_MidiPortInfo[flow][servicePortNum].Name = deviceName;
            m_MidiPortInfo[flow][servicePortNum].InterfaceId = interfaceDetailData->DevicePath;
            m_MidiPortInfo[flow][servicePortNum].DriverDeviceInterfaceId = WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(deviceDriverInterfaceId);

            if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
            {
                WORD wMid {MM_MICROSOFT};
                WORD wPid = (flow == MidiFlowOut)?MM_MSFT_GENERIC_MIDIOUT:MM_MSFT_GENERIC_MIDIIN;
                MMVERSION vDriverVersion {0x0100};
                GUID manufacturerGuid {0};
                GUID productGuid {0};
                GUID nameGuid {0};

                INIT_MMREG_MID( &manufacturerGuid, wMid );
                INIT_MMREG_PID( &productGuid, wPid );

                if (ksComponentIdSize > 0)
                {
                    // Legacy kscomponentid information is available, default to wdmaudio midi in/out
                    // pid, in the event the pid provided by the driver is not compatible,
                    // and the legacy driver versioning, in the even the provided version isn't
                    // compatible.
                    //
                    // This is to as closely as possible match what wdmaud returned for these
                    // drivers, which apps have come to depend upon.
                    wPid = (flow == MidiFlowOut)?MM_MSFT_WDMAUDIO_MIDIOUT:MM_MSFT_WDMAUDIO_MIDIIN;
                    vDriverVersion = MAKEWORD(VER_PRODUCTMINORVERSION, VER_PRODUCTMAJORVERSION);

                    manufacturerGuid = ksComponentId.Manufacturer;
                    productGuid = ksComponentId.Product;
                    nameGuid = ksComponentId.Name;
                
                    if (IS_COMPATIBLE_MMREG_MID(&ksComponentId.Manufacturer))
                    {
                        wMid = EXTRACT_MMREG_MID(&ksComponentId.Manufacturer);
                    }
                    
                    if (IS_COMPATIBLE_MMREG_PID(&ksComponentId.Product))
                    {
                        wPid = EXTRACT_MMREG_PID(&ksComponentId.Product);
                    }
                
                    if ((ksComponentId.Version < 256) && (ksComponentId.Revision < 256))
                    {
                        vDriverVersion = MAKEWORD(ksComponentId.Revision, ksComponentId.Version);
                    }
                }

                // Fill in the midiCaps for this port
                if (flow == MidiFlowOut)
                {
                    MIDIOUTCAPS2W *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiOutCaps);
                
                    caps->wMid = wMid;
                    caps->wPid = wPid;
                    caps->vDriverVersion = vDriverVersion;
                    caps->ManufacturerGuid = manufacturerGuid;
                    caps->ProductGuid = productGuid;
                    caps->NameGuid = nameGuid;
                
                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->wTechnology = MOD_MIDIPORT;
                    caps->wVoices = 0;
                    caps->wNotes = 0;
                    caps->wChannelMask = 0xFFFF;
                    caps->dwSupport = 0;
                }
                else
                {
                    MIDIINCAPS2W *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiInCaps);
                
                    caps->wMid = wMid;
                    caps->wPid = wPid;
                    caps->vDriverVersion = vDriverVersion;
                    caps->ManufacturerGuid = manufacturerGuid;
                    caps->ProductGuid = productGuid;
                    caps->NameGuid = nameGuid;

                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->dwSupport = 0;
                }
            }
            else
            {
                // Fill in the midiCaps for this port
                if (flow == MidiFlowOut)
                {
                    MIDIOUTCAPSW *caps = (MIDIOUTCAPSW*) &(m_MidiPortInfo[flow][servicePortNum].MidiOutCaps);
                
                    caps->wMid = MM_MICROSOFT;
                    caps->wPid = MM_MSFT_GENERIC_MIDIOUT;
                    caps->vDriverVersion = 0x0100;
                
                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->wTechnology = MOD_MIDIPORT;
                    caps->wVoices = 0;
                    caps->wNotes = 0;
                    caps->wChannelMask = 0xFFFF;
                    caps->dwSupport = 0;
                }
                else
                {
                    MIDIINCAPSW *caps = (MIDIINCAPSW*) &(m_MidiPortInfo[flow][servicePortNum].MidiInCaps);
                
                    caps->wMid = MM_MICROSOFT;
                    caps->wPid = MM_MSFT_GENERIC_MIDIIN;
                    caps->vDriverVersion = 0x0100;

                    //wcsncpy_s(caps->szPname, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), MAXPNAMELEN);
                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->dwSupport = 0;
                }
            }
        }
    }

    // save and return the highest port number
    m_MidiPortCount[flow] = highestPortNumber;
    count = highestPortNumber;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(count, "count"));

    return S_OK;
}
// End remove with Feature_Servicing_MIDI2NumDevsPerf

_Use_decl_annotations_
HRESULT
CMidiPorts::RefreshPortsForFlow(MidiFlow flow)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"));

    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlowIn && flow != MidiFlowOut);

    // We return the largest valid port number here, the client
    // is then responsible to retrieve the dev caps for these ports
    // to determine which ports are present at runtime.
    UINT highestPortNumber {0};

    // throw away old structure for this flow
    // build up new structure of all active devices for the requested flow, return
    // maximum port number, our port numbers are from 1->, max port number is 0->
    // (because port 0 is reserved for the synth, which will eventually be in midisrv)           
    m_MidiPortInfo[flow].clear();
    m_MidiPortCount[flow] = 0;

    SP_DEVINFO_DATA device = { sizeof(SP_DEVINFO_DATA) };
    const GUID* interfaceCategory = (flow == MidiFlowOut) ? &DEVINTERFACE_MIDI_OUTPUT : &DEVINTERFACE_MIDI_INPUT;
    auto devInfo = unique_hdevinfo{ SetupDiGetClassDevs(interfaceCategory, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT) };
    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), !devInfo);

    for (DWORD deviceIndex = 0; SetupDiEnumDeviceInfo(devInfo.get(), deviceIndex, &device); deviceIndex++)
    {
        SP_DEVICE_INTERFACE_DATA deviceInterfaceData = { sizeof(SP_DEVICE_INTERFACE_DATA) };
        for (DWORD deviceInterfaceIndex = 0; SetupDiEnumDeviceInterfaces(devInfo.get(), &device, interfaceCategory, deviceInterfaceIndex, &deviceInterfaceData); deviceInterfaceIndex++ )
        {
            DEVPROPTYPE propType = DEVPROP_TYPE_NULL;
            WCHAR deviceName[MAXPNAMELEN] = {0};
            DWORD servicePortNum {0};
            DWORD requiredSize {0};
            WCHAR deviceDriverInterfaceId[MAX_PATH] = {0};
            std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA> interfaceDetailData;
            KSCOMPONENTID ksComponentId {0};
            DWORD ksComponentIdSize {0};

            // retrieve the device interface id string
            if (!SetupDiGetDeviceInterfaceDetail(
                devInfo.get(),
                &deviceInterfaceData,
                nullptr,
                0,
                &requiredSize,
                nullptr))
            {
                if (ERROR_INSUFFICIENT_BUFFER == GetLastError())
                {
                    interfaceDetailData.reset((PSP_DEVICE_INTERFACE_DETAIL_DATA)(new (std::nothrow) BYTE[requiredSize]));
                    RETURN_IF_NULL_ALLOC(interfaceDetailData);
                    memset(interfaceDetailData.get(), 0, requiredSize);
                    interfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
                    SetupDiGetDeviceInterfaceDetail(
                                    devInfo.get(),
                                    &deviceInterfaceData,
                                    interfaceDetailData.get(),
                                    requiredSize,
                                    nullptr,
                                    nullptr);
                }
            }
            if (nullptr == interfaceDetailData)
            {
                continue;
            }

            // retrieve the assigned port number for this interface
            // if not present, then this may be the synth, which currently
            // doesn't go through midisrv.
            if (!SetupDiGetDeviceInterfaceProperty(
                devInfo.get(),
                &deviceInterfaceData,
                &PKEY_MIDI_ServiceAssignedPortNumber,
                &propType,
                (PBYTE) &servicePortNum,
                sizeof(DWORD),
                nullptr,
                0))
            {
                continue;
            }
            if (propType != DEVPROP_TYPE_UINT32)
            {
                continue;
            }

            // retrieve the friendly name for the port
            if (!SetupDiGetDeviceInterfaceProperty(
                devInfo.get(),
                &deviceInterfaceData,
                &DEVPKEY_DeviceInterface_FriendlyName,
                &propType,
                (PBYTE) &deviceName,
                sizeof(deviceName),
                &requiredSize,
                0))
            {
                continue;
            }
            if (propType != DEVPROP_TYPE_STRING ||
                requiredSize < sizeof(WCHAR))
            {
                continue;
            }

            // to support DRV_QUERYDEVICEINTERFACE
            if (!SetupDiGetDeviceInterfaceProperty(
                devInfo.get(),
                &deviceInterfaceData,
                &PKEY_MIDI_DriverDeviceInterface,
                &propType,
                (PBYTE)&deviceDriverInterfaceId,
                sizeof(deviceDriverInterfaceId),
                &requiredSize,
                0))
            {
                continue;
            }
            if (propType != DEVPROP_TYPE_STRING ||
                requiredSize < sizeof(WCHAR))
            {
                continue;
            }

            if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
            {
                // Retrieve the KS component id information, if available,
                // from the SWD.
                if (SetupDiGetDeviceInterfaceProperty(
                    devInfo.get(),
                    &deviceInterfaceData,
                    &PKEY_MIDI_KsComponentId,
                    &propType,
                    (PBYTE) &ksComponentId,
                    sizeof(KSCOMPONENTID),
                    &ksComponentIdSize,
                    0))
                {
                    if (propType != DEVPROP_TYPE_BINARY ||
                        ksComponentIdSize != sizeof(KSCOMPONENTID))
                    {
                        continue;
                    }
                }
            }

            // our port numbers start with 1 because they're the "global" port numbers
            // that the user should see and port 0 is reserved for the synth.
            // So, a service port number of 1 indicates that we have 1 port, and so on.
            // This means that the port number count is simply the largest port number
            // we are aware of.
            if (servicePortNum > highestPortNumber)
            {
                highestPortNumber = servicePortNum;
            }

            // save the port information to the array.
            m_MidiPortInfo[flow][servicePortNum].PortNumber = servicePortNum;
            m_MidiPortInfo[flow][servicePortNum].Name = deviceName;
            m_MidiPortInfo[flow][servicePortNum].InterfaceId = WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(interfaceDetailData->DevicePath);
            m_MidiPortInfo[flow][servicePortNum].DriverDeviceInterfaceId = WindowsMidiServicesInternal::ToLowerTrimmedWStringCopy(deviceDriverInterfaceId);

            if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
            {
                WORD wMid {MM_MICROSOFT};
                WORD wPid = (flow == MidiFlowOut)?MM_MSFT_GENERIC_MIDIOUT:MM_MSFT_GENERIC_MIDIIN;
                MMVERSION vDriverVersion {0x0100};
                GUID manufacturerGuid {0};
                GUID productGuid {0};
                GUID nameGuid {0};

                INIT_MMREG_MID( &manufacturerGuid, wMid );
                INIT_MMREG_PID( &productGuid, wPid );

                if (ksComponentIdSize > 0)
                {
                    // Legacy kscomponentid information is available, default to wdmaudio midi in/out
                    // pid, in the event the pid provided by the driver is not compatible,
                    // and the legacy driver versioning, in the even the provided version isn't
                    // compatible.
                    //
                    // This is to as closely as possible match what wdmaud returned for these
                    // drivers, which apps have come to depend upon.
                    wPid = (flow == MidiFlowOut)?MM_MSFT_WDMAUDIO_MIDIOUT:MM_MSFT_WDMAUDIO_MIDIIN;
                    vDriverVersion = MAKEWORD(VER_PRODUCTMINORVERSION, VER_PRODUCTMAJORVERSION);

                    manufacturerGuid = ksComponentId.Manufacturer;
                    productGuid = ksComponentId.Product;
                    nameGuid = ksComponentId.Name;
                
                    if (IS_COMPATIBLE_MMREG_MID(&ksComponentId.Manufacturer))
                    {
                        wMid = EXTRACT_MMREG_MID(&ksComponentId.Manufacturer);
                    }
                    
                    if (IS_COMPATIBLE_MMREG_PID(&ksComponentId.Product))
                    {
                        wPid = EXTRACT_MMREG_PID(&ksComponentId.Product);
                    }
                
                    if ((ksComponentId.Version < 256) && (ksComponentId.Revision < 256))
                    {
                        vDriverVersion = MAKEWORD(ksComponentId.Revision, ksComponentId.Version);
                    }
                }

                // Fill in the midiCaps for this port
                if (flow == MidiFlowOut)
                {
                    MIDIOUTCAPS2W *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiOutCaps);
                
                    caps->wMid = wMid;
                    caps->wPid = wPid;
                    caps->vDriverVersion = vDriverVersion;
                    caps->ManufacturerGuid = manufacturerGuid;
                    caps->ProductGuid = productGuid;
                    caps->NameGuid = nameGuid;
                
                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->wTechnology = MOD_MIDIPORT;
                    caps->wVoices = 0;
                    caps->wNotes = 0;
                    caps->wChannelMask = 0xFFFF;
                    caps->dwSupport = 0;
                }
                else
                {
                    MIDIINCAPS2W *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiInCaps);
                
                    caps->wMid = wMid;
                    caps->wPid = wPid;
                    caps->vDriverVersion = vDriverVersion;
                    caps->ManufacturerGuid = manufacturerGuid;
                    caps->ProductGuid = productGuid;
                    caps->NameGuid = nameGuid;

                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->dwSupport = 0;
                }
            }
            else
            {
                // Fill in the midiCaps for this port
                if (flow == MidiFlowOut)
                {
                    MIDIOUTCAPSW *caps = (MIDIOUTCAPSW*) &(m_MidiPortInfo[flow][servicePortNum].MidiOutCaps);
                
                    caps->wMid = MM_MICROSOFT;
                    caps->wPid = MM_MSFT_GENERIC_MIDIOUT;
                    caps->vDriverVersion = 0x0100;
                
                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->wTechnology = MOD_MIDIPORT;
                    caps->wVoices = 0;
                    caps->wNotes = 0;
                    caps->wChannelMask = 0xFFFF;
                    caps->dwSupport = 0;
                }
                else
                {
                    MIDIINCAPSW *caps = (MIDIINCAPSW*) &(m_MidiPortInfo[flow][servicePortNum].MidiInCaps);
                
                    caps->wMid = MM_MICROSOFT;
                    caps->wPid = MM_MSFT_GENERIC_MIDIIN;
                    caps->vDriverVersion = 0x0100;

                    //wcsncpy_s(caps->szPname, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), MAXPNAMELEN);
                    wcsncpy_s(caps->szPname, MAXPNAMELEN, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), _TRUNCATE);
                    caps->szPname[MAXPNAMELEN - 1] = NULL;
                
                    caps->dwSupport = 0;
                }
            }
        }
    }

    // save and return the highest port number
    m_MidiPortCount[flow] = highestPortNumber;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"));

    return S_OK;
}


_Use_decl_annotations_
HRESULT
CMidiPorts::GetDevCaps(MidiFlow flow, UINT portNumber, DWORD_PTR midiCaps, DWORD_PTR midiCapsSize)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(portNumber, "portNumber"),
        TraceLoggingValue(midiCaps, "midiCaps"));

    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlowIn && flow != MidiFlowOut);
    RETURN_HR_IF(E_INVALIDARG, midiCaps == 0);

    // clear the provided caps, in the event that the port is not active.
    if (MidiFlowIn == flow)
    {
        if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
        {
            RETURN_HR_IF(E_INVALIDARG, midiCapsSize < sizeof(MIDIINCAPSW));
            memset((PVOID) midiCaps, 0, midiCapsSize);
        }
        else
        {
            memset((PVOID) midiCaps, 0, sizeof(MIDIINCAPSW));
        }

        // set the default name in case the port is not active. Some apps ignore the hresult
        ::LoadStringW(HINST_WDMAUD2, IDS_MIDI_UNAVAILABLE_ENDPOINT, ((MIDIINCAPSW*)midiCaps)->szPname, MAXPNAMELEN);
    }
    else
    {
        if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
        {
            RETURN_HR_IF(E_INVALIDARG, midiCapsSize < sizeof(MIDIOUTCAPSW));
            memset((PVOID) midiCaps, 0, midiCapsSize);
        }
        else
        {
            memset((PVOID) midiCaps, 0, sizeof(MIDIOUTCAPSW));
        }

        // set the default name in case the port is not active. Some apps ignore the hresult
        ::LoadStringW(HINST_WDMAUD2, IDS_MIDI_UNAVAILABLE_ENDPOINT, ((MIDIOUTCAPSW*)midiCaps)->szPname, MAXPNAMELEN);
    }

    auto lock = m_Lock.lock();

    // The port numbers provided to us by winmm start with 0, our port numbers
    // start with 1 because they're the "global" port numbers that the user
    // should see and port 0 is reserved for the synth.
    UINT localPortNumber = portNumber + 1;

    RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_BADDEVICEID), localPortNumber > m_MidiPortCount[flow]);

    auto port = m_MidiPortInfo[flow].find(localPortNumber);

    RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_NODRIVER), port == m_MidiPortInfo[flow].end());

    if (MidiFlowIn == flow)
    {
        if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
        {
            memcpy((PVOID) midiCaps, &(port->second.MidiInCaps), min(sizeof(port->second.MidiInCaps), midiCapsSize));
        }
        else
        {
            memcpy((PVOID) midiCaps, &(port->second.MidiInCaps), sizeof(port->second.MidiInCaps));
        }
    }
    else
    {
        if (Feature_Servicing_MIDI2DevCaps2::IsEnabled())
        {
            memcpy((PVOID) midiCaps, &(port->second.MidiOutCaps), min(sizeof(port->second.MidiOutCaps), midiCapsSize));
        }
        else
        {
            memcpy((PVOID) midiCaps, &(port->second.MidiOutCaps), sizeof(port->second.MidiOutCaps));
        }
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::GetOpenedPort(MidiFlow flow, MidiPortHandle portHandle, wil::com_ptr_nothrow<CMidiPort> &port)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(portHandle, "portHandle")
    );

    auto lock = m_Lock.lock();
    auto portIterator = m_OpenPorts.find(portHandle);
    // If this is an invalid port handle, fail.
    RETURN_HR_IF(E_INVALIDARG, portIterator == m_OpenPorts.end());
    
    // Confirm that the flow requested matches the flow of the port we have
    // with this handle, otherwise it's an error.
    RETURN_HR_IF(E_INVALIDARG, !portIterator->second->IsFlow(flow));
    
    // take a reference to this open port so we don't have to hold up all other messages
    // while processing this one.
    port = portIterator->second;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::Open(MidiFlow flow, UINT portNumber, const MIDIOPENDESC* midiOpenDesc, DWORD_PTR flags, MidiPortHandle* openedPort)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(portNumber, "portNumber"),
        TraceLoggingValue(flags, "Flags"));

    auto lock = m_Lock.lock();

    wil::com_ptr_nothrow<CMidiPort> midiPort;

    RETURN_HR_IF(E_POINTER, nullptr == midiOpenDesc);
    RETURN_HR_IF(E_POINTER, nullptr == openedPort);
    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlowIn && flow != MidiFlowOut);

    // The port numbers provided to us by winmm start with index 0. Early in the design
    // the intent was to skip port numbers to get the input and output port numbers to align,
    // and because index 0 is always the midi synth it made sense to always start service port
    // numbers at 1, so the service assigned port number matched the winmm port number.
    // That strategy broke some legacy clients that didn't handle non-contiguous port numbers and
    // was abandoned.
    // We've left the service assigned port numbers as starting at index 1, so we have to add 1
    // to the port number provided by winmm.
    UINT localPortNumber = portNumber + 1;

    if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
    {
        RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_NODRIVER), localPortNumber > m_MidiPortCount[flow]);
    }
    else
    {
        RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_BADDEVICEID), localPortNumber > m_MidiPortCount[flow]);
    }

    auto portInfo = m_MidiPortInfo[flow].find(localPortNumber);

    RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_NODRIVER), portInfo == m_MidiPortInfo[flow].end());

    // create the CMidiPort for this port.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiPort>(&midiPort, m_SessionId, portInfo->second.InterfaceId, flow, midiOpenDesc, flags));

    *openedPort = (MidiPortHandle) midiPort.get();

    m_OpenPorts.emplace(*openedPort, std::move(midiPort));

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(portNumber, "portNumber"),
        TraceLoggingValue(flags, "Flags"),
        TraceLoggingValue(*openedPort, "MidiPortHandle"));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::Close(MidiFlow flow, MidiPortHandle portHandle)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(portHandle, "MidiPortHandle"));

    auto lock = m_Lock.lock();

    auto port = m_OpenPorts.find(portHandle);
    RETURN_HR_IF(E_INVALIDARG, port == m_OpenPorts.end());
    RETURN_HR_IF(E_INVALIDARG, !port->second->IsFlow(flow));

    // remove it from the open ports list, even if cleanup fails.
    auto remove = wil::scope_exit([&]() { m_OpenPorts.erase(port); });
    RETURN_IF_FAILED(port->second->Shutdown());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::GetDeviceInterface(MidiFlow flow, UINT portNumber, UINT msg, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)flow, "MidiFlow"),
        TraceLoggingValue(portNumber, "portNumber"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    auto lock = m_Lock.lock();

    // output memory pointer for both calls, for querying the interface this will
    // be a non-null string pointer. For querying the size it will be a non-null pointer
    // to a ulong.
    RETURN_HR_IF(E_POINTER, NULL == param1);
    RETURN_HR_IF(E_INVALIDARG, flow != MidiFlowIn && flow != MidiFlowOut);

    // The port numbers provided to us by winmm start with index 0. Early in the design
    // the intent was to skip port numbers to get the input and output port numbers to align,
    // and because index 0 is always the midi synth it made sense to always start service port
    // numbers at 1, so the service assigned port number matched the winmm port number.
    // That strategy broke some legacy clients that didn't handle non-contiguous port numbers and
    // was abandoned.
    // We've left the service assigned port numbers as starting at index 1, so we have to add 1
    // to the port number provided by winmm.
    UINT localPortNumber = portNumber + 1;
    RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_BADDEVICEID), localPortNumber > m_MidiPortCount[flow]);
    auto portInfo = m_MidiPortInfo[flow].find(localPortNumber);
    RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_NODRIVER), portInfo == m_MidiPortInfo[flow].end());

    switch(msg)
    {
        case DRV_QUERYDEVICEINTERFACE:
            RETURN_IF_FAILED(StringCchCopyW((PWSTR) param1, ((UINT)param2)/sizeof(wchar_t), portInfo->second.DriverDeviceInterfaceId.c_str()));
            break;
        case DRV_QUERYDEVICEINTERFACESIZE:
            *((PULONG)param1) = static_cast<ULONG>((portInfo->second.DriverDeviceInterfaceId.size() + 1) * sizeof(wchar_t));
            break;
    }

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::ForwardMidMessage(UINT msg, MidiPortHandle portHandle, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(portHandle, "MidiPortHandle"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    wil::com_ptr_nothrow<CMidiPort> port;

    RETURN_IF_FAILED(GetOpenedPort(MidiFlowIn, portHandle, port));

    if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
    {
        // If this is a close message, we still want to forward
        // the message to the port so that it has the opportunity
        // to close, even if it has been invalidated.
        if (MIDM_CLOSE != msg && port->IsInvalidated())
        {
            // If the device has been removed, return no driver
            return HRESULT_FROM_MMRESULT(MMSYSERR_NODRIVER);
        }
    }

    RETURN_IF_FAILED(port->MidMessage(msg, param1, param2));
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::ForwardModMessage(UINT msg, MidiPortHandle portHandle, DWORD_PTR param1, DWORD_PTR param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_VERBOSE,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(msg, "msg"),
        TraceLoggingValue(portHandle, "MidiPortHandle"),
        TraceLoggingValue(param1, "param1"),
        TraceLoggingValue(param2, "param2"));

    wil::com_ptr_nothrow<CMidiPort> port;

    RETURN_IF_FAILED(GetOpenedPort(MidiFlowOut, portHandle, port));

    if (Feature_Servicing_MIDI2NumDevsPerf::IsEnabled())
    {
        // If this is a close message, we still want to forward
        // the message to the port so that it has the opportunity
        // to close, even if it has been invalidated.
        if (MODM_CLOSE != msg && port->IsInvalidated())
        {
            // If the device has been removed, return no driver
            return E_HANDLE;
        }
    }

    RETURN_IF_FAILED(port->ModMessage(msg, param1, param2));
    return S_OK;
}

