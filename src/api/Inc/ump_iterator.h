// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <stdint.h>
#include <cstddef>
#include <iterator>

#include "ump_helpers.h"


namespace WindowsMidiServicesInternal
{
    class UmpBufferIterator
    {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = uint32_t;
        using pointer = uint32_t*;
        using reference = uint32_t&;


        UmpBufferIterator() { throw std::runtime_error("Must be constructed with a pointer to the UMP data."); }

        UmpBufferIterator(pointer bufferPointer, pointer copiedSentinal) :
            current(bufferPointer),
            start(bufferPointer),
            sentinal(copiedSentinal)
        {
        }

        UmpBufferIterator(pointer bufferPointer, uint32_t wordCount) :
            current(bufferPointer),
            start(bufferPointer),
            sentinal(bufferPointer + wordCount)
        {
        }


        reference operator*() const { return *current; }
        pointer operator->() const { return current; }

        UmpBufferIterator& operator++()
        {
            // this is where we need to read the type and increment by the right number of words
            if (current != sentinal)
            {
                current += CurrentMessageWordCount();
                return *this;
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        UmpBufferIterator operator++(int)
        {
            UmpBufferIterator temp = *this;

            operator++();

            return temp;
        }

        bool operator==(const UmpBufferIterator& other) const { return current == other.current; }
        bool operator!=(const UmpBufferIterator& other) const { return !(*this == other); }

        bool operator<(const UmpBufferIterator& other)
        {
            return this->current < other.current;
        }

        bool operator>(const UmpBufferIterator& other)
        {
            return this->current > other.current;
        }

        bool operator>=(const UmpBufferIterator& other)
        {
            return this->current >= other.current;
        }

        bool operator<=(const UmpBufferIterator& other)
        {
            return this->current <= other.current;
        }

        UmpBufferIterator begin() { return UmpBufferIterator(start, sentinal); }
        UmpBufferIterator end() { return UmpBufferIterator(sentinal, sentinal); }   // feels wrong

        pointer get()
        {
            return current;
        }

        // return the number of words in the message pointed to by current
        uint8_t CurrentMessageWordCount()
        {
            if (current != sentinal)
            {
                return GetUmpLengthInMidiWordsFromFirstWord(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        uint8_t CurrentMessageGroupIndex()
        {
            if (current != sentinal)
            {
                return GetGroupIndexFromFirstWord(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        uint32_t RemainingBufferWordCount()
        {
            if (current != sentinal)
            {
                return static_cast<uint32_t>(sentinal - current);
            }

            return 0;
        }

        // return true if the message type pointed to by current has a group field
        bool CurrentMessageHasGroupField()
        {
            if (current != sentinal)
            {
                return MessageHasGroupField(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        // returns true if there are enough words left to complete this message
        bool CurrentMessageSeemsComplete()
        {
            if (current != sentinal)
            {
                return RemainingBufferWordCount() >= CurrentMessageWordCount();
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }


    private:
        pointer current, start, sentinal;

    };

    // validate
    static_assert(std::forward_iterator<UmpBufferIterator>);



}
