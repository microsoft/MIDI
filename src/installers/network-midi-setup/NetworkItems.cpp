// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "NetworkItems.h"

#include "PendingInvitationItem.g.cpp"
#include "RemoteHostItem.g.cpp"
#include "HostConnectionItem.g.cpp"
#include "KnownClientItem.g.cpp"
#include "LocalHostItem.g.cpp"

namespace winrt::midinetworksetup::implementation
{
    LocalHostItem::LocalHostItem()
    {
        // the "nothing here yet" placeholders are derived from the collections, and a
        // collection change does not otherwise notify anything bound to them
        m_connections.VectorChanged([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->InternalRaiseEmptyStateChanged();
                }
            });

        m_knownClients.VectorChanged([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->InternalRaiseEmptyStateChanged();
                }
            });
    }
}
