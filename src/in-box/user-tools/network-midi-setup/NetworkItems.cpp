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
#include "RemoteClientSpeedItem.g.cpp"
#include "LocalHostItem.g.cpp"
#include "RtpRemoteHostItem.g.cpp"
#include "Midi1PortGroupItem.g.cpp"
#include "PageLayout.g.cpp"

namespace winrt::midinetworksetup::implementation
{
    // One for the process, made on first use by a row's binding or the window. Never destroyed:
    // rows can still hold handlers on it while the process ends. A projected type can't be made
    // with new, so a plain holder is.
    midinetworksetup::PageLayout PageLayout::Current()
    {
        struct Holder
        {
            midinetworksetup::PageLayout Value{ winrt::make<PageLayout>() };
        };

        static Holder const* const holder = new Holder{};
        return holder->Value;
    }

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

        m_remoteClientSpeeds.VectorChanged([weak = get_weak()](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->InternalRaiseEmptyStateChanged();
                }
            });
    }
}
