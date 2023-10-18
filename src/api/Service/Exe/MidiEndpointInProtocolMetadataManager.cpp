#include "stdafx.h"

#include "MidiEndpointInProtocolMetadataManager.h"

_Use_decl_annotations_
HRESULT CMidiEndpointInProtocolMetadataManager::Initialize(
	std::shared_ptr<IMidiBiDi>& /*endpointBiDi */ , 
	PCWSTR /*endpointDeviceId*/, 
	uint8_t /*preferredMidiProtocolVersion*/,
	bool /*handleProtocolNegotiation*/,
	bool /*handleFunctionBlocks*/)

{
	return S_OK;
}

_Use_decl_annotations_
HRESULT CMidiEndpointInProtocolMetadataManager::ConfigureMidi2Device(
	uint16_t /*timeoutMsPerAttempt*/,
	uint8_t /*retryCount*/)
{



	return S_OK;
}



HRESULT CMidiEndpointInProtocolMetadataManager::Cleanup()
{

	// clear cache and tear everything down

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMidiEndpointInProtocolMetadataManager::Callback(
    PVOID /*Data*/,
    UINT /*Length*/,
    LONGLONG /*Position*/
)
{
	// handle incoming messages


	return S_OK;
}
