// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Pin map changes for an aggregate endpoint that is already open. A device can add or remove
// filters at any time (every loopMIDI or other teVirtualMIDI port is one), and without this an
// open endpoint keeps only the pins it found when it opened.
//
// Header only, with no KS, tracing or service dependency, so the tests can use it unchanged.
// Include windows.h, wil, WindowsMidiServices.h and midi_ksa_pin_map_property.h first.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace KsaPinMap
{
    inline constexpr uint8_t MaxGroupCount{ 16 };

    // One pin of the aggregate endpoint, and the group it carries
    struct Entry
    {
        std::wstring FilterId{ };
        uint32_t PinId{ 0 };
        MidiFlow PinDataFlow{ MidiFlowIn };     // MidiFlowIn is a destination pin, which is a MIDI output to the user
        uint8_t GroupIndex{ 0 };
    };

    // An open endpoint has one proxy per direction and group
    inline bool SameSlot(_In_ Entry const& a, _In_ Entry const& b) noexcept
    {
        return a.PinDataFlow == b.PinDataFlow && a.GroupIndex == b.GroupIndex;
    }

    // The same pin in the same slot. Filter ids are compared without regard to case.
    inline bool SameBinding(_In_ Entry const& a, _In_ Entry const& b) noexcept
    {
        return SameSlot(a, b) &&
            a.PinId == b.PinId &&
            CompareStringOrdinal(
                a.FilterId.c_str(), static_cast<int>(a.FilterId.size()),
                b.FilterId.c_str(), static_cast<int>(b.FilterId.size()),
                TRUE) == CSTR_EQUAL;
    }


    // Reads the binary pin map property. Anything malformed is rejected instead of read past.
    inline HRESULT Parse(
        _In_reads_bytes_(size) uint8_t const* data,
        _In_ size_t const size,
        _Out_ std::vector<Entry>& entries) noexcept
    {
        entries.clear();

        if (data == nullptr || size < SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        UINT32 totalByteCount{ 0 };
        memcpy(&totalByteCount, data, sizeof(totalByteCount));

        if (totalByteCount < SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER || totalByteCount > size)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        // the fixed fields, then at least one character and the terminator
        constexpr size_t filterIdOffset{ offsetof(KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY, FilterId) };
        constexpr size_t smallestEntry{ filterIdOffset + 2 * sizeof(WCHAR) };

        try
        {
            std::vector<Entry> parsed{ };
            size_t offset{ SIZET_KSAGGMIDI_PIN_MAP_PROPERTY_VALUE_HEADER };

            while (offset < totalByteCount)
            {
                size_t const remaining{ totalByteCount - offset };

                if (remaining < smallestEntry)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                // entries are packed, so they are not always aligned
                KSAGGMIDI_PIN_MAP_PROPERTY_ENTRY fixedFields{ };
                memcpy(&fixedFields, data + offset, filterIdOffset);

                if (fixedFields.ByteCount < smallestEntry || fixedFields.ByteCount > remaining)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                if (fixedFields.GroupIndex >= MaxGroupCount)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                if (fixedFields.PinDataFlow != MidiFlowIn && fixedFields.PinDataFlow != MidiFlowOut)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                size_t const characterCapacity{ (fixedFields.ByteCount - filterIdOffset) / sizeof(WCHAR) };

                std::wstring filterId(characterCapacity, L'\0');
                memcpy(filterId.data(), data + offset + filterIdOffset, characterCapacity * sizeof(WCHAR));

                auto const terminator = filterId.find(L'\0');

                if (terminator == std::wstring::npos || terminator == 0)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                filterId.resize(terminator);

                Entry entry{ };
                entry.FilterId = std::move(filterId);
                entry.PinId = fixedFields.PinId;
                entry.PinDataFlow = fixedFields.PinDataFlow;
                entry.GroupIndex = fixedFields.GroupIndex;

                parsed.push_back(std::move(entry));

                offset += fixedFields.ByteCount;
            }

            entries = std::move(parsed);

            return S_OK;
        }
        catch (std::bad_alloc const&)
        {
            return E_OUTOFMEMORY;
        }
    }


    struct Changes
    {
        std::vector<Entry> ToClose{ };      // open slots the new map no longer has, or has given to a different pin
        std::vector<Entry> ToOpen{ };       // entries of the new map which are not open yet
    };

    // current holds one entry per open slot. When desired has more than one entry for a slot, the
    // last one wins, as it does when the endpoint opens.
    inline Changes PlanChanges(_In_ std::vector<Entry> const& current, _In_ std::vector<Entry> const& desired)
    {
        std::vector<Entry> wanted{ };

        for (auto const& entry : desired)
        {
            auto const existing = std::find_if(wanted.begin(), wanted.end(), [&entry](Entry const& candidate) { return SameSlot(candidate, entry); });

            if (existing != wanted.end())
            {
                *existing = entry;
            }
            else
            {
                wanted.push_back(entry);
            }
        }

        Changes changes{ };

        for (auto const& open : current)
        {
            if (!std::any_of(wanted.begin(), wanted.end(), [&open](Entry const& candidate) { return SameBinding(candidate, open); }))
            {
                changes.ToClose.push_back(open);
            }
        }

        for (auto const& entry : wanted)
        {
            if (!std::any_of(current.begin(), current.end(), [&entry](Entry const& candidate) { return SameBinding(candidate, entry); }))
            {
                changes.ToOpen.push_back(entry);
            }
        }

        return changes;
    }


    // Hands pin maps to one open instance of an endpoint, one at a time.
    class Follower
    {
    public:
        using ApplyFunction = std::function<HRESULT(std::vector<Entry> const&)>;

        // Runs initialWork while deliveries are held back, then hands later ones to apply. A delivery
        // which arrives before this starts is dropped. Nothing is lost that way, because the map is
        // written to the endpoint before it is published, and initialWork reads it from there.
        template <typename InitialWork>
        HRESULT Start(_In_ ApplyFunction apply, _In_ InitialWork&& initialWork) noexcept
        {
            auto lock = m_lock.lock_exclusive();

            try
            {
                HRESULT const result = initialWork();

                if (FAILED(result))
                {
                    return result;
                }

                m_apply = std::move(apply);

                return S_OK;
            }
            catch (...)
            {
                return wil::ResultFromCaughtException();
            }
        }

        // Returns only once no delivery is running, and none can start.
        void Stop() noexcept
        {
            auto lock = m_lock.lock_exclusive();

            m_apply = nullptr;
        }

        // S_FALSE when nothing was applied: not started, stopped, or older than a map already applied
        HRESULT Deliver(_In_ uint64_t const version, _In_ std::vector<Entry> const& entries) noexcept
        {
            auto lock = m_lock.lock_exclusive();

            if (!m_apply || version <= m_lastDeliveredVersion)
            {
                return S_FALSE;
            }

            m_lastDeliveredVersion = version;

            try
            {
                return m_apply(entries);
            }
            catch (...)
            {
                return wil::ResultFromCaughtException();
            }
        }

    private:
        wil::srwlock m_lock{ };
        ApplyFunction m_apply{ };
        uint64_t m_lastDeliveredVersion{ 0 };
    };


    // The open instances of each aggregate endpoint. Endpoint ids must already be normalized.
    class Registry
    {
    public:
        HRESULT Add(_In_ std::wstring const& endpointDeviceId, _In_ std::shared_ptr<Follower> const& follower) noexcept
        {
            if (!follower)
            {
                return E_INVALIDARG;
            }

            try
            {
                auto lock = m_lock.lock_exclusive();

                m_followers[endpointDeviceId].push_back(follower);

                return S_OK;
            }
            catch (...)
            {
                return wil::ResultFromCaughtException();
            }
        }

        void Remove(_In_ std::wstring const& endpointDeviceId, _In_ std::shared_ptr<Follower> const& follower) noexcept
        {
            auto lock = m_lock.lock_exclusive();

            auto const endpoint = m_followers.find(endpointDeviceId);

            if (endpoint == m_followers.end())
            {
                return;
            }

            auto& followers = endpoint->second;
            followers.erase(std::remove(followers.begin(), followers.end(), follower), followers.end());

            if (followers.empty())
            {
                m_followers.erase(endpoint);
            }
        }

        // Delivers entries to every open instance of the endpoint, outside the registry lock.
        // appliedCount, when given, counts the instances which applied them.
        HRESULT Publish(
            _In_ std::wstring const& endpointDeviceId,
            _In_ std::vector<Entry> const& entries,
            _Out_opt_ size_t* appliedCount = nullptr) noexcept
        {
            if (appliedCount != nullptr)
            {
                *appliedCount = 0;
            }

            std::vector<std::shared_ptr<Follower>> followers{ };
            uint64_t version{ 0 };

            try
            {
                auto lock = m_lock.lock_exclusive();

                version = ++m_lastVersion;

                auto const endpoint = m_followers.find(endpointDeviceId);

                if (endpoint != m_followers.end())
                {
                    followers = endpoint->second;
                }
            }
            catch (...)
            {
                return wil::ResultFromCaughtException();
            }

            HRESULT result{ S_OK };

            for (auto const& follower : followers)
            {
                HRESULT const delivered = follower->Deliver(version, entries);

                if (delivered == S_OK && appliedCount != nullptr)
                {
                    (*appliedCount)++;
                }

                if (FAILED(delivered) && SUCCEEDED(result))
                {
                    result = delivered;
                }
            }

            return result;
        }

        size_t FollowerCount(_In_ std::wstring const& endpointDeviceId) noexcept
        {
            auto lock = m_lock.lock_shared();

            auto const endpoint = m_followers.find(endpointDeviceId);

            return endpoint == m_followers.end() ? 0 : endpoint->second.size();
        }

    private:
        wil::srwlock m_lock{ };
        uint64_t m_lastVersion{ 0 };
        std::map<std::wstring, std::vector<std::shared_ptr<Follower>>> m_followers{ };
    };


    // One open instance's registration. Ending it, or destroying it, returns only once no
    // delivery is running.
    class Subscription
    {
    public:
        Subscription() = default;

        ~Subscription()
        {
            End();
        }

        Subscription(Subscription const&) = delete;
        Subscription& operator=(Subscription const&) = delete;

        template <typename InitialWork>
        HRESULT Begin(
            _In_ Registry& registry,
            _In_ std::wstring const& endpointDeviceId,
            _In_ Follower::ApplyFunction apply,
            _In_ InitialWork&& initialWork) noexcept
        {
            End();

            std::shared_ptr<Follower> follower{ };

            try
            {
                follower = std::make_shared<Follower>();
                m_endpointDeviceId = endpointDeviceId;
            }
            catch (...)
            {
                return wil::ResultFromCaughtException();
            }

            // registered before the initial work, so that a change published during it is not missed
            HRESULT const added = registry.Add(endpointDeviceId, follower);

            if (FAILED(added))
            {
                return added;
            }

            m_registry = &registry;
            m_follower = follower;

            HRESULT const started = follower->Start(std::move(apply), std::forward<InitialWork>(initialWork));

            if (FAILED(started))
            {
                End();
            }

            return started;
        }

        void End() noexcept
        {
            if (m_follower)
            {
                m_follower->Stop();
                m_registry->Remove(m_endpointDeviceId, m_follower);
                m_follower.reset();
            }

            m_registry = nullptr;
        }

        bool IsActive() const noexcept
        {
            return m_follower != nullptr;
        }

    private:
        Registry* m_registry{ nullptr };
        std::wstring m_endpointDeviceId{ };
        std::shared_ptr<Follower> m_follower{ };
    };
}
