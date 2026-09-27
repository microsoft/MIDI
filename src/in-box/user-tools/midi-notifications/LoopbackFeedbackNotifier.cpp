// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

namespace
{
    namespace midi2loop = ::winrt::Windows::Devices::Midi2::Transports::Loopback;
    namespace midi2bloop = ::winrt::Windows::Devices::Midi2::Transports::BasicLoopback;

    std::wstring LoadAppString(_In_ UINT const id) noexcept
    {
        wchar_t buffer[512]{ };

        auto const length = ::LoadStringW(::GetModuleHandleW(nullptr), id, buffer, ARRAYSIZE(buffer));

        return length > 0 ? std::wstring{ buffer, static_cast<size_t>(length) } : std::wstring{ };
    }

    std::wstring FormatAppString(_In_ UINT const id, _In_ std::wstring const& argument) noexcept
    {
        auto const format = LoadAppString(id);

        if (format.empty())
        {
            return argument;
        }

        wchar_t buffer[1024]{ };

        // The format strings are ours, from the resource file. The argument has already been
        // sanitized, and swprintf_s truncates rather than overflowing.
        if (::swprintf_s(buffer, ARRAYSIZE(buffer), format.c_str(), argument.c_str()) < 0)
        {
            return argument;
        }

        return std::wstring{ buffer };
    }

    std::wstring TripIdentity(_In_ winrt::guid const& associationId, _In_ winrt::Windows::Foundation::DateTime const& detectedTime) noexcept
    {
        try
        {
            return std::wstring{ winrt::to_hstring(associationId) } + L"@" +
                std::to_wstring(detectedTime.time_since_epoch().count());
        }
        catch (...)
        {
            return { };
        }
    }

    // The tools installer registers the scheme. A PC with an older one has nothing to open, and
    // a button that only raised "find an app to open this link" would be worse than none.
    bool IsLoopbackSetupProtocolRegistered() noexcept
    {
        wil::unique_hkey key{ };

        return ::RegOpenKeyExW(
            HKEY_CLASSES_ROOT,
            MIDI_LOOPBACK_SETUP_PROTOCOL_SCHEME,
            0,
            KEY_QUERY_VALUE,
            key.put()) == ERROR_SUCCESS;
    }
}

void LoopbackFeedbackNotifier::Evaluate() noexcept
{
    if (!AppSettings::NotificationsEnabled() || !AppSettings::LoopbackFeedbackNotificationsEnabled())
    {
        // Turned off while a banner was up. Withdrawing it is part of honoring the setting.
        m_toast.Remove(ToastTag);
        m_reportedIdentities.clear();

        return;
    }

    auto const muted = ReadMutedLoopbacks();

    if (muted.empty())
    {
        // Unmuted or removed since. A banner saying otherwise would be wrong by the time it was read.
        m_toast.Remove(ToastTag);
        m_reportedIdentities.clear();

        return;
    }

    Notify(muted);
}

_Use_decl_annotations_
void LoopbackFeedbackNotifier::Notify(std::vector<MutedLoopback> const& muted) noexcept
{
    bool anythingNew{ false };

    for (auto const& loopback : muted)
    {
        if (!m_reportedIdentities.contains(loopback.Identity))
        {
            anythingNew = true;
            break;
        }
    }

    if (!anythingNew)
    {
        return;
    }

    auto const now = std::chrono::steady_clock::now();

    if (m_lastToast.time_since_epoch().count() != 0 && now - m_lastToast < MinimumIntervalBetweenToasts)
    {
        // The banner which is already up says a loopback was muted and where to go. The app
        // shows every one of them.
        return;
    }

    std::wstring body{ };

    if (muted.size() == 1)
    {
        body = FormatAppString(IDS_NOTIFICATION_LOOPBACK_FEEDBACK_ONE, muted.front().DisplayName);
    }
    else
    {
        body = FormatAppString(IDS_NOTIFICATION_LOOPBACK_FEEDBACK_MANY, std::to_wstring(muted.size()));
    }

    auto const result = m_toast.ShowOrReplace(
        ToastTag,
        LoadAppString(IDS_NOTIFICATION_TITLE),
        body,
        LoadAppString(IDS_NOTIFICATION_LOOPBACK_ATTRIBUTION),
        LoadAppString(IDS_NOTIFICATION_OPEN_LOOPBACK_SETUP_BUTTON),
        IsLoopbackSetupProtocolRegistered() ? MIDI_LOOPBACK_SETUP_PROTOCOL_URI_FEEDBACK : L"");

    if (FAILED(result))
    {
        LOG_IF_FAILED(result);
        return;
    }

    m_lastToast = now;

    m_reportedIdentities.clear();

    for (auto const& loopback : muted)
    {
        m_reportedIdentities.insert(loopback.Identity);
    }
}

std::vector<LoopbackFeedbackNotifier::MutedLoopback> LoopbackFeedbackNotifier::ReadMutedLoopbacks() const noexcept
{
    std::vector<MutedLoopback> result{ };

    // Each kind is asked separately, so a service with only one of them still reports that one.
    try
    {
        if (midi2loop::MidiLoopbackManager::IsTransportAvailable() &&
            midi2loop::MidiLoopbackManager::IsFeedbackProtectionAvailable())
        {
            for (auto const& entry : midi2loop::MidiLoopbackManager::GetActiveLoopbackEntries())
            {
                if (entry == nullptr || !entry.IsMutedForFeedback())
                {
                    continue;
                }

                MutedLoopback loopback{ };
                loopback.Identity = TripIdentity(entry.AssociationId(), entry.FeedbackDetectedTime());

                // A pair is named after its A side, which is the name the customer typed first.
                auto const name = entry.EndpointA() == nullptr ? winrt::hstring{ } : entry.EndpointA().Name();
                loopback.DisplayName = ToastSender::SanitizeForToast(std::wstring{ name });

                result.push_back(std::move(loopback));
            }
        }
    }
    catch (...)
    {
        // The service may have stopped between the hint and this call, which is ordinary.
        LOG_CAUGHT_EXCEPTION();
    }

    try
    {
        if (midi2bloop::MidiBasicLoopbackManager::IsTransportAvailable() &&
            midi2bloop::MidiBasicLoopbackManager::IsFeedbackProtectionAvailable())
        {
            for (auto const& entry : midi2bloop::MidiBasicLoopbackManager::GetActiveLoopbackEntries())
            {
                if (entry == nullptr || !entry.IsMutedForFeedback())
                {
                    continue;
                }

                MutedLoopback loopback{ };
                loopback.Identity = TripIdentity(entry.AssociationId(), entry.FeedbackDetectedTime());
                loopback.DisplayName = ToastSender::SanitizeForToast(std::wstring{ entry.Name() });

                result.push_back(std::move(loopback));
            }
        }
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }

    return result;
}
