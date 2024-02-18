// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#define AUDIO_DEVICE_ENUMERATOR L"MMDEVAPI"
#define MIDI_DEVICE_ENUMERATOR L"MIDISRV"
#define MIDI_SWD_VIRTUAL_PARENT_ROOT L"HTREE\\ROOT\\0"




// ----------------------------------------------------------------------
//
//  MIDIPORT
//
//      Describes a MIDI port SWD that is created or being created
//
// ----------------------------------------------------------------------
typedef enum _SWDEVICESTATE
{
    NotCreated = 0, // SwDeviceCreate not yet called
    CreatePending,  // SwDeviceCreate called successfully, but creation callback not yet invoked
    Created,        // SwDeviceCreate creation callback has been invoked and device interface has been created
    Failed
} SWDEVICESTATE;

struct GUIDCompare
{
    bool operator()(const GUID& Guid1, const GUID& Guid2) const
    {
        return ::memcmp(&Guid1, &Guid2, sizeof(GUID)) < 0;
    }
};

using unique_hswdevice = wil::unique_any<HSWDEVICE, decltype(&::SwDeviceClose), ::SwDeviceClose>;
using unique_swd_string = wil::unique_any<PWSTR, decltype(&::SwMemFree), ::SwMemFree>;

typedef struct _MIDIPORT
{
    MidiFlow MidiFlow{ MidiFlowIn };
    const GUID* InterfaceCategory{ nullptr };
    SWDEVICESTATE SwDeviceState{ CreatePending };         // SWD creation state
    unique_hswdevice SwDevice;           // Handle to the SWD created for the MIDI port
    unique_swd_string DeviceInterfaceId; // SWD interface ID for the MIDI port
    std::wstring InstanceId;
    std::wstring Enumerator;
    HRESULT hr{ S_OK };
} MIDIPORT, *PMIDIPORT;



typedef struct _MIDIPARENTDEVICE
{
    SWDEVICESTATE SwDeviceState{ SWDEVICESTATE::NotCreated };   // SWD creation state
    unique_hswdevice SwDevice{};                                // Handle to the SWD created for the MIDI parent device
    unique_swd_string DeviceId{};                               // SWD device ID for this MIDI parent device
    std::wstring InstanceId{};                                  // created instance id
    //std::wstring Name{};                                        // friendly name for this device
    HRESULT hr{ S_OK };

} MIDIPARENTDEVICE, * PMIDIPARENTDEVICE;



class CMidiDeviceManager  : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDeviceManagerInterface>
{
public:

    CMidiDeviceManager() {}
    ~CMidiDeviceManager() {}

    STDMETHOD(Initialize)(
        _In_ std::shared_ptr<CMidiPerformanceManager>& performanceManager,
        _In_ std::shared_ptr<CMidiEndpointProtocolManager>& EndpointProtocolManager,
        _In_ std::shared_ptr<CMidiConfigurationManager>& configurationManager);


    STDMETHOD(ActivateVirtualParentDevice)(
        _In_ ULONG DevPropertyCount,
        _In_opt_ PVOID DevProperties,
        _In_ PVOID CreateInfo,
        _Out_writes_opt_z_(CreatedSwDeviceIdCharCount) PWSTR CreatedSwDeviceId,
        _In_  ULONG CreatedSwDeviceIdCharCount
    );



    STDMETHOD(ActivateEndpoint)(
        _In_ PCWSTR,
        _In_ BOOL,
        _In_ MidiFlow,
        _In_ PMIDIENDPOINTCOMMONPROPERTIES,
        _In_ ULONG,
        _In_ ULONG,
        _In_ PVOID,
        _In_ PVOID,
        _In_ PVOID,
        _Out_writes_opt_z_(CreatedSwDeviceInterfaceIdCharCount) PWSTR CreatedDeviceInterfaceId,
        _In_ ULONG CreatedSwDeviceInterfaceIdWCharCount
    );

    STDMETHOD(DeactivateEndpoint)(_In_ PCWSTR);

    STDMETHOD(RemoveEndpoint)(_In_ PCWSTR);

    STDMETHOD(UpdateEndpointProperties)(
        _In_ PCWSTR,
        _In_ ULONG,
        _In_ PVOID
        );

    STDMETHOD(DeleteEndpointProperties)(
        _In_ PCWSTR,
        _In_ ULONG,
        _In_ PVOID
        );

    STDMETHOD(DeleteAllEndpointInProtocolDiscoveredProperties)(
        _In_ PCWSTR
        );


    // this is for runtime updates only, not for config file updates
    STDMETHOD(UpdateAbstractionConfiguration)(
        _In_ GUID AbstractionId,
        _In_ LPCWSTR ConfigurationJson,
        _In_ BOOL IsFromConfigurationFile,
        _Out_ BSTR* Response
        );


    STDMETHOD(Cleanup)();

    //TODO: Method to update the properties (using SwDevicePropertySet and an array of props) for a device by its Id

private:


    HRESULT ActivateEndpointInternal(
        _In_ PCWSTR,
        _In_opt_ PCWSTR,
        _In_ BOOL,
        _In_ MidiFlow,
        _In_ ULONG,
        _In_ ULONG,
        _In_ DEVPROPERTY*,
        _In_ DEVPROPERTY*,
        _In_ SW_DEVICE_CREATE_INFO*,
        _In_opt_ std::wstring*
    );

    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiConfigurationManager> m_ConfigurationManager;

    std::map<GUID, wil::com_ptr_nothrow<IMidiEndpointManager>, GUIDCompare> m_MidiEndpointManagers;
    std::map<GUID, wil::com_ptr_nothrow<IMidiAbstractionConfigurationManager>, GUIDCompare> m_MidiAbstractionConfigurationManagers;

    wil::critical_section m_MidiPortsLock;
    std::vector<std::unique_ptr<MIDIPORT>> m_MidiPorts;

    std::vector<std::unique_ptr<MIDIPARENTDEVICE>> m_MidiParents;
};

