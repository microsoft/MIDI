// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "MidiSrvPorts.h"

using namespace winrt::Windows::Devices::Enumeration;

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
        static_cast<void>(m_MidisrvAbstraction.detach());
        static_cast<void>(m_MidiSessionTracker.detach());
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

    RETURN_IF_FAILED(CoCreateGuid(&m_SessionId));
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_MidisrvAbstraction)));
    RETURN_IF_FAILED(m_MidisrvAbstraction->Activate(__uuidof(IMidiSessionTracker), (void **) &m_MidiSessionTracker));

    RETURN_IF_FAILED(m_MidiSessionTracker->AddClientSession(m_SessionId, m_SessionName.c_str()));

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

    if (m_MidiSessionTracker)
    {
        m_MidiSessionTracker->RemoveClientSession(m_SessionId);
        m_MidiSessionTracker.reset();
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

#define ACTIVE_MIDI1_OUTPUT_DEVICES \
    L"System.Devices.InterfaceClassGuid:=\"{6DC23320-AB33-4CE4-80D4-BBB3EBBF2814}\"" \
    L" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"

#define ACTIVE_MIDI1_INPUT_DEVICES \
    L"System.Devices.InterfaceClassGuid:=\"{504BE32C-CCF6-4D2C-B73F-6F8B3747E22B}\"" \
    L" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"

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

    winrt::hstring deviceSelector(flow == MidiFlowOut?ACTIVE_MIDI1_OUTPUT_DEVICES:ACTIVE_MIDI1_INPUT_DEVICES);
    wil::unique_event enumerationCompleted{wil::EventOptions::None};

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_ServiceAssignedPortNumber));

    auto watcher = DeviceInformation::CreateWatcher(deviceSelector, additionalProperties);

    auto deviceAddedHandler = watcher.Added(winrt::auto_revoke, [&](DeviceWatcher watcher, DeviceInformation device)
    {
        bool servicePortNumValid {false};
        UINT32 servicePortNum {0};

        // all others with a service assigned port number goes into the portInfo structure for processing.
        auto prop = device.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_ServiceAssignedPortNumber));
        

        if (prop)
        {
//            servicePortNum = winrt::unbox_value<UINT32>(prop);

            std::optional<UINT32> servicePortNumValue = prop.try_as<UINT32>();
            if (servicePortNumValue != std::nullopt && servicePortNumValue.has_value())
            {
                servicePortNum = servicePortNumValue.value();
                servicePortNumValid = true;
            }
        }

        if (servicePortNumValid)
        {
            // our port numbers start with 1 because they're the "global" port numbers
            // that the user should see and port 0 is reserved for the synth.
            // So, a service port number of 1 indicates that we have 1 port, and so on.
            // This means that the port number count is simply the largest port number
            // we are aware of.
            if (servicePortNum > highestPortNumber)
            {
                highestPortNumber = servicePortNum;
            }

            m_MidiPortInfo[flow][servicePortNum].PortNumber = servicePortNum;
            m_MidiPortInfo[flow][servicePortNum].Name = device.Name();
            m_MidiPortInfo[flow][servicePortNum].InterfaceId = device.Id().c_str();

            // Fill in the midiCaps for this port
            if (flow == MidiFlowOut)
            {
                MIDIOUTCAPSW *caps = &(m_MidiPortInfo[flow][servicePortNum].MidiOutCaps);

                caps->wMid = MM_MICROSOFT;
                caps->wPid = MM_MSFT_GENERIC_MIDIOUT;
                caps->vDriverVersion = 0x0100;

                wcsncpy_s(caps->szPname, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), MAXPNAMELEN);
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

                wcsncpy_s(caps->szPname, m_MidiPortInfo[flow][servicePortNum].Name.c_str(), MAXPNAMELEN);
                caps->szPname[MAXPNAMELEN - 1] = NULL;

                caps->dwSupport = 0;
            }
        }
    });

    auto deviceStoppedHandler = watcher.Stopped(winrt::auto_revoke, [&](DeviceWatcher, winrt::Windows::Foundation::IInspectable)
    {
        enumerationCompleted.SetEvent();
        return S_OK;
    });

    auto enumerationCompletedHandler = watcher.EnumerationCompleted(winrt::auto_revoke, [&](DeviceWatcher , winrt::Windows::Foundation::IInspectable)
    {
        enumerationCompleted.SetEvent();
        return S_OK;
    });

    watcher.Start();
    enumerationCompleted.wait();
    watcher.Stop();
    enumerationCompleted.wait();

    deviceAddedHandler.revoke();
    deviceStoppedHandler.revoke();
    enumerationCompletedHandler.revoke();

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
        MIDI_TRACE_EVENT_INFO,
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
        MIDI_TRACE_EVENT_INFO,
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

