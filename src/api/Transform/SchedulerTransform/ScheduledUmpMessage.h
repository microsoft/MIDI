// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

struct ScheduledUmpMessage
{
    internal::MidiTimestamp Timestamp{ 0 };
    uint32_t ReceivedIndex{ 0 };            // this allows us to preserve order for messages with the same timestamp
    UINT ByteCount{ 0 };
    //BYTE Data[MAXIMUM_UMP_DATASIZE];        // pre-define this array to avoid another allocation/indirection
    std::vector<uint8_t> Data{};

    
    ScheduledUmpMessage(
        _In_ internal::MidiTimestamp timestamp, 
        _In_ uint32_t receivedIndex, 
        _In_ UINT byteCount, 
        _In_reads_bytes_(byteCount) BYTE* data)
    {
        Data.resize(byteCount);

        if (Data.size() == byteCount)
        {
            memcpy(Data.data(), data, byteCount);
            ByteCount = byteCount;

            Timestamp = timestamp;
            ReceivedIndex = receivedIndex;
        }
        else
        {
            // couldn't allocate the required memory
        }
    }

};

