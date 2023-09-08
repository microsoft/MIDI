// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include "MidiUmp32.h"
#include "MidiUmp64.h"
#include "MidiUmp96.h"
#include "MidiUmp128.h"

#include <iostream>

namespace implementation = winrt::Windows::Devices::Midi2::implementation;


namespace Windows::Devices::Midi2::Internal
{
    template<typename TEndpoint>
    class InternalMidiMessageSenderHelper
    {
    public:
        bool SendMessageRaw(_In_ winrt::com_ptr<TEndpoint> endpoint, _In_ void* data, _In_ uint32_t sizeInBytes, _In_ internal::MidiTimestamp timestamp);
        bool SendUmp(_In_ winrt::com_ptr<TEndpoint> endpoint, _In_ winrt::Windows::Devices::Midi2::IMidiUmp const& ump);

    private:
        void* GetUmpDataPointer(winrt::Windows::Devices::Midi2::IMidiUmp const& ump, uint32_t& dataSizeOut);

    };


    // implementation must be in header (or otherwise included) for template classes


    template <typename TEndpoint>
    bool InternalMidiMessageSenderHelper<TEndpoint>::SendMessageRaw(
        _In_ winrt::com_ptr<TEndpoint> endpoint, 
        _In_ void* data, 
        _In_ uint32_t sizeInBytes, 
        _In_ internal::MidiTimestamp timestamp)
    {
        try
        {
            if (endpoint != nullptr)
            {
                winrt::check_hresult(endpoint->SendMidiMessage(data, sizeInBytes, timestamp));

                return true;
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            return false;
        }
    }


    template <typename TEndpoint>
    void* InternalMidiMessageSenderHelper<TEndpoint>::GetUmpDataPointer(
        _In_ winrt::Windows::Devices::Midi2::IMidiUmp const& ump, 
        _In_ uint32_t& dataSizeOut)
    {
        void* umpDataPointer{};
        dataSizeOut = 0;

        switch (ump.UmpPacketType())
        {
        case winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump32:
            dataSizeOut = (uint32_t)sizeof(internal::PackedUmp32);
            umpDataPointer = ump.as<implementation::MidiUmp32>()->GetInternalUmpDataPointer();
            break;
        case winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump64:
            dataSizeOut = (uint32_t)sizeof(internal::PackedUmp64);
            umpDataPointer = ump.as<implementation::MidiUmp64>()->GetInternalUmpDataPointer();
            break;
        case winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump96:
            dataSizeOut = (uint32_t)sizeof(internal::PackedUmp96);
            umpDataPointer = ump.as<implementation::MidiUmp96>()->GetInternalUmpDataPointer();
            break;
        case winrt::Windows::Devices::Midi2::MidiUmpPacketType::Ump128:
            dataSizeOut = (uint32_t)sizeof(internal::PackedUmp128);
            umpDataPointer = ump.as<implementation::MidiUmp128>()->GetInternalUmpDataPointer();
            break;
        }

        return umpDataPointer;
    }


    template <typename TEndpoint>
    bool InternalMidiMessageSenderHelper<TEndpoint>::SendUmp(
        _In_ com_ptr<TEndpoint> endpoint, 
        _In_ winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
    {
        try
        {
            if (endpoint != nullptr)
            {
                uint32_t umpDataSize{};

                auto umpDataPointer = GetUmpDataPointer(ump, umpDataSize);

                return SendMessageRaw(endpoint, umpDataPointer, umpDataSize, ump.Timestamp());
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"endpoint is nullptr");

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult error sending message. Is the service running?", ex);

            return false;
        }
    }



}
