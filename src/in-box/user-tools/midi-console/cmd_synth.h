// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    struct SynthEnableOptions
    {
        bool Enabled{ true };

        // Applies to the running service without writing the configuration file, so the change is
        // gone at the next service restart.
        bool Temporary{ false };
    };

    struct SynthConfigureOptions
    {
        std::string SynthMode;
        std::string AudioMode;
        std::string BankSelectMode;
        std::string Effects;

        // Empty means leave it alone, which is why this is not a plain double.
        std::string Volume;

        bool Temporary{ false };
    };

    int RunSynthStatusCommand();
    int RunSynthSoundSetCommand();
    int RunSynthEnableCommand(_In_ SynthEnableOptions const& options);
    int RunSynthConfigureCommand(_In_ SynthConfigureOptions const& options);
}
