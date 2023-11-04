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
    std::wstring Id{};                  // the filter InterfaceId
    std::wstring InstanceId{};          // the MIDI instance id
    std::wstring ParentInstanceId{};    // The instance id of the parent device
    std::wstring Name{};                // friendly name for this device
    MidiFlow Flow{ MidiFlowBidirectional };

    // TODO: Pointer to the interface?

};

class MidiEndpointParentDeviceInfo
{
public:
    GUID InterfaceCategory{};
    SWDEVICESTATE SwDeviceState{ SWDEVICESTATE::NotCreated };   // SWD creation state
    unique_hswdevice SwDevice{};                                // Handle to the SWD created for the MIDI port
    unique_swd_string DeviceInterfaceId{};                      // SWD interface ID for the MIDI port
    std::wstring InstanceId{};
    std::wstring Name{};                                        // friendly name for this device

};

typedef struct _PARENTDEVICECREATECONTEXT
{
    MidiEndpointParentDeviceInfo* MidiParentDevice{ nullptr };
    wil::unique_event creationCompleted{ wil::EventOptions::None };
    DEVPROPERTY* InterfaceDevProperties{ nullptr };
    ULONG IntPropertyCount{};
} PARENTDEVICECREATECONTEXT, * PPARENTDEVICECREATECONTEXT;


// TODO: This class can implement another interface which takes in the json parameters 
// (or whatever we want) for naming the new endpoints. Then, the SDK can call that
// interface to do the setup of the device. Or, there can be a common "runtime creatable transport"
// interface that is called from the API. TBD. But can't break that interface with 
// the SDK, since the SDK ships with apps and could be older.


class CMidi2VirtualMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IUnknown*, _In_ LPCWSTR));
    STDMETHOD(Cleanup)();

    //STDMETHOD(ApplyConfiguration(
    //    _In_ LPCWSTR configurationJson,
    //    _Out_ LPWSTR resultJson
    //));


private:
    GUID m_containerId{};
    GUID m_transportAbstractionId{};

    HRESULT CreateEndpoint(
        _In_ std::wstring const instanceId,
        _In_ std::wstring const uniqueId,
        _In_ bool const multiclient,
        _In_ bool const isVirtualEndpointResponder,
        _In_ std::wstring const name,
        _In_ std::wstring const largeImagePath,
        _In_ std::wstring const smallImagePath,
        _In_ std::wstring const description
    );


    HRESULT CreateConfiguredDeviceEndpoints(_In_ std::wstring configurationJson);
    
    HRESULT CreateParentDevice();
    HRESULT CreateClientSideEndpoint(_In_ std::wstring deviceSideInstanceId);


    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_MidiDeviceManager;

    // TBD if we need to keep this here as well. The MidiDeviceManager has its own vector of endpoints
    std::vector<std::unique_ptr<MidiUmpEndpointInfo>> m_AvailableMidiUmpEndpoints;

    std::unique_ptr<MidiEndpointParentDeviceInfo> m_parentDevice{ nullptr };

    json::JsonObject m_jsonObject{ nullptr };
};
