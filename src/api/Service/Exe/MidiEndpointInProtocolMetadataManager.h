#pragma once

#include <map>






// TODO: ParseDeviceIdentityNotificationMessage

// TODO: Parse Endpoint Name Notification

// TODO: Parse Product Instance Id

// TODO: Parse Function Block Info notification

// TODO: Parse Function Block Name Notification






// Instantiate one of these for each connected unique device endpoint.
// It manages the data for only one device endpoint

class MidiEndpointInProtocolMetadataManager
{
public:

    MidiEndpointInProtocolMetadataManager() { m_discoveryComplete.create(); }
    ~MidiEndpointInProtocolMetadataManager() {}

    // TODO: Probably need the handle to the endpoint so we can update metadata
    HRESULT Initialize(
        _In_ PCWSTR endpointDeviceId
    );



    // Performs endpoint discovery. Updates the
    // SWD proeprties with the metadata from the discovery
    // Returns S_OK when The endpoint info notification and 
    // Stream configuration notifications have been received, 
    // or the transaction times out.
    HRESULT DiscoverMidi2Endpoint(
        _In_ uint16_t timeoutMsPerAttempt,
        _In_ uint8_t retryCount
    );


    // Performs protocol negotiation required and then. Returns S_OK
    // when all the negotiation is complete. Updates the SWD properties
    // with the protocol information.
    HRESULT ConfigureMidi2Stream(
        _In_ uint8_t preferredMidiProtocolVersion,
        _In_ uint16_t timeoutMsPerAttempt,
        _In_ uint8_t retryCount
    );


    // TODO: need to have something that will listen for additional
    // function blocks or endpoint info notifications and update
    // the SWD. This can come at any-time, so this would live as
    // long as the endpoint does




    // the device callback calls this when a message arrives from the device
    HRESULT ProcessMessage(
        _In_ PVOID data, 
        _In_ UINT size, 
        _In_ LONGLONG timestamp
    );




    HRESULT Cleanup();


private:
    wil::com_ptr<IMidiBiDi> m_endpointDeviceBiDi;

    std::wstring m_endpointDeviceId{};

    bool m_functionBlocksAreStatic{ false };
    uint8_t m_functionBlockCount{ 0 };


    wil::unique_event_nothrow m_discoveryComplete;
    bool m_discoveryEndpointInfoNotificationReceived{ false };
    bool m_discoveryStreamConfigurationNotificationReceived{ false };

    bool m_discoveryDeviceIdentityNotificationReceived{ false };
    bool m_discoveryEndpointNameNotificationSetReceived{ false };
    bool m_discoveryEndpointProductInstanceIdNotificationSetReceived{ false };


    // All metadata is cached as strings (for names/ids) or JSON (for structures)

    // map is property key and then json data
    //std::map<std::string, std::string> m_endpointMetadata;




    internal::PackedUmp128 BuildEndpointDiscoveryMessage();
    internal::PackedUmp128 BuildFunctionBlockDiscoveryMessage(_In_ uint8_t functionBlockNumber);
    internal::PackedUmp128 BuildStreamConfigurationRequestMessage(_In_ uint8_t protocol, _In_ bool rxjr, _In_ bool txjr);


};

