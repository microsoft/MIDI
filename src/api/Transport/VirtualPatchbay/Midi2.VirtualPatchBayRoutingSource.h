// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once



class CMidi2VirtualPatchBayRoutingSource :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    STDMETHOD(Initialize)(_In_ LPCWSTR endpointMatchJson, _In_ CMidi2VirtualPatchBayRoutingEntry* router);

    STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT length, _In_ LONGLONG position, _In_ LONGLONG context);

    STDMETHOD(SetIncludedGroups)(_In_ std::vector<uint8_t> groupIndexes);
    STDMETHOD(SetIncludedMessageTypes)(_In_ std::vector<uint8_t> messageTypes);
    STDMETHOD(SetEndpointCallback)(_In_ LPCWSTR resolvedEndpointDeviceInterfaceId, _In_ IMidiCallback* sourceEndpointCallback);

    STDMETHOD(Cleanup)();

private:
    CMidi2VirtualPatchBayRoutingEntry* m_router;

    std::wstring m_matchJson{ };

    GUID m_endpointAbstractionId{ };
    std::wstring m_resolvedEndpointDeviceInterfaceId{ };

    // array of booleans indexed by message type. If true, the type is included
    std::array<bool, 16> m_includedMessageTypes{ true };
    
    // array of booleans. If an index is true, it is included
    std::array<bool, 16> m_includedGroupIndexes{ true };


    wil::com_ptr<IMidiBidirectional> m_sourceEndpointBidi{ nullptr };

};

