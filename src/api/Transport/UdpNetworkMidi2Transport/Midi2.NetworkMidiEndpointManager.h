// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class CMidi2NetworkMidiEndpointManager :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiEndpointManager>

{
public:
    STDMETHOD(Initialize(_In_ IMidiDeviceManagerInterface*, _In_ IMidiEndpointProtocolManagerInterface*));
    STDMETHOD(Shutdown)();

    STDMETHOD(InitiateDiscoveryAndNegotiation(_In_ std::wstring const& endpointDeviceInterfaceId));

    STDMETHOD(CreateNewEndpoint(
        _In_ std::wstring const& endpointName,
        _In_ std::wstring const& remoteEndpointProductInstanceId,
        _In_ winrt::Windows::Networking::HostName const& hostName,
        _In_ std::wstring const& networkPort,
        _Out_ std::wstring& createdNewDeviceInstanceId,
        _Out_ std::wstring& createdNewEndpointDeviceInterfaceId
    ));

    STDMETHOD(DeleteEndpoint(_In_ std::wstring deviceInstanceId));

    bool IsInitialized() { return m_initialized; }

private:
    bool m_initialized{ false };

    GUID m_containerId{};
    GUID m_transportId{ };
    std::wstring m_parentDeviceId{};

    HRESULT CreateParentDevice();


    wil::com_ptr_nothrow<IMidiDeviceManagerInterface> m_midiDeviceManager;
    wil::com_ptr_nothrow<IMidiEndpointProtocolManagerInterface> m_midiProtocolManager;
};
