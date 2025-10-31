// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#ifndef UMP_ITERATOR_H
#define UMP_ITERATOR_H

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


        inline UmpBufferIterator() { throw std::runtime_error("Must be constructed with a pointer to the UMP data."); }

        inline UmpBufferIterator(_In_ pointer bufferPointer, _In_ pointer copiedSentinal) :
            current(bufferPointer),
            start(bufferPointer),
            sentinal(copiedSentinal)
        { }

        inline UmpBufferIterator(_In_ pointer bufferPointer, _In_ uint32_t wordCount) :
            current(bufferPointer),
            start(bufferPointer),
            sentinal(bufferPointer + wordCount)
        { }


        reference operator*() const { return *current; }
        pointer operator->() const { return current; }

        inline UmpBufferIterator& operator++()
        {
            // this is where we need to read the type and increment by the right number of words
            if (current != sentinal)
            {
                current += CurrentMessageWordCount();
                return *this;
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        inline UmpBufferIterator operator++(int)
        {
            UmpBufferIterator temp = *this;

            operator++();

            return temp;
        }

        inline bool operator==(_In_ const UmpBufferIterator& other) const { return current == other.current; }
        inline bool operator!=(_In_ const UmpBufferIterator& other) const { return !(*this == other); }

        inline bool operator<(_In_ const UmpBufferIterator& other) const
        {
            return this->current < other.current;
        }

        inline bool operator>(_In_ const UmpBufferIterator& other) const
        {
            return this->current > other.current;
        }

        inline bool operator>=(_In_ const UmpBufferIterator& other) const
        {
            return this->current >= other.current;
        }

        inline bool operator<=(_In_ const UmpBufferIterator& other) const
        {
            return this->current <= other.current;
        }

        inline UmpBufferIterator begin() { return UmpBufferIterator(start, sentinal); }
        inline UmpBufferIterator end() { return UmpBufferIterator(sentinal, sentinal); }   // feels wrong

        inline pointer get()
        {
            return current;
        }

        inline uint32_t GetCurrentMessageWord(_In_ uint8_t const wordIndex) const
        {
            if (current != sentinal)
            {
                if (CurrentMessageSeemsComplete() && wordIndex < GetUmpLengthInMidiWordsFromFirstWord(*current))
                {
                    return *(current + wordIndex);
                }

                throw std::runtime_error("Word index out of range for this message.");

            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        inline void CopyCurrentMessageToVector(_In_ std::vector<uint32_t>& destination) const
        {
            if (current != sentinal)
            {
                for (int i = 0; i < GetUmpLengthInMidiWordsFromFirstWord(*current); ++i)
                {
                    destination.push_back(*(current + i));
                }

                return;
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        inline uint8_t CurrentMessageType() const
        {
            if (current != sentinal)
            {
                return GetUmpMessageTypeFromFirstWord(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        // return the number of words in the message pointed to by current
        inline uint8_t CurrentMessageWordCount() const
        {
            if (current != sentinal)
            {
                return GetUmpLengthInMidiWordsFromFirstWord(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        inline uint8_t CurrentMessageGroupIndex() const
        {
            if (current != sentinal)
            {
                return GetGroupIndexFromFirstWord(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        inline uint32_t RemainingBufferWordCount() const
        {
            if (current < sentinal)
            {
                return static_cast<uint32_t>(sentinal - current);
            }

            return 0;
        }

        // return true if the message type pointed to by current has a group field
        inline bool CurrentMessageHasGroupField() const
        {
            if (current != sentinal)
            {
                return MessageHasGroupField(*current);
            }

            throw std::runtime_error("Invalid UMP. Past end of buffer.");
        }

        // returns true if there are enough words left to complete this message
        inline bool CurrentMessageSeemsComplete() const
        {
            if (current < sentinal)
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




    inline bool ValidateBufferHasCompleteUmps(_In_ uint32_t* buffer, _In_ uint32_t wordCount)
    {
        if (buffer == nullptr || wordCount == 0)
        {
            return false;
        }

        UmpBufferIterator iterator(buffer, wordCount);

        for (auto it = iterator.begin(); it < iterator.end(); ++it)
        {
            if (!it.CurrentMessageSeemsComplete())
            {
                return false;
            }
        }

        return true;
    }

}

#endif