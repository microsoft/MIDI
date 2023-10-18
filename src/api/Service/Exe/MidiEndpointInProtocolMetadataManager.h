#pragma once

#include <map>


// todo: need to bring over all the relevant message types and make them work here. Same 
// with builders for those types. Have a shared def file
// todo: move this enum out to a shared file
enum MidiFunctionBlockDiscoveryFilterFlags
{
    None = 0x00000000,
    RequestFunctionBlockInformation = 0x00000001,
    RequestFunctionBlockName = 0x00000002,
};



// intent is to instantiate one of these for each connected unique device endpoint.
//

class CMidiEndpointInProtocolMetadataManager : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:

    CMidiEndpointInProtocolMetadataManager() {}
    ~CMidiEndpointInProtocolMetadataManager() {}

    HRESULT Initialize(
        _In_ std::shared_ptr<IMidiBiDi>& endpointBiDi, 
        _In_ PCWSTR endpointDeviceId,
        _In_ uint8_t preferredMidiProtocolVersion,
        _In_ bool handleProtocolNegotiation,
        _In_ bool handleFunctionBlocks);

    // performs all the protocol negotiation required and then
    // requests endpoint info and function blocks. Updates the
    // SWD with the metadata from the discovery. Returns S_OK
    // when all the initial discovery is complete
    HRESULT ConfigureMidi2Device(
        _In_ uint16_t timeoutMsPerAttempt, 
        _In_ uint8_t retryCount);


    // TODO: need to have something that will listen for additional
    // function blocks or endpoint info notifications and update
    // the SWD. This can come at any-time, so this would live as
    // long as the endpoint does


    HRESULT Cleanup();


    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);


private:
    // All metadata is cached as strings (for names/ids) or JSON (for structures)

    // map is property key and then json data
    //std::map<std::string, std::string> m_endpointMetadata;


};

