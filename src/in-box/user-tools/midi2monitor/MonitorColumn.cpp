// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MonitorColumn.h"
#include "MonitorColumn.g.cpp"

namespace winrt::midi2monitor::implementation
{
    _Use_decl_annotations_
    void MonitorColumn::IsVisible(bool value)
    {
        if (m_isVisible == value)
        {
            return;
        }

        m_isVisible = value;

        try
        {
            m_propertyChanged(*this, xaml::Data::PropertyChangedEventArgs{ L"IsVisible" });
        }
        MIDI_MONITOR_CATCH_AND_LOG(L"Unable to raise the column visibility change notification.")
    }
}
