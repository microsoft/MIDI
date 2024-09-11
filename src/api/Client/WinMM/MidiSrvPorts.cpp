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
        Cleanup();
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
        TraceLoggingPointer(this, "this"));

    RETURN_IF_FAILED(CoCreateGuid(&m_SessionId));
    RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvAbstraction), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_MidisrvAbstraction)));
    RETURN_IF_FAILED(m_MidisrvAbstraction->Activate(__uuidof(IMidiSessionTracker), (void **) &m_MidiSessionTracker));
    RETURN_IF_FAILED(m_MidiSessionTracker->AddClientSession(m_SessionId, m_SessionName.c_str()));

    return S_OK;        
}

HRESULT
CMidiPorts::Cleanup()
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
        port.second->Cleanup();
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
CMidiPorts::MidMessage(UINT DeviceID, UINT Msg, DWORD_PTR User, DWORD_PTR Param1, DWORD_PTR Param2)
{
    HRESULT hr = S_OK;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(DeviceID, "DeviceID"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(User, "User"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    switch(Msg)
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
            hr = GetDevCaps(MidiFlowIn, DeviceID, Param1);
            break;
        case MIDM_OPEN:
            hr = Open(MidiFlowIn, DeviceID, (MIDIOPENDESC *) Param1, Param2, (MidiPortHandle*) User);
            break;
        case MIDM_CLOSE:
            // Forward the message to the open port, if one exists, giving the port an opportunity to
            // communicate that it can not be closed.
            if (SUCCEEDED(hr = ForwardMidMessage(Msg, (MidiPortHandle) User, Param1, Param2)))
            {
                hr = Close(MidiFlowIn, (MidiPortHandle) User);
            }
            break;
        default:
            hr = ForwardMidMessage(Msg, (MidiPortHandle) User, Param1, Param2);
    }

    return MMRESULT_FROM_HRESULT(hr);
}

_Use_decl_annotations_
DWORD APIENTRY
CMidiPorts::ModMessage(UINT DeviceID, UINT Msg, DWORD_PTR User, DWORD_PTR Param1, DWORD_PTR Param2)
{
    HRESULT hr = S_OK;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(DeviceID, "DeviceID"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(User, "User"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    switch(Msg)
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
            hr = GetDevCaps(MidiFlowOut, DeviceID, Param1);
            break;
        case MODM_OPEN:
            hr = Open(MidiFlowOut, DeviceID, (MIDIOPENDESC *) Param1, Param2, (MidiPortHandle*) User);
            break;
        case MODM_CLOSE:
            // Forward the message to the open port, if one exists, giving the port an opportunity to
            // communicate that it can not be closed.
            if (SUCCEEDED(hr = ForwardModMessage(Msg, (MidiPortHandle) User, Param1, Param2)))
            {
                hr = Close(MidiFlowOut, (MidiPortHandle) User);
            }
            break;
        default:
            hr = ForwardModMessage(Msg, (MidiPortHandle) User, Param1, Param2);
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
CMidiPorts::GetMidiDeviceCount(MidiFlow Flow, UINT32& Count)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"));

    auto lock = m_Lock.lock();

    // We return the largest valid port number here, the client
    // is then responsible to retrieve the dev caps for these ports
    // to determine which ports are present at runtime.
    UINT highestPortNumber {0};

    // throw away old structure for this flow
    // build up new structure of all active devices for the requested flow, return
    // maximum port number, our port numbers are from 1->, max port number is 0->
    // (because port 0 is reserved for the synth, which will eventually be in midisrv)           
    m_MidiPortInfo[Flow].clear();

    winrt::hstring deviceSelector(Flow == MidiFlowOut?ACTIVE_MIDI1_OUTPUT_DEVICES:ACTIVE_MIDI1_INPUT_DEVICES);
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
            servicePortNum = winrt::unbox_value<UINT32>(prop);
            servicePortNumValid = true;
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

            m_MidiPortInfo[Flow][servicePortNum].PortNumber = servicePortNum;
            m_MidiPortInfo[Flow][servicePortNum].Name = L"temp name";
            m_MidiPortInfo[Flow][servicePortNum].InterfaceId = device.Id().c_str();

            // Fill in the MidiCaps for this port
            if (Flow == MidiFlowOut)
            {
                MIDIOUTCAPSW *caps = &(m_MidiPortInfo[Flow][servicePortNum].MidiOutCaps);

                caps->wMid = MM_MICROSOFT;
                caps->wPid = MM_MSFT_GENERIC_MIDIOUT;
                caps->vDriverVersion = 0x0100;

                // not good because this could end up > 32 characters
                std::wstring tempName = std::wstring((std::wstring_view)device.Name()).substr(0,21) + std::wstring(L" (MidiSrv)");

                if (FAILED(StringCchPrintfW(caps->szPname, MAXPNAMELEN, tempName.c_str())))
                {
                    return;
                }

                //if (FAILED(StringCchPrintfW(caps->szPname, MAXPNAMELEN, TEXT("MidiSrv output device %d"), servicePortNum)))
                //{
                //    return;
                //}

                caps->wTechnology = MOD_MIDIPORT;
                caps->wVoices = 0;
                caps->wNotes = 0;
                caps->wChannelMask = 0xFFFF;
                caps->dwSupport = 0;
            }
            else
            {
                MIDIINCAPSW *caps = &(m_MidiPortInfo[Flow][servicePortNum].MidiInCaps);

                caps->wMid = MM_MICROSOFT;
                caps->wPid = MM_MSFT_GENERIC_MIDIIN;
                caps->vDriverVersion = 0x0100;

                // not good because this could end up > 32 characters
                std::wstring tempName = std::wstring((std::wstring_view)device.Name()).substr(0, 21) + std::wstring(L" (MidiSrv)");

                if (FAILED(StringCchPrintfW(caps->szPname, MAXPNAMELEN, tempName.c_str())))
                {
                    return;
                }

                //if (FAILED(StringCchPrintfW(caps->szPname, MAXPNAMELEN, TEXT("MidiSrv input device %d"), servicePortNum)))
                //{
                //    return;
                //}

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

    Count = highestPortNumber;

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(Count, "Count"));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::GetDevCaps(MidiFlow Flow, UINT PortNumber, DWORD_PTR MidiCaps)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(PortNumber, "PortNumber"),
        TraceLoggingValue(MidiCaps, "MidiCaps"));

    RETURN_HR_IF(E_INVALIDARG, Flow != MidiFlowIn && Flow != MidiFlowOut);

    auto lock = m_Lock.lock();

    // The port numbers provided to us by winmm start with 0, our port numbers
    // start with 1 because they're the "global" port numbers that the user
    // should see and port 0 is reserved for the synth.
    UINT localPortNumber = PortNumber + 1;
    auto port = m_MidiPortInfo[Flow].find(localPortNumber);

    //RETURN_HR_IF(E_INVALIDARG, port == m_MidiPortInfo[Flow].end());
    //RETURN_HR_IF(E_HANDLE, port == m_MidiPortInfo[Flow].end());

    if (port == m_MidiPortInfo[Flow].end())
    {
        // port doesn't exist. We return a zero struct. This is likely
        // not correct, but doing this for testing

        if (MidiFlowIn == Flow)
        {
            MIDIINCAPSW caps{ 0 };

            memcpy((PVOID)MidiCaps, &caps, sizeof(caps));
        }
        else
        {
            MIDIOUTCAPSW caps{ 0 };

            memcpy((PVOID)MidiCaps, &caps, sizeof(caps));
        }
    }
    else
    {
        if (MidiFlowIn == Flow)
        {
            memcpy((PVOID)MidiCaps, &(port->second.MidiInCaps), sizeof(port->second.MidiInCaps));
        }
        else
        {
            memcpy((PVOID)MidiCaps, &(port->second.MidiOutCaps), sizeof(port->second.MidiOutCaps));
        }
    }

    
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::GetOpenedPort(MidiFlow Flow, MidiPortHandle PortHandle, wil::com_ptr_nothrow<CMidiPort> &Port)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(PortHandle, "PortHandle")
    );


    auto lock = m_Lock.lock();
    auto portIterator = m_OpenPorts.find(PortHandle);
    // If this is an invalid port handle, fail.
    RETURN_HR_IF(E_INVALIDARG, portIterator == m_OpenPorts.end());
    
    // Confirm that the flow requested matches the flow of the port we have
    // with this handle, otherwise it's an error.
    RETURN_HR_IF(E_INVALIDARG, !portIterator->second->IsFlow(Flow));
    
    // take a reference to this open port so we don't have to hold up all other messages
    // while processing this one.
    Port = portIterator->second;

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::Open(MidiFlow Flow, UINT PortNumber, MIDIOPENDESC* MidiOpenDesc, DWORD_PTR Flags, MidiPortHandle* OpenedPort)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(PortNumber, "PortNumber"),
        TraceLoggingValue(Flags, "Flags"));

    auto lock = m_Lock.lock();

    wil::com_ptr_nothrow<CMidiPort> midiPort;

    RETURN_HR_IF(E_POINTER, nullptr == MidiOpenDesc);
    RETURN_HR_IF(E_POINTER, nullptr == OpenedPort);
    RETURN_HR_IF(E_INVALIDARG, Flow != MidiFlowIn && Flow != MidiFlowOut);

    // The port numbers provided to us by winmm start with 0, our port numbers
    // start with 1 because they're the "global" port numbers that the user
    // should see and port 0 is reserved for the synth.
    UINT localPortNumber = PortNumber + 1;
    auto portInfo = m_MidiPortInfo[Flow].find(localPortNumber);
    RETURN_HR_IF(E_INVALIDARG, portInfo == m_MidiPortInfo[Flow].end());

    // create the CMidiPort for this port.
    RETURN_IF_FAILED(Microsoft::WRL::MakeAndInitialize<CMidiPort>(&midiPort, m_SessionId, portInfo->second.InterfaceId, Flow, MidiOpenDesc, Flags));

    *OpenedPort = (MidiPortHandle) midiPort.get();

    m_OpenPorts.emplace(*OpenedPort, std::move(midiPort));

    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(PortNumber, "PortNumber"),
        TraceLoggingValue(Flags, "Flags"),
        TraceLoggingValue(*OpenedPort, "MidiPortHandle"));

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::Close(MidiFlow Flow, MidiPortHandle PortHandle)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue((int)Flow, "MidiFlow"),
        TraceLoggingValue(PortHandle, "MidiPortHandle"));

    auto lock = m_Lock.lock();

    auto port = m_OpenPorts.find(PortHandle);
    RETURN_HR_IF(E_INVALIDARG, port == m_OpenPorts.end());
    RETURN_HR_IF(E_INVALIDARG, !port->second->IsFlow(Flow));

    // remove it from the open ports list, even if cleanup fails.
    auto remove = wil::scope_exit([&]() { m_OpenPorts.erase(port); });
    RETURN_IF_FAILED(port->second->Cleanup());

    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::ForwardMidMessage(UINT Msg, MidiPortHandle PortHandle, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(PortHandle, "MidiPortHandle"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    wil::com_ptr_nothrow<CMidiPort> port;

    RETURN_IF_FAILED(GetOpenedPort(MidiFlowIn, PortHandle, port));
    RETURN_IF_FAILED(port->MidMessage(Msg, Param1, Param2));
    return S_OK;
}

_Use_decl_annotations_
HRESULT
CMidiPorts::ForwardModMessage(UINT Msg, MidiPortHandle PortHandle, DWORD_PTR Param1, DWORD_PTR Param2)
{
    TraceLoggingWrite(WdmAud2TelemetryProvider::Provider(),
        MIDI_TRACE_EVENT_INFO,
        TraceLoggingString(__FUNCTION__, MIDI_TRACE_EVENT_LOCATION_FIELD),
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingPointer(this, "this"),
        TraceLoggingValue(Msg, "Msg"),
        TraceLoggingValue(PortHandle, "MidiPortHandle"),
        TraceLoggingValue(Param1, "Param1"),
        TraceLoggingValue(Param2, "Param2"));

    wil::com_ptr_nothrow<CMidiPort> port;

    RETURN_IF_FAILED(GetOpenedPort(MidiFlowOut, PortHandle, port));
    RETURN_IF_FAILED(port->ModMessage(Msg, Param1, Param2));
    return S_OK;
}

