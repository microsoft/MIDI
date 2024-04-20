// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================



#include "pch.h"
#include "MidiSessionSettings.h"
#include "MidiSessionSettings.g.cpp"

namespace MIDI_CPP_NAMESPACE::implementation
{

    _Use_decl_annotations_
    void MidiSessionSettings::UseMmcssThreads(bool const value)
    {
        m_useMmcss = value;
    }

}
