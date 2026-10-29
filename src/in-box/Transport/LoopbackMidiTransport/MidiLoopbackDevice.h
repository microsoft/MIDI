// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// The table keeps a copy of the device built at creation, so a copy gets a lock of its own instead of sharing one.
class MidiLoopbackCallbackLock
{
public:
    MidiLoopbackCallbackLock() = default;
    MidiLoopbackCallbackLock(_In_ MidiLoopbackCallbackLock const&) noexcept {}
    MidiLoopbackCallbackLock& operator=(_In_ MidiLoopbackCallbackLock const&) noexcept { return *this; }

    wil::srwlock Lock;
};

// represents a loopback device. The device has exactly two
// endpoints which are cross-wired to each other

class MidiLoopbackDevice
{
public:
    bool IsMuted{ false };

    MidiLoopbackDeviceDefinition DefinitionA;
    MidiLoopbackDeviceDefinition DefinitionB;
    MidiLoopbackDevice() = default;

    // A pointer because the table keeps a copy of the device built at creation, and both
    // copies must share one set of guards.
    std::shared_ptr<MidiLoopbackFeedback> Feedback{};

    MidiLoopbackDevice(const MidiLoopbackDevice& rhs)
    {
        IsMuted = rhs.IsMuted;
        DefinitionA = rhs.DefinitionA;
        DefinitionB = rhs.DefinitionB;

        auto lock = const_cast<wil::critical_section&>(rhs.m_callbackLock).lock();
        m_callbackA = rhs.m_callbackA;
        m_callbackB = rhs.m_callbackB;
    }

    MidiLoopbackDevice& operator=(const MidiLoopbackDevice& rhs)
    {
        if (this == &rhs)
        {
            return *this;
        }

        IsMuted = rhs.IsMuted;
        DefinitionA = rhs.DefinitionA;
        DefinitionB = rhs.DefinitionB;

        wil::com_ptr_nothrow<IMidiCallback> callbackA;
        wil::com_ptr_nothrow<IMidiCallback> callbackB;
        {
            auto rhsLock = const_cast<wil::critical_section&>(rhs.m_callbackLock).lock();
            callbackA = rhs.m_callbackA;
            callbackB = rhs.m_callbackB;
        }

        auto thisLock = m_callbackLock.lock();
        m_callbackA = callbackA;
        m_callbackB = callbackB;

        return *this;
    }

    void Shutdown()
    {
        if (Feature_Servicing_MIDI2LoopbackAddRemoveReliability::IsEnabled())
        {
            wil::com_ptr_nothrow<IMidiCallback> releasedA{ nullptr };
            wil::com_ptr_nothrow<IMidiCallback> releasedB{ nullptr };
            
            {
                auto lock = m_callbackLock.Lock.lock_exclusive();
            
                releasedA.swap(m_callbackA);
                releasedB.swap(m_callbackB);
            }
            
            // released here, outside the lock
        }
        else
        {
            auto lock = m_callbackLock.lock();
            if (m_callbackA != nullptr)
            {
                m_callbackA = nullptr;
            }
            if (m_callbackB != nullptr)
            {
                m_callbackB = nullptr;
            }
        }
    }

    void RegisterEndpointA(/*_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> endpoint,*/ _In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        if (Feature_Servicing_MIDI2LoopbackAddRemoveReliability::IsEnabled())
        {
            {
                auto lock = m_callbackLock.Lock.lock_exclusive();
                m_callbackA.swap(callback);
            }

            // callback now holds the one it replaced, released here, outside the lock
        }
        else
        {
            auto lock = m_callbackLock.lock();
            //m_bidiA = endpoint;
            m_callbackA = callback;
        }
    }

    void RegisterEndpointB(/*_In_ wil::com_ptr_nothrow<CMidi2LoopbackMidiBidi> endpoint,*/ _In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        if (Feature_Servicing_MIDI2LoopbackAddRemoveReliability::IsEnabled())
        {
            {
                auto lock = m_callbackLock.Lock.lock_exclusive();
                m_callbackA.swap(callback);
            }

            // callback now holds the one it replaced, released here, outside the lock
        }
        else
        {
            auto lock = m_callbackLock.lock();
            //m_bidiB = endpoint;
            m_callbackB = callback;
        }
    }

