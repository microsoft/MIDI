// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiMessage64.h"
#include "MidiMessage64.g.cpp"


namespace winrt::Windows::Devices::Midi2::implementation
{
    _Use_decl_annotations_
    midi2::MidiMessage64 MidiMessage64::CreateFromStruct(
        internal::MidiTimestamp const timestamp,
        MidiMessageStruct const& message) noexcept
    {
        try
        {
            return midi2::MidiMessage64(timestamp, message.Word0, message.Word1);
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult error creating MidiMessage64 from struct.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(nullptr, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"General exception creating MidiMessage64 from struct.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }


    collections::IVector<uint32_t> MidiMessage64::GetAllWords() const noexcept
    {
        try
        {
            auto vec = winrt::single_threaded_vector<uint32_t>();

            vec.Append(m_ump.word0);
            vec.Append(m_ump.word1);

            return vec;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult error building the words vector.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return winrt::single_threaded_vector<uint32_t>();
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"General exception building the words vector.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return winrt::single_threaded_vector<uint32_t>();
        }
    }

    _Use_decl_annotations_
    uint8_t MidiMessage64::AppendAllMessageWordsToList(collections::IVector<uint32_t> targetVector) const noexcept
    {
        try
        {
            targetVector.Append(m_ump.word0);
            targetVector.Append(m_ump.word1);

            return 2;
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult error appending words to the list.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return 0;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"General exception appending words to the list.", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return 0;
        }
    }

    _Use_decl_annotations_
    uint8_t MidiMessage64::FillBuffer(uint32_t const byteOffset, foundation::IMemoryBuffer const& buffer) const noexcept
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
        catch (winrt::hresult_error const& ex)
        {
            MIDI_SDK_LOG_HRESULT_EXCEPTION(this, ex, L"hresult error filling buffer.");
            return 0;
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(this, L"General exception filling buffer.");
            return 0;
        }
    }


    _Use_decl_annotations_
        MidiMessage64::MidiMessage64(
        internal::MidiTimestamp const timestamp, 
        uint32_t const word0, 
        uint32_t const word1)
    {
        m_timestamp = timestamp;

        m_ump.word0 = word0;
        m_ump.word1 = word1;
    }

    // internal constructor for reading from the service callback
    _Use_decl_annotations_
    void MidiMessage64::InternalInitializeFromPointer(
        internal::MidiTimestamp const timestamp, 
        PVOID data)
    {
        if (data == nullptr) return;

        m_timestamp = timestamp;

        // need to have some safeties around this
        memcpy((void*)&m_ump, data, sizeof(internal::PackedUmp64));
    }


    winrt::hstring MidiMessage64::ToString()
    {
        std::stringstream stream;

        stream << "64-bit MIDI message:"
            << " 0x" << std::hex << std::setw(8) << std::setfill('0') << Word0()
            << " 0x" << std::hex << std::setw(8) << std::setfill('0') << Word1();

        return winrt::to_hstring(stream.str());
    }



}

