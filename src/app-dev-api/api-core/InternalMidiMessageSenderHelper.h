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

namespace implementation = winrt::Windows::Devices::Midi2::implementation;


namespace Windows::Devices::Midi2::Internal
{
	template<typename TEndpoint>
	class InternalMidiMessageSenderHelper
	{
	public:
		bool SendMessageRaw(winrt::com_ptr<TEndpoint> endpoint, void* data, uint32_t sizeInBytes, uint64_t timestamp);
		bool SendUmp(winrt::com_ptr<TEndpoint> endpoint, winrt::Windows::Devices::Midi2::IMidiUmp const& ump);

	private:
		void* GetUmpDataPointer(winrt::Windows::Devices::Midi2::IMidiUmp const& ump, uint32_t& dataSizeOut);

	};


    // implementation must be in header (or otherwise included) for template classes


    template <typename TEndpoint>
    bool InternalMidiMessageSenderHelper<TEndpoint>::SendMessageRaw(winrt::com_ptr<TEndpoint> endpoint, void* data, uint32_t sizeInBytes, uint64_t timestamp)
    {
        if (endpoint != nullptr)
        {
            winrt::check_hresult(endpoint->SendMidiMessage(data, sizeInBytes, timestamp));

            return true;
        }

        return false;
    }



    template <typename TEndpoint>
    void* InternalMidiMessageSenderHelper<TEndpoint>::GetUmpDataPointer(winrt::Windows::Devices::Midi2::IMidiUmp const& ump, uint32_t& dataSizeOut)
    {
        void* umpDataPointer{};
        dataSizeOut = 0;

        switch (ump.MidiUmpPacketType())
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
    bool InternalMidiMessageSenderHelper<TEndpoint>::SendUmp(com_ptr<TEndpoint> endpoint, winrt::Windows::Devices::Midi2::IMidiUmp const& ump)
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
                std::cout << __FUNCTION__ << " _bidiEndpoint is nullptr" << std::endl;

                return false;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            //std::cout << __FUNCTION__ << " hresult exception sending message" << std::endl;
            //std::cout << "HRESULT: 0x" << std::hex << (uint32_t)(ex.code()) << std::endl;
            //std::cout << "Message: " << winrt::to_string(ex.message()) << std::endl;

            return false;
        }
    }



}
