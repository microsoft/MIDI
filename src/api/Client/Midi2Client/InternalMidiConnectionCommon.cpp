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


    _Use_decl_annotations_
    bool InternalMidiConnectionCommon::ActivateMidiStream(
        winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        const IID& iid,
        void** iface) noexcept
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
