// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#include <pch.h>

#include "MidiFunctionBlocksUpdatedEventArgs.h"
#include "MidiEndpointMetadataCache.h"
#include "MidiFunctionBlock.h"

namespace Windows::Devices::Midi2::Internal
{
    class InternalMidiFunctionBlockDevice
    {
    public:
        bool RequestUpdatedFunctionBlocks()
        {
            // TODO
        }

        collections::IVectorView<midi2::MidiFunctionBlock> FunctionBlocks() { return m_functionBlocks.GetView(); }

        winrt::event_token FunctionBlocksUpdated(
            _In_ foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiFunctionBlocksUpdatedEventArgs> const& handler)
        {
            return m_informationUpdatedEvent.add(handler);
        }

        void FunctionBlocksUpdated(_In_ winrt::event_token const& token) noexcept
        {
            if (m_informationUpdatedEvent) m_informationUpdatedEvent.remove(token);
        }

    private:

        winrt::event<foundation::TypedEventHandler<foundation::IInspectable, midi2::MidiFunctionBlocksUpdatedEventArgs>> m_informationUpdatedEvent;

        collections::IVector<midi2::MidiFunctionBlock> m_functionBlocks
            { winrt::single_threaded_vector<midi2::MidiFunctionBlock>() };


    };
}