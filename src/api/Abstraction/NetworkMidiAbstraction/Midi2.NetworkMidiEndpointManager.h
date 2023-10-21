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
    //INT32 PinId{ 0 };
    //BOOL Standard{ FALSE };
    //BOOL Cyclic{ FALSE };
    //BOOL UMP{ TRUE };
    MidiFlow Flow{ MidiFlowBidirectional };
};

class MidiEndpointParentDeviceInfo
{
public:
    GUID InterfaceCategory;
    SWDEVICESTATE SwDeviceState;         // SWD creation state
    unique_hswdevice SwDevice;           // Handle to the SWD created for the MIDI port
    unique_swd_string DeviceInterfaceId; // SWD interface ID for the MIDI port
    std::wstring InstanceId;

//    std::wstring Id; // the filter InterfaceId
    std::wstring Name; // friendly name for this device
    //INT32 PinId{ 0 };
    //BOOL Standard{ FALSE };
    //BOOL Cyclic{ FALSE };
    //BOOL UMP{ TRUE };
    //MidiFlow Flow{ MidiFlowBidirectional };
};

typedef struct _PARENTDEVICECREATECONTEXT
{
    MidiEndpointParentDeviceInfo* MidiParentDevice;
    wil::unique_event creationCompleted{ wil::EventOptions::None };
    DEVPROPERTY* InterfaceDevProperties;
    ULONG IntPropertyCount;
} PARENTDEVICECREATECONTEXT, * PPARENTDEVICECREATECONTEXT;


// TODO: This class can implement another interface which takes in the json parameters 
// (or whatever we want) for naming the new endpoints. Then, the SDK can call that
// interface to do the setup of the device. Or, there can be a common "runtime creatable transport"
// interface that is called from the API. TBD. But can't break that interface with 
// the SDK, since the SDK ships with apps and could be older.


class CMidi2NetworkMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ LPCWSTR));
    STDMETHOD(Cleanup)();

private:

    HRESULT CreateEndpoint();
    HRESULT CreateParentDevice();

    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    // TBD if we need to keep this here as well. The MidiDeviceManager has its own vector of endpoints
    std::vector<std::unique_ptr<MidiUmpEndpointInfo>> m_AvailableMidiUmpEndpoints;

    std::unique_ptr<MidiEndpointParentDeviceInfo> m_parentDevice{ nullptr };


};
