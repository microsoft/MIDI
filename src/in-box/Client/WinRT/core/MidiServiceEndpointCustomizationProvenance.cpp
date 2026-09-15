// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceEndpointCustomizationProvenance.h"
#include "ServiceConfig.MidiServiceEndpointCustomizationProvenance.g.cpp"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    namespace
    {
        winrt::hstring NowAsIso8601() noexcept
        {
            try
            {
                SYSTEMTIME now{};
                ::GetSystemTime(&now);

                return winrt::hstring{ std::format(
                    L"{:04}-{:02}-{:02}T{:02}:{:02}:{:02}Z",
                    now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond) };
            }
            catch (...)
            {
                return L"";
            }
        }
    }

    _Use_decl_annotations_
    midi2::ServiceConfig::MidiServiceEndpointCustomizationProvenance MidiServiceEndpointCustomizationProvenance::CreateForEndpoint(
        midi2enum::MidiEndpointDeviceInformation const& endpointDeviceInformation) noexcept
    {
        auto provenance = winrt::make_self<MidiServiceEndpointCustomizationProvenance>();

        try
        {
            if (endpointDeviceInformation == nullptr)
            {
                return *provenance;
            }

            provenance->Created(NowAsIso8601());

            // The name the customer would recognize. Their own name for the endpoint if they have
            // already given it one, otherwise what the transport called it.
            provenance->CreatedFor(endpointDeviceInformation.Name());
            provenance->ParentDeviceInstanceId(endpointDeviceInformation.ParentDeviceInstanceId());

            if (auto const transportInfo = endpointDeviceInformation.GetTransportSuppliedInfo())
            {
                provenance->UsbVendorId(transportInfo.VendorId());
                provenance->UsbProductId(transportInfo.ProductId());
                provenance->UsbSerialNumber(transportInfo.SerialNumber());
                provenance->ManufacturerName(transportInfo.ManufacturerName());
                provenance->TransportSuppliedName(transportInfo.Name());
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(nullptr, ex, L"hresult error building customization provenance.");
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception building customization provenance.");
        }

        return *provenance;
    }


    winrt::hstring MidiServiceEndpointCustomizationProvenance::GetConfigJson() const noexcept
    {
        try
        {
            json::JsonObject provenanceObject;

            if (m_provenance->WriteJson(provenanceObject))
            {
                return provenanceObject.Stringify();
            }

            return L"";
        }
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error getting provenance config json.");
            return L"";
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception getting provenance config json.");
            return L"";
        }
    }
}
