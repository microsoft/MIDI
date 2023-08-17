#pragma once
class MidiStateService
{
public:

    // returns the json for function blocks
    std::wstring GetFunctionBlockEntriesForEndpoint(_In_ std::wstring endpointDeviceId);
    bool UpdateFunctionBlockEntriesForEndpoint(_In_ std::wstring endpointDeviceId, _In_ std::wstring functionBlocksJson);

    // other cache types, MUID table, etc.



private:
};

