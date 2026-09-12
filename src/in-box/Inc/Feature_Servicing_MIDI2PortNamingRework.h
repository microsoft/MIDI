// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Covers the MIDI 1.0 port naming rework: the parent device walk-up for endpoint names, the
// device-supplied port name resolution and composition, the Automatic naming style and its new
// default, cross-endpoint name uniqueness, and keeping group terminal block names in sync with
// customized port names.
//
// Client tools also check this, because the service side rolls out gradually. A tool must not
// offer the Automatic style on a machine whose service would not honor it.

class Feature_Servicing_MIDI2PortNamingRework
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2PortNamingRework_IsEnabled()
{
    return true;
}
