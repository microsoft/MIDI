// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "MidiMessage32.h"
#include "MidiMessage32.g.cpp"



namespace winrt::Windows::Devices::Midi2::implementation
{
    collections::IVector<uint32_t> MidiMessage32::GetAllWords() const noexcept
    {
        auto vec = winrt::single_threaded_vector<uint32_t>();

        vec.Append(m_ump.word0);

        return vec;
    }

    _Use_decl_annotations_
    uint8_t MidiMessage32::AppendAllMessageWordsToList(collections::IVector<uint32_t> targetVector) const noexcept
    {
        targetVector.Append(m_ump.word0);

        return 1;
    }

    _Use_decl_annotations_
    uint8_t MidiMessage32::FillBuffer(uint32_t const byteOffset, foundation::IMemoryBuffer const& buffer) const noexcept
    {
        const uint8_t numWordsInPacket = 2;
        const uint8_t numBytesInPacket = numWordsInPacket * sizeof(uint32_t);

        try
        {
            auto ref = buffer.CreateReference();
            auto interop = ref.as<IMemoryBufferByteAccess>();

            uint8_t* value{};
            uint32_t valueSize{};

            // get a pointer to the buffer
            if (SUCCEEDED(interop->GetBuffer(&value, &valueSize)))
            {
                if (byteOffset + numBytesInPacket > valueSize)
                {
                    // no room
                    return 0;
                }
                else
                {
                    uint32_t* bufferWordPointer = reinterpret_cast<uint32_t*>(value + byteOffset);

                    // copy the number of valid bytes in our internal UMP structure
                    memcpy(bufferWordPointer, &m_ump, numBytesInPacket);

                    return numBytesInPacket;
                }
            }
            else
            {
                return 0;
            }

        }
        catch (...)
        {
            return 0;
        }
    }


    _Use_decl_annotations_
    MidiMessage32::MidiMessage32(
        internal::MidiTimestamp const timestamp, 
        uint32_t const word0)
    {
        m_timestamp = timestamp;
        m_ump.word0 = word0;
    }

    // internal constructor for reading from the service callback
    // we needed a second function here because overload with the other
    // constructor caused access violations when wrong one was picked
    _Use_decl_annotations_
    void MidiMessage32::InternalInitializeFromPointer(
        internal::MidiTimestamp timestamp, 
        PVOID data)
    {
        if (data == nullptr) return;

        m_timestamp = timestamp;

        memcpy((void*)&m_ump, data, sizeof(internal::PackedUmp32));
    }


    winrt::hstring MidiMessage32::ToString()
    {
        std::stringstream stream;

        stream << "32-bit MIDI message:" 
            << " 0x" << std::hex << std::setw(8) << std::setfill('0') << Word0();

        return winrt::to_hstring(stream.str());
    }


}
