#pragma once





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
