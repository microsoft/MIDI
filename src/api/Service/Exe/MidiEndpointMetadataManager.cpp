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


_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::BeginDiscovery()
{
	return S_OK;
}


_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::BeginNegotiation(
	uint8_t /*preferredMidiProtocolVersion*/)
{
	return S_OK;
}



_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::Cleanup()
{
	return S_OK;
}


_Use_decl_annotations_
HRESULT CMidiEndpointMetadataManager::Callback(
    PVOID /*Data*/,
    UINT /*Length*/,
    LONGLONG /*Position*/
)
{
	return S_OK;
}
