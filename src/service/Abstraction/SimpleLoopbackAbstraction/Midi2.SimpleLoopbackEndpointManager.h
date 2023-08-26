// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

typedef enum _SWDEVICESTATE
{
    NotCreated = 0, // SwDeviceCreate not yet called
    CreatePending,  // SwDeviceCreate called successfully, but creation callback not yet invoked
    Created,        // SwDeviceCreate creation callback has been invoked and device interface has been created
    Failed
} SWDEVICESTATE;


using unique_hswdevice = wil::unique_any<HSWDEVICE, decltype(&::SwDeviceClose), ::SwDeviceClose>;
using unique_swd_string = wil::unique_any<PWSTR, decltype(&::SwMemFree), ::SwMemFree>;


class MidiUmpEndpointInfo
{
public:
    std::wstring Id; // the filter InterfaceId
    std::wstring InstanceId; // the MIDI instance id
    std::wstring ParentInstanceId; // The instance id of the parent device
    std::wstring Name; // friendly name for this device
    MidiFlow Flow{ MidiFlowBidirectional };

    // TODO: Pointer to the interface?

};

class MidiEndpointParentDeviceInfo
{
public:
    GUID InterfaceCategory;
    SWDEVICESTATE SwDeviceState;         // SWD creation state
    unique_hswdevice SwDevice;           // Handle to the SWD created for the MIDI port
    unique_swd_string DeviceInterfaceId; // SWD interface ID for the MIDI port
    std::wstring InstanceId;
    std::wstring Name; // friendly name for this device

};

typedef struct _PARENTDEVICECREATECONTEXT
{
    MidiEndpointParentDeviceInfo* MidiParentDevice;
    wil::unique_event creationCompleted{ wil::EventOptions::None };
    DEVPROPERTY* InterfaceDevProperties;
    ULONG IntPropertyCount;
} PARENTDEVICECREATECONTEXT, * PPARENTDEVICECREATECONTEXT;


class CMidi2SimpleLoopbackEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*));
    STDMETHOD(Cleanup)();

private:
    GUID m_containerId{};
    GUID m_transportAbstractionId{};




    HRESULT CreateEndpoint(_In_ std::wstring const instanceId, _In_ std::wstring const name, _In_ MidiFlow const flow);
    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    std::vector<std::unique_ptr<MidiUmpEndpointInfo>> m_AvailableMidiUmpEndpoints;




    std::unique_ptr<MidiEndpointParentDeviceInfo> m_parentDevice{ nullptr };


};
