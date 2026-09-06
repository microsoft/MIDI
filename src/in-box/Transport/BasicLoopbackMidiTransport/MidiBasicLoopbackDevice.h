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
    std::shared_ptr<MidiBasicLoopbackDeviceDefinition> Definition;


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
        std::shared_ptr<MidiBasicLoopbackDeviceDefinition> definition;
        {
            auto lock = m_lock.lock_shared();
            callback = m_callback;
            definition = Definition;
        }

        if (!definition || definition->IsMuted) return S_OK;
        if (callback == nullptr) return S_OK;

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
        Definition.reset();

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

    wil::srwlock m_lock;
    wil::com_ptr_nothrow<IMidiCallback> m_callback{ nullptr };

    std::atomic<uint64_t> m_messageCount{ 0 };

};

#pragma pop_macro("SendMessage")