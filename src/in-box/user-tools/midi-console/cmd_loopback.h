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
    struct LoopbackCreateOptions
    {
        std::string NameA;
        std::string NameB;
        std::string RootName;
        std::string UniqueIdentifier;
        bool SaveToConfig{ false };
    };

    struct BasicLoopbackCreateOptions
    {
        std::string Name;
        std::string UniqueIdentifier;
        bool SaveToConfig{ false };
    };

    struct LoopbackRemoveOptions
    {
        std::string AssociationId;
        bool SaveToConfig{ false };
    };

    struct LoopbackMuteOptions
    {
        std::string AssociationId;
        bool Mute{ true };
    };

    int RunLoopbackListCommand();
    int RunLoopbackCreateCommand(_In_ LoopbackCreateOptions const& options);
    int RunLoopbackRemoveCommand(_In_ LoopbackRemoveOptions const& options);

    int RunBasicLoopbackListCommand();
    int RunBasicLoopbackCreateCommand(_In_ BasicLoopbackCreateOptions const& options);
    int RunBasicLoopbackRemoveCommand(_In_ LoopbackRemoveOptions const& options);
    int RunLoopbackMuteCommand(_In_ LoopbackMuteOptions const& options);
    int RunBasicLoopbackMuteCommand(_In_ LoopbackMuteOptions const& options);
}
