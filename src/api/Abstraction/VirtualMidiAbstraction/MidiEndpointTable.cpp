// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"


MidiEndpointTable::MidiEndpointTable() = default;
MidiEndpointTable::~MidiEndpointTable() = default;

MidiEndpointTable& MidiEndpointTable::Current()
{
    // explanation: http://www.modernescpp.com/index.php/thread-safe-initialization-of-data/

    static MidiEndpointTable current;

    return current;
}


//HRESULT
//GetDeviceVirtualEndpointAssociationId(_In_ std::wstring deviceId, _Inout_ std::wstring& associationId)
//{
//    OutputDebugString(__FUNCTION__ L" enter");
//
//    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
//    additionalProperties.Append(winrt::to_hstring(STRING_PKEY_MIDI_SupportedDataFormats));
//    auto deviceInfo = DeviceInformation::CreateFromIdAsync(MidiDevice, additionalProperties, winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();
//
//    auto prop = deviceInfo.Properties().Lookup(winrt::to_hstring(STRING_PKEY_MIDI_SupportedDataFormats));
//    if (prop)
//    {
//        OutputDebugString(__FUNCTION__ L" found property");
//
//        // this interface is pointing to a UMP interface, so use that instance id.
//        DataFormat = (MidiDataFormat)winrt::unbox_value<uint8_t>(prop);
//    }
//    else
//    {
//        OutputDebugString(__FUNCTION__ L" didn't find property");
//        // default to any
//        DataFormat = MidiDataFormat::MidiDataFormat_Any;
//    }
//
//    OutputDebugString(__FUNCTION__ L" exiting OK");
//
//    return S_OK;
//}
//
//
//
//
//
//
//
//
//







HRESULT MidiEndpointTable::Cleanup()
{
    OutputDebugString(__FUNCTION__ L"");

    m_endpointManager = nullptr;
    m_endpoints.clear();

    return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointTable::AddCreatedEndpointDevice(MidiVirtualDeviceEndpointEntry& entry) noexcept
{
    OutputDebugString(__FUNCTION__ L"");

    // index by the association Id
    std::wstring cleanId = internal::ToUpperTrimmedWStringCopy(entry.VirtualEndpointAssociationId);

    m_endpoints[cleanId] = entry;

    return S_OK;
}

std::wstring GetStringSWDProperty(_In_ std::wstring instanceId, _In_ std::wstring propertyName, _In_ std::wstring defaultValue)
{
    auto propertyKey = winrt::to_hstring(propertyName.c_str());

    auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
    additionalProperties.Append(propertyKey);


    auto deviceInfo = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
        winrt::to_hstring(instanceId.c_str()), 
        additionalProperties, 
        winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

    auto prop = deviceInfo.Properties().Lookup(propertyKey);

    if (prop)
    {
        OutputDebugString(__FUNCTION__ L" found property");

        // this interface is pointing to a UMP interface, so use that instance id.
        return (winrt::unbox_value<winrt::hstring>(prop)).c_str();
    }
    else
    {
        OutputDebugString(__FUNCTION__ L" didn't find property");
        // default to any
        return defaultValue;
    }

}

_Use_decl_annotations_
HRESULT 
MidiEndpointTable::OnClientConnected(std::wstring clientInstanceId, CMidi2VirtualMidiBiDi* clientBiDi)
{
    // get the device BiDi, and then wire them together

    try
    {
        OutputDebugString(__FUNCTION__ L"");

        std::wstring cleanId = internal::ToUpperTrimmedWStringCopy(clientInstanceId);

        // look up the association ID in SWD properties

        auto associationId = internal::ToUpperTrimmedWStringCopy(GetStringSWDProperty(cleanId, STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L""));

        if (associationId != L"")
        {
            // find this id in the table
            auto entry = m_endpoints[associationId];

            if (entry.MidiClientBiDi == nullptr)
            {
                entry.MidiClientBiDi = clientBiDi;
                entry.CreatedClientEndpointId = cleanId;

                entry.MidiDeviceBiDi->LinkAssociatedBiDi(clientBiDi);
                clientBiDi->LinkAssociatedBiDi(entry.MidiDeviceBiDi);

                m_endpoints[associationId] = entry;
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}


_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceConnected(std::wstring deviceInstanceId, CMidi2VirtualMidiBiDi* deviceBiDi)
{
    try
    {
        OutputDebugString(__FUNCTION__ L"");

        // look up the association ID in SWD properties
        std::wstring cleanId = internal::ToUpperTrimmedWStringCopy(deviceInstanceId);
        auto associationId = internal::ToUpperTrimmedWStringCopy(GetStringSWDProperty(cleanId, STRING_PKEY_MIDI_VirtualMidiEndpointAssociator, L""));

        if (associationId != L"")
        {
            // find this id in the table
            auto entry = m_endpoints[associationId];

            if (entry.MidiDeviceBiDi == nullptr)
            {
                OutputDebugString(__FUNCTION__ L" - no registered device bidi yet\n");

                entry.MidiDeviceBiDi = deviceBiDi;
                m_endpoints[associationId] = entry;

                // if we have an endpoint manager, go ahead and create the client endpoint
                if (m_endpointManager)
                {
                    OutputDebugString(__FUNCTION__ L" - Creating client endpoint");

                    // create the client endpoint
                    RETURN_IF_FAILED(m_endpointManager->CreateClientVisibleEndpoint(entry));

                    OutputDebugString(__FUNCTION__ L" - Client endpoint created");

                    m_endpoints[associationId] = entry;




//                    LOG_IF_FAILED(m_endpointManager->NegotiateAndRequestMetadata(entry.CreatedClientEndpointId));




                }
                else
                {
                    OutputDebugString(__FUNCTION__ L" - Endpoint Manager is null");

                    return E_FAIL;
                }
            }
            else
            {
                // already created. Exit. This happens during protocol negotiation.
            }
        }
    }
    CATCH_RETURN();

    return S_OK;
}

_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceDisconnected(std::wstring /*deviceInstanceId*/)
{
    OutputDebugString(__FUNCTION__ L"");

    //std::wstring deviceEndpointId{ deviceInstanceId };
    //internal::InPlaceToUpper(deviceEndpointId);


    // tear down the client device and BiDi as well

    return S_OK;
}


