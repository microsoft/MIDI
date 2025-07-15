// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "MidiSrvPorts.h"

using unique_hdevinfo = wil::unique_any_handle_invalid<decltype(&::SetupDiDestroyDeviceInfoList), ::SetupDiDestroyDeviceInfoList>;

// via https://devblogs.microsoft.com/oldnewthing/20041025-00/?p=37483
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_WDMAUD2 ((HINSTANCE)&__ImageBase)


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
                UINT32 deviceCount{ };
                hr = GetMidiDeviceCount(MidiFlowIn, deviceCount);
                if (SUCCEEDED(hr))
                {
                    return (MAKELONG(deviceCount, MMSYSERR_NOERROR));
                }
            }
            break;
        case MIDM_GETDEVCAPS:
            hr = GetDevCaps(MidiFlowIn, deviceID, param1);
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
                UINT32 deviceCount{ };
                hr = GetMidiDeviceCount(MidiFlowOut, deviceCount);
                if (SUCCEEDED(hr))
                {
                    return (MAKELONG(deviceCount, MMSYSERR_NOERROR));
                }
            }
            break;
        case MODM_GETDEVCAPS:
            hr = GetDevCaps(MidiFlowOut, deviceID, param1);
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
        default:
            hr = ForwardModMessage(msg, (MidiPortHandle) user, param1, param2);
    }

    return MMRESULT_FROM_HRESULT(hr);
}

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
            std::unique_ptr<SP_DEVICE_INTERFACE_DETAIL_DATA> interfaceDetailData;

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
            
            // Fill in the midiCaps for this port
            if (flow == MidiFlowOut)
            {
                MIDIOUTCAPSW *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiOutCaps);
            
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
                MIDIINCAPSW *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiInCaps);
            
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

_Use_decl_annotations_
HRESULT
CMidiPorts::GetDevCaps(MidiFlow flow, UINT portNumber, DWORD_PTR midiCaps)
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
        memset((PVOID) midiCaps, 0, sizeof(MIDIINCAPSW));

        // set the default name in case the port is not active. Some apps ignore the hresult
        ::LoadStringW(HINST_WDMAUD2, IDS_MIDI_UNAVAILABLE_ENDPOINT, ((MIDIINCAPSW*)midiCaps)->szPname, MAXPNAMELEN);
    }
    else
    {
        memset((PVOID) midiCaps, 0, sizeof(MIDIOUTCAPSW));

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
        memcpy((PVOID) midiCaps, &(port->second.MidiInCaps), sizeof(port->second.MidiInCaps));
    }
    else
    {
        memcpy((PVOID) midiCaps, &(port->second.MidiOutCaps), sizeof(port->second.MidiOutCaps));
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

    // The port numbers provided to us by winmm start with 0, our port numbers
    // start with 1 because they're the "global" port numbers that the user
    // should see and port 0 is reserved for the synth.
    UINT localPortNumber = portNumber + 1;

    RETURN_HR_IF(HRESULT_FROM_MMRESULT(MMSYSERR_BADDEVICEID), localPortNumber > m_MidiPortCount[flow]);

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
    RETURN_IF_FAILED(port->ModMessage(msg, param1, param2));
    return S_OK;
}

