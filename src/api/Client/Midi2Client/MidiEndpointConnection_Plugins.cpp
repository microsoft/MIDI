// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiEndpointConnection.h"

namespace MIDI_CPP_NAMESPACE::implementation
{

    void MidiEndpointConnection::InitializePlugins() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Initializing message processing plugins");

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Initialize(*this);
            }
            catch (...)
            {
                internal::LogGeneralError(__FUNCTION__, L"Exception initializing plugins.");
            }
        }

        internal::LogInfo(__FUNCTION__, L"Initializing complete");
    }


    void MidiEndpointConnection::CallOnConnectionOpenedOnPlugins() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Notifying message processing plugins that the connection is opened");

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.OnEndpointConnectionOpened();
            }
            catch (...)
            {
                internal::LogGeneralError(__FUNCTION__, L"Exception calling Open on plugins.");
            }
        }

        internal::LogInfo(__FUNCTION__, L"Notifying message processing plugins complete");
    }

    void MidiEndpointConnection::CleanupPlugins() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Cleaning up plugins");

        for (const auto& plugin : m_messageProcessingPlugins)
        {
            try
            {
                plugin.Cleanup();
            }
            catch (...)
            {
                internal::LogGeneralError(__FUNCTION__, L"Exception cleaning up plugins.");
            }
        }

        internal::LogInfo(__FUNCTION__, L"Cleaning up plugins - complete");
    }




    _Use_decl_annotations_
    void MidiEndpointConnection::AddMessageProcessingPlugin(midi2::IMidiEndpointMessageProcessingPlugin const& plugin)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        m_messageProcessingPlugins.Append(plugin);

        try
        {
            plugin.Initialize(*this);

            // if this is added after we've already been opened, call the
            // handler anyway to get it ready.

            if (m_isOpen)
            {
                plugin.OnEndpointConnectionOpened();
            }
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception initializing or calling OnEndpointConnectionOpened on newly-added plugin");
        }

        internal::LogInfo(__FUNCTION__, L"Complete");

    }

    _Use_decl_annotations_
    void MidiEndpointConnection::RemoveMessageProcessingPlugin(winrt::guid id)
    {
        internal::LogInfo(__FUNCTION__, L"Enter");

        for (uint32_t i = 0; i < m_messageProcessingPlugins.Size(); i++)
        {
            if (m_messageProcessingPlugins.GetAt(i).Id() == id)
            {
                m_messageProcessingPlugins.RemoveAt(i);
                break;
            }
        }

        internal::LogInfo(__FUNCTION__, L"Complete");

    }



}
