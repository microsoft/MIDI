// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

// represents a loopback device. The device has exactly two
// endpoints which are cross-wired to each other

#pragma push_macro("SendMessage")
#undef SendMessage

class MidiBasicLoopbackDevice
{
public:
    MidiBasicLoopbackDevice(
        _In_ std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> definition,
        _In_ std::shared_ptr<MidiBasicLoopbackFeedback> feedback) :
        Feedback{ std::move(feedback) },
        m_definition{ std::move(definition) }
    {
    }

    MidiBasicLoopbackDevice(_In_ MidiBasicLoopbackDevice const&) = delete;
    MidiBasicLoopbackDevice& operator=(_In_ MidiBasicLoopbackDevice const&) = delete;

    // Set once, before the device is published to the table, and never changed.
    std::shared_ptr<MidiBasicLoopbackFeedback> const Feedback;

    // A definition is never changed once the device holds it. A change makes a new one and swaps
    // it in, so a caller can read the snapshot it got here for as long as it likes. Null once the
    // device has been shut down.
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> GetDefinition() const noexcept
    {
        auto lock = m_lock.lock_shared();
        return m_definition;
    }

    // Copies the current definition, applies the change and swaps the copy in, all under the lock,
    // so two changes can't lose one another. Returns null, and changes nothing, once the device
    // has been shut down. Throws only when out of memory.
    template <typename Change>
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> ChangeDefinition(_In_ Change&& change)
    {
        auto lock = m_lock.lock_exclusive();

        if (m_definition == nullptr)
        {
            return nullptr;
        }

        auto changed = std::make_shared<MidiBasicLoopbackDeviceDefinition>(*m_definition);
        change(*changed);

        m_definition = changed;

        return changed;
    }


    HRESULT Initialize(_In_ wil::com_ptr_nothrow<IMidiCallback> callback)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, callback);

        auto lock = m_lock.lock_exclusive();

        m_callback = callback;

        return S_OK;
    }

    // Detach the current client connection's callback WITHOUT tearing down the
    // device itself. The device (and its Definition) is owned by the endpoint
    // table and must outlive individual client connections so the endpoint can
    // be opened again later. Called on client disconnect (Bidi::Shutdown).
    HRESULT DisconnectClient()
    {
        auto lock = m_lock.lock_exclusive();

        m_callback = nullptr;

        return S_OK;
    }

    HRESULT SendMessage(_In_ MessageOptionFlags optionFlags, _In_ PVOID message, _In_ UINT size, _In_ LONGLONG position, _In_ LONGLONG context)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, message);
        RETURN_HR_IF(E_INVALIDARG, size < sizeof(uint32_t));

        // Snapshot the callback and definition under the lock so a concurrent
        // Shutdown() (which clears both) can't free them out from under us
        // while we're forwarding the message. We deliberately release the
        // lock before invoking the callback to avoid holding it across a
        // potentially long, re-entrant call into client code.
        wil::com_ptr_nothrow<IMidiCallback> callback;
        std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> definition;
        {
            auto lock = m_lock.lock_shared();
            callback = m_callback;
            definition = m_definition;
        }

        if (!definition || definition->IsMuted) return S_OK;
        if (callback == nullptr) return S_OK;

        auto const& feedback = Feedback;

        if (feedback != nullptr && feedback->IsEnabled() && feedback->Guard() != nullptr)
        {
            bool dropped{ false };

            RETURN_IF_FAILED(feedback->Guard()->Send(callback.get(), optionFlags, message, size, position, context, &dropped));

            // held messages are counted now, because they go out later in order
            if (!dropped)
            {
                m_messageCount.fetch_add(CountUmpMessages(message, size), std::memory_order_relaxed);
            }

            return S_OK;
        }

        RETURN_IF_FAILED(callback->Callback(optionFlags, message, size, position, context));

        m_messageCount.fetch_add(CountUmpMessages(message, size), std::memory_order_relaxed);

        return S_OK;
    }

    // Running total of UMP messages carried from the destination back to the source. Only
    // messages which were actually delivered are counted, so a muted or unopened loopback
    // reads zero rather than pretending to pass traffic.
    uint64_t MessageCount() const noexcept { return m_messageCount.load(std::memory_order_relaxed); }

    // Full teardown of the device. Only the endpoint table (RemoveDevice /
    // table Shutdown) should call this, when the endpoint itself is being
    // destroyed -- NOT on a per-connection disconnect.
    HRESULT Shutdown()
    {
        auto lock = m_lock.lock_exclusive();

        m_callback = nullptr;
        m_definition.reset();

        return S_OK;
    }

    ~MidiBasicLoopbackDevice()
    {
        Shutdown();
    }

private:
    // A single send can carry several messages, and a count of buffers would make a burst of
    // notes look identical to one long system exclusive.
    static uint64_t CountUmpMessages(_In_ PVOID const message, _In_ UINT const size) noexcept
    {
        auto const words = reinterpret_cast<uint32_t const*>(message);
        auto const wordCount = size / sizeof(uint32_t);

        uint64_t count{ 0 };

        for (size_t index = 0; index < wordCount; )
        {
            auto const length = internal::GetUmpLengthInMidiWordsFromFirstWord(words[index]);

            count++;

            // A length which does not fit means the buffer is not what it claims to be. Stop
            // rather than walking off the end or spinning on a zero length.
            if (length == 0 || index + length > wordCount)
            {
                break;
            }

            index += length;
        }

        return count;
    }

    // Guards the callback and which definition is current. Taken after the endpoint table's lock
    // and after a feedback object's status lock, never before either.
    mutable wil::srwlock m_lock;
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition const> m_definition{ nullptr };

    std::atomic<uint64_t> m_messageCount{ 0 };

};

#pragma pop_macro("SendMessage")