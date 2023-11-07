#include "stdafx.h"

#include "MidiEndpointInProtocolMetadataManager.h"




// UMP version is driven by what the Windows API and service implementation supports, so it's hard-coded here
internal::PackedUmp128 MidiEndpointInProtocolMetadataManager::BuildEndpointDiscoveryMessage()
{
	internal::PackedUmp128 ump;

	ump.word0 = 0xF0000101; // UMP 1.1

	ump.word1 = 0x0000001F; // request everything (Endpoint Info, Device Identity, Endpoint Name, Product Instance Id, Stream Configuration)
	ump.word2 = 0;
	ump.word3 = 0;

	return ump;
}


_Use_decl_annotations_
internal::PackedUmp128 MidiEndpointInProtocolMetadataManager::BuildFunctionBlockDiscoveryMessage(
	uint8_t functionBlockNumber)
{
	internal::PackedUmp128 ump;

	uint8_t filter = 0x03;  // request block info and block name

	ump.word0 = 0xF0100000 | ((uint16_t)functionBlockNumber << 8) | filter;

	ump.word1 = 0;
	ump.word2 = 0;
	ump.word3 = 0;

	return ump;
}


_Use_decl_annotations_
internal::PackedUmp128 MidiEndpointInProtocolMetadataManager::BuildStreamConfigurationRequestMessage(
	uint8_t protocol, 
	bool rxjr, 
	bool txjr)
{
	internal::PackedUmp128 ump;

	ump.word0 = 0xF0050000 | ((uint16_t)protocol << 8);

	if (rxjr) ump.word0 |= 0x00000002;
	if (txjr) ump.word0 |= 0x00000001;

	ump.word1 = 0;
	ump.word2 = 0;
	ump.word3 = 0;

	return ump;
}



_Use_decl_annotations_
HRESULT MidiEndpointInProtocolMetadataManager::Initialize(
	PCWSTR endpointDeviceId 
	)

{
	m_endpointDeviceId = endpointDeviceId;

	return S_OK;
}

HRESULT MidiEndpointInProtocolMetadataManager::Cleanup()
{

	// clear cache and tear everything down. Null out callback / endpoint

	return S_OK;
}



_Use_decl_annotations_
HRESULT MidiEndpointInProtocolMetadataManager::DiscoverMidi2Endpoint(
	uint16_t timeoutMsPerAttempt,
	uint8_t retryCount)
{
	m_discoveryEndpointInfoNotificationReceived = false;
	m_discoveryDeviceIdentityNotificationReceived = false;
	m_discoveryEndpointNameNotificationSetReceived = false;
	m_discoveryEndpointProductInstanceIdNotificationSetReceived = false;
	m_discoveryStreamConfigurationNotificationReceived = false;

	m_discoveryComplete.open(L"DiscoveryCompleteEvent");

	uint16_t totalTries = retryCount + 1;

	for (uint16_t i = 0; i < totalTries; i++)
	{
		// clear current endpoint info and stream configuration info
		
		// build endpoint discovery message
		



		// send endpoint discovery and wait for a response (callback sets the  flags)

		m_discoveryComplete.wait(timeoutMsPerAttempt);

		// TODO: Also wait for stream configuration notification

		// TODO: Also wait for device identity notification

		// TODO: Endpoint Name and Product Instance Id are all multi-message streams
	}

	// Store the metadata in the SWD

	//m_functionBlocksAreStatic = staticBlocks;
	//m_functionBlockCount = blockCount;


	// reset the event. Is this correct? Need to recreate or reopen afterwards?
	m_discoveryComplete.reset();

	return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointInProtocolMetadataManager::ConfigureMidi2Stream(
	uint8_t /*preferredMidiProtocolVersion*/,
	uint16_t /*timeoutMsPerAttempt*/,
	uint8_t /*retryCount*/)
{

	// Send Stream Configuration Request

	// Wait for Stream Configuration Notification or timeout


	return S_OK;
}


HRESULT MidiEndpointInProtocolMetadataManager::ProcessMessage(
	_In_ PVOID /*data*/,
	_In_ UINT /*size*/,
	_In_ LONGLONG /*timestamp*/
)
{
	// handle incoming messages

	// set appropriate boolean flags

	// set appropriate events

	return S_OK;
}

