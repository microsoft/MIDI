#pragma once

#include <map>


// TODO: Need to define a callback or event to tell the client that some data was updated
// Although initial protocol negotiation happens at the start, it can be re-requested at
// any time. Additionally, function block and endpoint info notifications can happen at 
// any time. Name updates should 


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

class CMidiEndpointMetadataManager : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:

    CMidiEndpointMetadataManager() {}
    ~CMidiEndpointMetadataManager() {}

    HRESULT Initialize(
        _In_ std::shared_ptr<IMidiBiDi>& endpointBiDi, 
        _In_ PCWSTR endpointDeviceId,
        _In_ bool handleProtocolNegotiation,
        _In_ bool handleFunctionBlocks);

    // discovery gets all the endpoint information and also requests the function blocks
    HRESULT BeginDiscovery();

    // protocol negotiation happens when the preferred MIDI protocol differs from what
    // the connected UMP endpoint is reporting
    HRESULT BeginNegotiation(_In_ uint8_t preferredMidiProtocolVersion);

    HRESULT Cleanup();

 //   std::string GetAllMetadataJson();


    STDMETHOD(Callback)(_In_ PVOID, _In_ UINT, _In_ LONGLONG);

    // TODO: Callback or other approach for endpoint name update so we can update PnP

private:
    // All metadata is cached as strings (for names/ids) or JSON (for structures)

    // map is property key and then json data
    std::map<std::string, std::string> m_endpointMetadata;


};

