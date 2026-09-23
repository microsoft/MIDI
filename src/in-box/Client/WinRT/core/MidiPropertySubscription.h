// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "CapabilityInquiry.MidiPropertySubscription.g.h"

namespace winrt::Windows::Devices::Midi2::CapabilityInquiry::implementation
{
    struct MidiPropertySubscription : MidiPropertySubscriptionT<MidiPropertySubscription>
    {
        MidiPropertySubscription() = default;

        ci::MidiCapabilityInquiryStatus Status() const noexcept { return m_status; }
        int32_t ResourceStatus() const noexcept { return m_resourceStatus; }

        ci::MidiUniqueId ResponderMuid() const noexcept { return m_responderMuid; }

        winrt::hstring Resource() const noexcept { return m_resource; }
        winrt::hstring ResourceId() const noexcept { return m_resourceId; }
        winrt::hstring SubscribeId() const noexcept { return m_subscribeId; }

        bool IsActive() const noexcept { return m_isActive; }

        // Not projected. The session owns every field here.
        void InternalSetStatus(_In_ ci::MidiCapabilityInquiryStatus const value) noexcept { m_status = value; }
        void InternalSetResourceStatus(_In_ int32_t const value) noexcept { m_resourceStatus = value; }
        void InternalSetResponderMuid(_In_ ci::MidiUniqueId const& value) noexcept { m_responderMuid = value; }
        void InternalSetResource(_In_ winrt::hstring const& value) noexcept { m_resource = value; }
        void InternalSetResourceId(_In_ winrt::hstring const& value) noexcept { m_resourceId = value; }
        void InternalSetSubscribeId(_In_ winrt::hstring const& value) noexcept { m_subscribeId = value; }
        void InternalSetIsActive(_In_ bool const value) noexcept { m_isActive = value; }

    private:
        ci::MidiCapabilityInquiryStatus m_status{ ci::MidiCapabilityInquiryStatus::NoResponse };
        int32_t m_resourceStatus{ 0 };

        ci::MidiUniqueId m_responderMuid{ nullptr };

        winrt::hstring m_resource{};
        winrt::hstring m_resourceId{};
        winrt::hstring m_subscribeId{};

        std::atomic<bool> m_isActive{ false };
    };
}
