// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_SEQUENCE_NUMBER_H
#define MIDI_SEQUENCE_NUMBER_H

// we shouldn't rely on undefined behavior like unsigned wrapping, even though it usually works, 
// so this type handles sequence number wrapping and math

class MidiSequenceNumber
{
public:
    // this is the number we use when trying to compare two sequence numbers
    const uint16_t ComparisonDelta = 60000;

    MidiSequenceNumber() : m_value(0) { }
    MidiSequenceNumber(_In_ uint16_t const v) : m_value(v) { }
    MidiSequenceNumber(_In_ MidiSequenceNumber const& v) : m_value(v.m_value) { }

    MidiSequenceNumber& operator=(const MidiSequenceNumber& rhs)
    {
        m_value = rhs.m_value;

        return *this;
    }

    MidiSequenceNumber operator+ (_In_ const MidiSequenceNumber value) const
    {
        uint32_t val = m_value + value.m_value;

        return MidiSequenceNumber(static_cast<uint16_t>(val % Max()));
    }

    MidiSequenceNumber operator+ (_In_ uint16_t const value) const
    {
        uint32_t val = m_value + value;

        return MidiSequenceNumber(static_cast<uint16_t>(val % Max()));
    }


    MidiSequenceNumber operator- (_In_ MidiSequenceNumber const value) const
    {
        int32_t val = m_value - value.m_value;

        if (val >= 0)
        {
            return MidiSequenceNumber(static_cast<uint16_t>(val % Max()));
        }
        else
        {
            return MidiSequenceNumber(static_cast<uint16_t>(Max() - (val % Max())));
        }
    }

    MidiSequenceNumber operator- (_In_ uint16_t const value) const
    {
        int32_t val = m_value - value;

        if (val >= 0)
        {
            return MidiSequenceNumber(static_cast<uint16_t>(val % Max()));
        }
        else
        {
            return MidiSequenceNumber(static_cast<uint16_t>(Max() - (val % Max())));
        }
    }

    //postfix val++
    MidiSequenceNumber operator++ (int)
    {
        MidiSequenceNumber temp = *this;

        Increment();

        return temp;
    }

    //prefix ++val
    MidiSequenceNumber& operator++ ()
    {
        Increment();

        return *this;
    }

    //postfix val--
    MidiSequenceNumber operator-- (int)
    {
        MidiSequenceNumber temp = *this;
        
        Decrement();

        return temp;
    }

    //prefix --val
    MidiSequenceNumber& operator-- ()
    {
        Decrement();

        return *this;
    }


    void operator= (_In_ uint16_t const rhs)
    {
        m_value = rhs;
    }


    bool operator== (_In_ MidiSequenceNumber const& rhs) const
    {
        return m_value == rhs.m_value;
    }

    bool operator > (_In_ MidiSequenceNumber const& rhs) const
    {
        return IsLaterThan(rhs);
    }

    bool operator>= (_In_ MidiSequenceNumber const& rhs) const
    {
        return m_value == rhs.m_value || IsLaterThan(rhs);
    }

    bool operator < (_In_ MidiSequenceNumber const& rhs) const
    {
        return IsEarlierThan(rhs);
    }

    bool operator <= (_In_ MidiSequenceNumber const& rhs) const
    {
        return m_value == rhs.m_value || IsEarlierThan(rhs);
    }

    bool IsEarlierThan(_In_ MidiSequenceNumber const& value) const
    {
        // if our value is greater than the comparison value by more than the comparison delta size, it is considered to be older/earlier (wrapped)
        // if our value is less than the comparison value by more than the comparison delta size, it is considered to be newer/later (wrapped)

        // we promote up to uint32_t to avoid an undefined wrapping behavior when calculating
        uint32_t comparisonValue = static_cast<uint32_t>(value.m_value);
        uint32_t ourValue = static_cast<uint32_t>(m_value);

        if (ourValue > comparisonValue)
        {
            if (ourValue - comparisonValue > ComparisonDelta)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else if (ourValue < comparisonValue)
        {
            if (comparisonValue - ourValue > ComparisonDelta)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else
        {
            // they are equal.
            return false;
        }
    }

    bool IsLaterThan(_In_ MidiSequenceNumber const& value) const
    {
        // if our value is greater than the comparison value by more than the comparison delta size, it is considered to be older/earlier (wrapped)
        // if our value is less than the comparison value by more than the comparison delta size, it is considered to be newer/later (wrapped)

        // we promote up to uint32_t to avoid an undefined wrapping behavior when calculating
        uint32_t comparisonValue = static_cast<uint32_t>(value.m_value);
        uint32_t ourValue = static_cast<uint32_t>(m_value);

        if (ourValue > comparisonValue)
        {
            if (ourValue - comparisonValue > ComparisonDelta)
            {
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (ourValue < comparisonValue)
        {
            if (comparisonValue - ourValue > ComparisonDelta)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            // they are equal.
            return false;
        }
    }

    void Increment()
    {
        if (m_value == Max())
        {
            m_value = 0;
        }
        else
        {
            m_value++;
        }
    }

    void Decrement()
    {
        if (m_value == 0)
        {
            m_value = Max();
        }
        else
        {
            m_value--;
        }
    }

    void Decrement(_In_ uint16_t amount)
    {
        // todo: this is lazy on my part
        for (uint16_t i = 0; i < amount; i++)
        {
            Decrement();
        }
    }

    void Increment(_In_ uint16_t amount)
    {
        // todo: this is lazy on my part
        for (uint16_t i = 0; i < amount; i++)
        {
            Increment();
        }
    }

    uint16_t Value()
    {
        return m_value;
    }


    static uint16_t Max()
    {
#pragma push_macro("max")
#undef max
        return std::numeric_limits<uint16_t>::max();
#pragma pop_macro("max")
    }

private:
    uint16_t m_value{ 0 };
};


#endif