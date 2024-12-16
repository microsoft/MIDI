// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
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
using shared_hswdevice = wil::shared_any<unique_hswdevice>;
using unique_swd_string = wil::unique_any<PWSTR, decltype(&::SwMemFree), ::SwMemFree>;

typedef struct _MIDIPORT
{
    MidiFlow Flow{ MidiFlowIn };
    const GUID* InterfaceCategory{ nullptr };
    SWDEVICESTATE SwDeviceState{ CreatePending };         // SWD creation state
    shared_hswdevice SwDevice;           // Handle to the SWD created for the MIDI port
    unique_swd_string DeviceInterfaceId; // SWD interface ID for the MIDI port
    std::wstring InstanceId;
    std::wstring Enumerator;
    std::wstring ParentInstanceId;
    std::wstring DeviceDescription;
    GUID ContainerId;
    HRESULT hr{ S_OK };
    bool MidiOne {false};
    UINT32 GroupIndex{0};
    UINT32 CustomPortNumber {0};
    std::wstring AssociatedInterfaceId;
} MIDIPORT, *PMIDIPORT;

typedef struct _MIDIPARENTDEVICE
{
    SWDEVICESTATE SwDeviceState{ SWDEVICESTATE::NotCreated };   // SWD creation state
    shared_hswdevice SwDevice;                                  // Handle to the SWD created for the MIDI parent device
    unique_swd_string DeviceId{};                               // SWD device ID for this MIDI parent device
    std::wstring InstanceId{};                                  // created instance id
    //std::wstring Name{};                                      // friendly name for this device
    HRESULT hr{ S_OK };

} MIDIPARENTDEVICE, * PMIDIPARENTDEVICE;

typedef struct _PORT_INFO
{
    bool InUse {false};
    bool IsEnabled {false};
    bool HasCustomPortNumber{false};
    UINT32 CustomPortNumber {0};
    std::wstring InterfaceId;
    MidiFlow Flow {MidiFlowIn};
    std::wstring Name;          // added PMB 2024-12-15 due to compile error after merge
} PORT_INFO;

class CMidiDeviceManager  : 
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiDeviceManagerInterface>
{
public:

    CMidiDeviceManager() {}
    ~CMidiDeviceManager() {}

    STDMETHOD(Initialize)(
        _In_ std::shared_ptr<CMidiPerformanceManager>&,
        _In_ std::shared_ptr<CMidiEndpointProtocolManager>&,
        _In_ std::shared_ptr<CMidiConfigurationManager>&);

    STDMETHOD(ActivateVirtualParentDevice)(
        _In_ ULONG,
        _In_opt_ const DEVPROPERTY*,
        _In_ const SW_DEVICE_CREATE_INFO*,
        _Out_opt_ LPWSTR*
    );

    STDMETHOD(ActivateEndpoint)(
        _In_ PCWSTR,
        _In_ BOOL,
        _In_ MidiFlow,
        _In_ PMIDIENDPOINTCOMMONPROPERTIES,
        _In_ ULONG,
        _In_ ULONG,
        _In_ const DEVPROPERTY* ,
        _In_ const DEVPROPERTY* ,
        _In_ const SW_DEVICE_CREATE_INFO*,
        _Out_opt_ LPWSTR*
    );

    STDMETHOD(DeactivateEndpoint)(_In_ PCWSTR);

    STDMETHOD(RemoveEndpoint)(_In_ PCWSTR);

    STDMETHOD(UpdateEndpointProperties)(
        _In_ PCWSTR,
        _In_ ULONG,
        _In_ const DEVPROPERTY*
        );

    STDMETHOD(DeleteEndpointProperties)(
        _In_ PCWSTR,
        _In_ ULONG,
        _In_ const DEVPROPERTY*
        );

    //STDMETHOD(DeleteAllEndpointInProtocolDiscoveredProperties)(
    //    _In_ PCWSTR
    //    );


    // this is for runtime updates only, not for config file updates
    STDMETHOD(UpdateTransportConfiguration)(
        _In_ GUID,
        _In_ LPCWSTR,
        _Out_ LPWSTR*
        );

    STDMETHOD(Shutdown)();

    //TODO: Method to update the properties (using SwDevicePropertySet and an array of props) for a device by its Id

private:


    HRESULT ActivateEndpointInternal(
        _In_ PCWSTR,
        _In_opt_ PCWSTR,
        _In_ BOOL,
        _In_ UINT32,
        _In_ MidiFlow,
        _In_ ULONG,
        _In_ ULONG,
        _In_ const DEVPROPERTY*,
        _In_ const DEVPROPERTY*,
        _In_ const SW_DEVICE_CREATE_INFO*,
        _In_opt_ std::wstring*,
        _Out_ PMIDIPORT* createdPort
    );

    HRESULT AssignPortNumber(
        _In_ HSWDEVICE,
        _In_ PWSTR,
        _In_ MidiFlow
    );

    HRESULT GetFunctionBlockPortInfo(
        _In_ LPCWSTR umpDeviceInterfaceId,
        _In_ std::map<UINT32, PORT_INFO> portInfo[2]
    );

    HRESULT GetGroupTerminalBlockPortInfo(
        _In_ LPCWSTR umpDeviceInterfaceId,
        _In_ std::map<UINT32, PORT_INFO> portInfo[2]
    );

    HRESULT UseFallbackMidi1PortDefinition(
        _In_ LPCWSTR umpDeviceInterfaceId,
        _In_ std::map<UINT32, PORT_INFO> portInfo[2]
    );

    HRESULT GetCustomPortMapping(
        _In_ LPCWSTR umpDeviceInterfaceId,
        _In_ std::map<UINT32, PORT_INFO> portInfo[2]
    );

    HRESULT SyncMidi1Ports(
        PMIDIPORT umpMidiPort
    );

    std::shared_ptr<CMidiPerformanceManager> m_performanceManager;
    std::shared_ptr<CMidiConfigurationManager> m_configurationManager;

    std::map<GUID, wil::com_ptr_nothrow<IMidiEndpointManager>, GUIDCompare> m_midiEndpointManagers;
    std::map<GUID, wil::com_ptr_nothrow<IMidiTransportConfigurationManager>, GUIDCompare> m_midiTransportConfigurationManagers;

    wil::critical_section m_midiPortsLock;
    std::vector<std::unique_ptr<MIDIPORT>> m_midiPorts;

    std::vector<std::unique_ptr<MIDIPARENTDEVICE>> m_midiParents;
};

