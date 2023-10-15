#include "stdafx.h"

#include "MidiEndpointMetadataManager.h"

_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::Initialize(
	std::shared_ptr<IMidiBiDi>& /*endpointBiDi */ , 
	PCWSTR /*endpointDeviceId*/, 
	bool /*handleProtocolNegotiation*/,
	bool /*handleFunctionBlocks*/)

{
	return S_OK;
}

HRESULT CMidiEndpointMetadataManager::BeginDiscovery()
{
	// check to see if data is already in cache
	// if the data is there, then we just exit

	// otherwise, send the endpoint discovery to start things off


	return S_OK;
}


_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::BeginNegotiation(
	uint8_t /*preferredMidiProtocolVersion*/)
{

	// if the cached endpoint protocol is different from what we prefer
	// then negotiate the new protocol. Do this only once for any given
	// negotiation session.

	return S_OK;
}



HRESULT CMidiEndpointMetadataManager::Cleanup()
{

	// clear cache and tear everything down

	return S_OK;
}


_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::Callback(
    PVOID /*Data*/,
    UINT /*Length*/,
    LONGLONG /*Position*/
)
{
	// handle incoming messages




	return S_OK;
}
