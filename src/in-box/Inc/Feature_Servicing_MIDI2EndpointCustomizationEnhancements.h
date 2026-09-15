// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Covers the endpoint customization work as one rollback unit: the shared customization update
// path and the listEndpointCustomizations verb, image file name validation, and the loopback
// transport honoring muted, image and post-creation customization.

class Feature_Servicing_MIDI2EndpointCustomizationEnhancements
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2EndpointCustomizationEnhancements_IsEnabled()
{
    return true;
}
