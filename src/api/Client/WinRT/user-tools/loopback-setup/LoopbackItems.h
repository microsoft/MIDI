// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "LoopbackItem.g.h"

// The pages refresh from the service on a timer, so the row raises property changed rather
// than being replaced. Nothing here throws: a failing notification must never take down a
// UI callback.
#define MIDI_LOOPSETUP_OBSERVABLE_ITEM()                                                       \
    public:                                                                                    \
        winrt::event_token PropertyChanged(                                                    \
            winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)      \
        {                                                                                      \
            return m_propertyChanged.add(handler);                                             \
        }                                                                                      \
                                                                                               \
        void PropertyChanged(winrt::event_token const& token) noexcept                         \
        {                                                                                      \
            m_propertyChanged.remove(token);                                                   \
        }                                                                                      \
                                                                                               \
    private:                                                                                   \
        void RaisePropertyChanged(std::wstring_view const name) noexcept                       \
        {                                                                                      \
            try                                                                                \
            {                                                                                  \
                m_propertyChanged(                                                             \
                    *this,                                                                     \
                    winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{ name });       \
            }                                                                                  \
            catch (...)                                                                        \
            {                                                                                  \
            }                                                                                  \
        }                                                                                      \
                                                                                               \
        template <typename TValue>                                                             \
        bool UpdateField(TValue& field, TValue const& value, std::wstring_view const name) noexcept \
        {                                                                                      \
            if (field == value)                                                                \
            {                                                                                  \
                return false;                                                                  \
            }                                                                                  \
                                                                                               \
            field = value;                                                                     \
            RaisePropertyChanged(name);                                                        \
                                                                                               \
            return true;                                                                       \
        }                                                                                      \
                                                                                               \
        winrt::event<winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged{};

namespace midiloopbacksetup
{
    // Everything a row shows, gathered in one place so a refresh is a single call rather than
    // a dozen setters which each raise a change notification.
    struct LoopbackRowData
    {
        winrt::hstring AssociationId{};

        winrt::hstring DisplayName{};

        winrt::hstring NameA{};
        winrt::hstring DescriptionA{};
        winrt::hstring EndpointDeviceIdA{};

        winrt::hstring NameB{};
        winrt::hstring DescriptionB{};
        winrt::hstring EndpointDeviceIdB{};

        // bare file name in the shared endpoint assets folder
        winrt::hstring ImageFileName{};

        // localized, because the row does not reach into the resource loader itself
        winrt::hstring MuteButtonLabel{};
        winrt::hstring PersistenceText{};

        // Every row carries a mute and a delete button, so each needs a name which says which
        // loopback it acts on. "Delete" on its own is the same name eleven times over.
        winrt::hstring MuteButtonAccessibleName{};
        winrt::hstring DeleteButtonAccessibleName{};

        bool HasSecondEndpoint{ false };
        bool IsMuted{ false };
        bool IsPersisted{ false };
    };
}

namespace winrt::midiloopbacksetup::implementation
{
    struct LoopbackItem : LoopbackItemT<LoopbackItem>
    {
        LoopbackItem() = default;

        winrt::hstring AssociationId() const noexcept { return m_associationId; }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }

        winrt::hstring NameA() const noexcept { return m_nameA; }
        winrt::hstring DescriptionA() const noexcept { return m_descriptionA; }
        winrt::hstring EndpointDeviceIdA() const noexcept { return m_endpointDeviceIdA; }

        winrt::hstring NameB() const noexcept { return m_nameB; }
        winrt::hstring DescriptionB() const noexcept { return m_descriptionB; }
        winrt::hstring EndpointDeviceIdB() const noexcept { return m_endpointDeviceIdB; }

        bool HasSecondEndpoint() const noexcept { return m_hasSecondEndpoint; }