    // Holds its own reference to the destination for the whole delivery, and keeps the muting and feedback
    // protection behavior of the four Send functions below.
    HRESULT SendMessageWithLock(
        _In_ bool const directionAToB,
        _In_ PVOID message,
        _In_ UINT size,
        _In_ LONGLONG position,
        _In_ LONGLONG context)
    {
        wil::com_ptr_nothrow<IMidiCallback> destination{ nullptr };

        {
            auto lock = m_callbackLock.Lock.lock_shared();
            destination = directionAToB ? m_callbackB : m_callbackA;
        }

        if (destination == nullptr)
        {
            return S_OK;
        }

        if (Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
        {
            if (IsMuted)
            {
                return S_OK;
            }

            if (Feature_Servicing_MIDI2LoopbackFeedbackProtection::IsEnabled())
            {
                return SendThroughFeedbackGuard(directionAToB, destination, message, size, position, context);
            }
        }

        return destination->Callback(MessageOptionFlags_None, message, size, position, context);
    }

    HRESULT SendMessageAToB(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        wil::com_ptr_nothrow<IMidiCallback> callback;
        {
            auto lock = m_callbackLock.lock();
            callback = m_callbackB;
        }
        if (Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
        {
            if (callback != nullptr && !IsMuted)
            {
                return callback->Callback(MessageOptionFlags_None, message, size, position, context);
            }
        }
        else
        {
            if (callback != nullptr)
            {
                return callback->Callback(MessageOptionFlags_None, message, size, position, context);
            }
        }

        return S_OK;
    }

    HRESULT SendMessageBToA(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        wil::com_ptr_nothrow<IMidiCallback> callback;
        {
            auto lock = m_callbackLock.lock();
            callback = m_callbackA;
        }
        if (Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
        {
            if (callback != nullptr && !IsMuted)
            {
                return callback->Callback(MessageOptionFlags_None, message, size, position, context);
            }
        }
        else
        {
            if (callback != nullptr)
            {
                return callback->Callback(MessageOptionFlags_None, message, size, position, context);
            }
        }

        return S_OK;
    }

    // Protection acts by muting, so where muting has been rolled back it stands aside and this is the shipped path.
    HRESULT SendMessageAToBWithFeedbackProtection(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        if (Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
        {
            if (m_callbackB != nullptr && !IsMuted)
            {
                return SendThroughFeedbackGuard(true, m_callbackB, message, size, position, context);
            }
        }
        else
        {
            if (m_callbackB != nullptr)
            {
                return m_callbackB->Callback(MessageOptionFlags_None, message, size, position, context);
            }
        }

        return S_OK;
    }

    HRESULT SendMessageBToAWithFeedbackProtection(_In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        if (Feature_Servicing_MIDI2LoopbackMuteAndList::IsEnabled())
        {
            if (m_callbackA != nullptr && !IsMuted)
            {
                return SendThroughFeedbackGuard(false, m_callbackA, message, size, position, context);
            }
        }
        else
        {
            if (m_callbackA != nullptr)
            {
                return m_callbackA->Callback(MessageOptionFlags_None, message, size, position, context);
            }
        }

        return S_OK;
    }

    ~MidiLoopbackDevice()
    {
        //m_bidiA = nullptr;
        //m_bidiB = nullptr;

        Shutdown();
    }

private:
    // With protection off the guard is not touched at all, so there is no test and no cost.
    HRESULT SendThroughFeedbackGuard(
        _In_ bool const directionAToB,
        _In_ wil::com_ptr_nothrow<IMidiCallback> const& destination,
        _In_ PVOID message,
        _In_ UINT size,
        _In_ LONGLONG position,
        _In_ LONGLONG context)
    {
        // Set once at creation, and the caller holds the device for the whole send.
        auto const& feedback = Feedback;

        if (feedback != nullptr && feedback->IsEnabled())
        {
            auto const& guard = directionAToB ? feedback->AToB() : feedback->BToA();

            if (guard != nullptr)
            {
                return guard->Send(destination.get(), MessageOptionFlags_None, message, size, position, context);
            }
        }

        return destination->Callback(MessageOptionFlags_None, message, size, position, context);
    }

    wil::com_ptr_nothrow<IMidiCallback> m_callbackA{ nullptr };
    wil::com_ptr_nothrow<IMidiCallback> m_callbackB{ nullptr };
    wil::critical_section m_callbackLock;
};
