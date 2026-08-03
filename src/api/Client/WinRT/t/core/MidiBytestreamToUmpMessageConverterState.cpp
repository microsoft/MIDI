// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiBytestreamToUmpMessageConverterState.h"
#include "Utilities.Messages.MidiBytestreamToUmpMessageConverterState.g.cpp"

namespace winrt::Windows::Devices::Midi2::Utilities::Messages::implementation
{
    std::shared_ptr<bytestreamToUMP> MidiBytestreamToUmpMessageConverterState::InternalGetConverter() noexcept
    {
        try
        {
            if (!m_converter)
            {
                m_converter = std::make_shared<bytestreamToUMP>();
            }
        }
        catch (...)
        {
            MIDI_SDK_LOG_GENERAL_EXCEPTION(nullptr, L"General exception creating bytestreamToUMP instance.");
            m_converter = nullptr;
        }

        return m_converter;
    }

}
