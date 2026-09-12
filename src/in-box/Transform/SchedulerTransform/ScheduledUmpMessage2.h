// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// Used only by CMidi2SchedulerMidiTransform2. Unlike ScheduledUmpMessage this carries the option
// flags through, and holds the payload inline so enqueue does not allocate while under the lock.
struct ScheduledUmpMessage2
{
    internal::MidiTimestamp Timestamp{ 0 };
    uint64_t ReceivedIndex{ 0 };            // preserves arrival order among equal timestamps
    MessageOptionFlags OptionFlags{ MessageOptionFlags_None };
    UINT ByteCount{ 0 };
    BYTE Data[MAXIMUM_UMP_DATASIZE]{};

    ScheduledUmpMessage2() = default;

    ScheduledUmpMessage2(
        _In_ internal::MidiTimestamp const timestamp,
        _In_ uint64_t const receivedIndex,
        _In_ MessageOptionFlags const optionFlags,
        _In_ UINT const byteCount,
        _In_reads_bytes_(byteCount) BYTE const* const data)
    {
        if (data != nullptr && byteCount >= MINIMUM_UMP_DATASIZE && byteCount <= MAXIMUM_UMP_DATASIZE)
        {
            memcpy(Data, data, byteCount);

            ByteCount = byteCount;
            Timestamp = timestamp;
            ReceivedIndex = receivedIndex;
            OptionFlags = optionFlags;
        }
    }
};

// Smallest timestamp on top, arrival order preserved within a timestamp. A stateless functor
// rather than a function pointer, so the comparison inlines.
struct ScheduledUmpMessage2Comparer
{
    bool operator()(_In_ ScheduledUmpMessage2 const& left, _In_ ScheduledUmpMessage2 const& right) const noexcept
    {
        if (left.Timestamp == right.Timestamp)
        {
            return left.ReceivedIndex > right.ReceivedIndex;
        }

        return left.Timestamp > right.Timestamp;
    }
};
