// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


// TODO: Need to consider maps that work more like:
// 
// message type
//    group
//       status
//       channel
//          other CV properties
//
// and then each would have a destination with transforms
//
// That way, we can route messages to destinations based on channel etc, but also
// be able to change the group/channel 
// 
// We may be able to genercize the transform side into a proper MIDI Transform
//

class CMidi2VirtualPatchBayRoutingEntry :
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

    STDMETHOD(Shutdown)();

private:
    std::wstring m_configIdentifier;

    std::wstring m_name;
    std::wstring m_description;

    bool m_fullyResolved{ false };    // true if we've been able to resolve all the devices in here

    std::vector<CMidi2VirtualPatchBayRoutingSource> m_messageSources;
    std::vector<CMidi2VirtualPatchBayRoutingDestination> m_messageDestinations;

};