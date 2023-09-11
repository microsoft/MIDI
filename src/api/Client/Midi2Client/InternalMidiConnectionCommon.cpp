// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================


#include <pch.h>

#include "InternalMidiConnectionCommon.h"

namespace Windows::Devices::Midi2::Internal
{

    void InternalMidiConnectionCommon::InitializePlugins()
    {

    }

    void InternalMidiConnectionCommon::CallOnConnectionOpenedOnPlugins()
    {

    }

    void InternalMidiConnectionCommon::CleanupPlugins()
    {

    }


    _Success_(return == true)
        bool InternalMidiConnectionCommon::ActivateMidiStream(
            _In_ com_ptr<IMidiAbstraction> serviceAbstraction,
            _In_ const IID& iid,
            _Out_ void** iface)
    {
        try
        {
            winrt::check_hresult(serviceAbstraction->Activate(iid, iface));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception activating stream. Service may be unavailable", ex);

            return false;
        }
    }
}
