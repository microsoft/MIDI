// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SynthItem.h"
#if __has_include("SynthItem.g.cpp")
#include "SynthItem.g.cpp"
#endif

namespace winrt::midisoundfontsynth::implementation
{
    _Use_decl_annotations_
    void SynthItem::Raise(wchar_t const* propertyName)
    {
        m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ propertyName });
    }

    _Use_decl_annotations_
    void SynthItem::SetFile(winrt::hstring const& path, winrt::hstring const& fileName)
    {
        Set(m_filePath, path, L"FilePath");
        Set(m_fileName, fileName, L"FileName");
    }

    _Use_decl_annotations_
    void SynthItem::SetDetailText(winrt::hstring const& value)
    {
        Set(m_detailText, value, L"DetailText");
        Set(m_detailVisibility, value.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible, L"DetailVisibility");
    }

    _Use_decl_annotations_
    void SynthItem::SetVolume(double decibels, winrt::hstring const& text)
    {
        Set(m_volumeDb, decibels, L"VolumeDb");
        Set(m_volumeText, text, L"VolumeText");
    }

    _Use_decl_annotations_
    void SynthItem::SetWarning(bool value)
    {
        Set(m_warningVisibility, value ? xaml::Visibility::Visible : xaml::Visibility::Collapsed, L"WarningVisibility");
    }

    _Use_decl_annotations_
    void SynthItem::SetInUse(bool value)
    {
        Set(m_inUseVisibility, value ? xaml::Visibility::Visible : xaml::Visibility::Collapsed, L"InUseVisibility");
    }

    _Use_decl_annotations_
    void SynthItem::SetAccessibleNames(
        winrt::hstring const& card,
        winrt::hstring const& toggle,
        winrt::hstring const& volume,
        winrt::hstring const& more)
    {
        Set(m_accessibleName, card, L"AccessibleName");
        Set(m_toggleAccessibleName, toggle, L"ToggleAccessibleName");
        Set(m_volumeAccessibleName, volume, L"VolumeAccessibleName");
        Set(m_moreAccessibleName, more, L"MoreAccessibleName");
    }
}
