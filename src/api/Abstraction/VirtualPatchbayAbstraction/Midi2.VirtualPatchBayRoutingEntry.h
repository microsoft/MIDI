// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once




class CMidiVirtualPatchBayRoutingEntry :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiCallback>
{
public:
    STDMETHOD(Initialize)(_In_ LPCWSTR associationId, _In_ LPCWSTR name, _In_ LPCWSTR description);

    STDMETHOD(AddSource)(_In_ CMidi2VirtualPatchBayRoutingSource* source);
    STDMETHOD(AddDestination)(_In_ CMidi2VirtualPatchBayRoutingDestination* destination);

    // all sources call this. This then replicates the messages to all destinations.
    // destinations are responsible for any group mapping etc.
    STDMETHOD(Callback)(_In_ PVOID data, _In_ UINT length, _In_ LONGLONG position, _In_ LONGLONG context);

    STDMETHOD(Cleanup)();

private:
    std::wstring associationId;

    std::wstring Name;
    std::wstring Description;

    bool FullyResolved{ false };    // true if we've been able to resolve all the devices in here

    VirtualPatchBayRouter m_router{ };

    std::vector<CMidi2VirtualPatchBayRoutingSource> m_messageSources;
    std::vector<CMidi2VirtualPatchBayRoutingDestination> m_messageDestinations;


    //~CMidiVirtualPatchBayRoutingEntry()
    //{
    //    MessageSources.clear();
    //    MessageDestinations.clear();
    //}
};