        xaml::Visibility SecondEndpointVisibility() const noexcept
        {
            return m_hasSecondEndpoint ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility DescriptionAVisibility() const noexcept
        {
            return m_descriptionA.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        xaml::Visibility DescriptionBVisibility() const noexcept
        {
            return (m_hasSecondEndpoint && !m_descriptionB.empty()) ?
                xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        winrt::hstring ImageFileName() const noexcept { return m_imageFileName; }

        xaml::Media::ImageSource ImageSource() const noexcept { return m_imageSource; }

        xaml::Visibility ImageVisibility() const noexcept
        {
            return m_imageSource != nullptr ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        xaml::Visibility GlyphVisibility() const noexcept
        {
            return m_imageSource != nullptr ? xaml::Visibility::Collapsed : xaml::Visibility::Visible;
        }

        bool IsMuted() const noexcept { return m_isMuted; }

        winrt::hstring MuteButtonLabel() const noexcept { return m_muteButtonLabel; }

        winrt::hstring MuteButtonAccessibleName() const noexcept { return m_muteButtonAccessibleName; }
        winrt::hstring DeleteButtonAccessibleName() const noexcept { return m_deleteButtonAccessibleName; }

        // Speaker vs. muted speaker. The glyph carries the state at a glance; the label says
        // what the button will do.
        winrt::hstring MuteButtonGlyph() const noexcept
        {
            return m_isMuted ? winrt::hstring{ L"\xE74F" } : winrt::hstring{ L"\xE767" };
        }

        xaml::Visibility MutedBadgeVisibility() const noexcept
        {
            return m_isMuted ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        bool CanMute() const noexcept { return m_canMute; }
        void CanMute(bool const value) noexcept
        {
            if (UpdateField(m_canMute, value, L"CanMute"))
            {
                RaisePropertyChanged(L"MuteVisibility");
            }
        }

        xaml::Visibility MuteVisibility() const noexcept
        {
            return m_canMute ? xaml::Visibility::Visible : xaml::Visibility::Collapsed;
        }

        bool IsPersisted() const noexcept { return m_isPersisted; }
        void IsPersisted(bool const value) noexcept { UpdateField(m_isPersisted, value, L"IsPersisted"); }

        winrt::hstring PersistenceText() const noexcept { return m_persistenceText; }

        bool IsBusy() const noexcept { return m_isBusy; }
        void IsBusy(bool const value) noexcept { UpdateField(m_isBusy, value, L"IsBusy"); }

        int32_t DisplayOrder() const noexcept { return m_displayOrder; }
        void DisplayOrder(int32_t const value) noexcept { UpdateField(m_displayOrder, value, L"DisplayOrder"); }

        void InternalInitialize(_In_ winrt::hstring const& associationId) noexcept
        {
            m_associationId = associationId;
        }

        void InternalUpdate(_In_ ::midiloopbacksetup::LoopbackRowData const& data) noexcept
        {
            UpdateField(m_displayName, data.DisplayName, L"DisplayName");

            if (UpdateField(m_nameA, data.NameA, L"NameA") ||
                UpdateField(m_descriptionA, data.DescriptionA, L"DescriptionA"))
            {
                RaisePropertyChanged(L"DescriptionAVisibility");
            }

            UpdateField(m_endpointDeviceIdA, data.EndpointDeviceIdA, L"EndpointDeviceIdA");

            if (UpdateField(m_nameB, data.NameB, L"NameB") ||
                UpdateField(m_descriptionB, data.DescriptionB, L"DescriptionB"))
            {
                RaisePropertyChanged(L"DescriptionBVisibility");
            }

            UpdateField(m_endpointDeviceIdB, data.EndpointDeviceIdB, L"EndpointDeviceIdB");

            if (UpdateField(m_imageFileName, data.ImageFileName, L"ImageFileName"))
            {
                RefreshImageSource();
            }

            if (UpdateField(m_hasSecondEndpoint, data.HasSecondEndpoint, L"HasSecondEndpoint"))
            {
                RaisePropertyChanged(L"SecondEndpointVisibility");
                RaisePropertyChanged(L"DescriptionBVisibility");
            }

            if (UpdateField(m_isMuted, data.IsMuted, L"IsMuted"))
            {
                RaisePropertyChanged(L"MuteButtonGlyph");
                RaisePropertyChanged(L"MutedBadgeVisibility");
            }

            UpdateField(m_muteButtonLabel, data.MuteButtonLabel, L"MuteButtonLabel");
            UpdateField(m_muteButtonAccessibleName, data.MuteButtonAccessibleName, L"MuteButtonAccessibleName");
            UpdateField(m_deleteButtonAccessibleName, data.DeleteButtonAccessibleName, L"DeleteButtonAccessibleName");
            UpdateField(m_persistenceText, data.PersistenceText, L"PersistenceText");
            UpdateField(m_isPersisted, data.IsPersisted, L"IsPersisted");
        }

    private:
        // The picture is loaded once per change rather than per redraw, and a name which no
        // longer resolves to a file simply leaves the glyph in place.
        void RefreshImageSource() noexcept
        {
            m_imageSource = nullptr;

            if (!m_imageFileName.empty())
            {
                auto const path = midiapp::EndpointImageAssets::FullPathForFileName(
                    std::wstring{ m_imageFileName });

                if (!path.empty() && midiapp::EndpointImageAssets::Exists(std::wstring{ m_imageFileName }))
                {
                    try
                    {
                        foundation::Uri const uri{ winrt::hstring{ path } };

                        if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ m_imageFileName }))
                        {
                            m_imageSource = xaml::Media::Imaging::SvgImageSource{ uri };
                        }
                        else
                        {
                            m_imageSource = xaml::Media::Imaging::BitmapImage{ uri };
                        }
                    }
                    catch (...)
                    {
                        m_imageSource = nullptr;
                    }
                }
            }

            RaisePropertyChanged(L"ImageSource");
            RaisePropertyChanged(L"ImageVisibility");
            RaisePropertyChanged(L"GlyphVisibility");
        }

        winrt::hstring m_associationId{};
        winrt::hstring m_displayName{};

        winrt::hstring m_nameA{};
        winrt::hstring m_descriptionA{};
        winrt::hstring m_endpointDeviceIdA{};

        winrt::hstring m_nameB{};
        winrt::hstring m_descriptionB{};
        winrt::hstring m_endpointDeviceIdB{};

        winrt::hstring m_muteButtonLabel{};
        winrt::hstring m_muteButtonAccessibleName{};
        winrt::hstring m_deleteButtonAccessibleName{};
        winrt::hstring m_persistenceText{};

        winrt::hstring m_imageFileName{};
        xaml::Media::ImageSource m_imageSource{ nullptr };

        bool m_hasSecondEndpoint{ false };
        bool m_isMuted{ false };
        bool m_canMute{ false };
        bool m_isPersisted{ false };
        bool m_isBusy{ false };

        int32_t m_displayOrder{ 0 };

        MIDI_LOOPSETUP_OBSERVABLE_ITEM()
    };
}

namespace winrt::midiloopbacksetup::factory_implementation
{
    struct LoopbackItem : LoopbackItemT<LoopbackItem, implementation::LoopbackItem>
    {
    };
}
