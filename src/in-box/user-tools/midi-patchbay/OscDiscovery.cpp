// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "OscDiscovery.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <midi_dnssd_browser.h>
#include <midi_dnssd_advertiser.h>
#include <midi_dnssd_announcer.h>

namespace midipatchbay
{
    namespace dnssd = ::WindowsMidiServicesInternal;

    namespace
    {
        // DNS names compare without regard to case.
        bool SameLabel(_In_ std::wstring_view first, _In_ std::wstring_view second) noexcept
        {
            return CompareStringOrdinal(first.data(), static_cast<int>(first.size()),
                second.data(), static_cast<int>(second.size()), TRUE) == CSTR_EQUAL;
        }
    }

    struct OscDiscovery::Browser
    {
        dnssd::MidiDnssdBrowser Dnssd{};
    };

    OscDiscovery& OscDiscovery::Current() noexcept
    {
        // Never destroyed, like OscHub, which calls it while it shuts down.
        static OscDiscovery* discovery = new OscDiscovery{};
        return *discovery;
    }

    std::vector<OscFoundApp> OscDiscovery::FoundApps() noexcept
    {
        try
        {
            std::shared_ptr<Browser> browser{};
            std::vector<std::wstring> own{};
            auto start = false;

            {
                std::scoped_lock guard{ m_lock };

                if (m_shutDown)
                {
                    return {};
                }

                if (m_browser == nullptr)
                {
                    m_browser = std::make_shared<Browser>();
                    start = true;
                }

                browser = m_browser;
                own = m_advertisedLabels;
            }

            if (start)
            {
                // The handlers do nothing: the list is read when it's wanted.
                auto const hr = browser->Dnssd.Start(OscUdpServiceType,
                    [](dnssd::MidiDnssdService const&) {},
                    [](dnssd::MidiDnssdService const&, uint32_t) {},
                    [](std::wstring const&, std::wstring const&) {});

                if (FAILED(hr))
                {
                    MIDI_PATCHBAY_LOG_WARNING(L"Unable to look for OSC apps on the network.");
                }
            }

            std::vector<OscFoundApp> apps{};

            for (auto const& service : browser->Dnssd.EnumeratedServices())
            {
                if (!service.IsResolved() ||
                    std::any_of(own.begin(), own.end(), [&service](std::wstring const& label) { return SameLabel(label, service.ServiceInstanceName); }))
                {
                    continue;
                }

                OscFoundApp app{};
                app.Name = service.ServiceInstanceName;
                app.Port = service.Port;

                // An address works at once, where a .local name needs the network to answer for it.
                if (!service.IPv4Addresses.empty())
                {
                    app.Host = service.IPv4Addresses.front();
                }
                else
                {
                    app.Host = service.HostName;

                    while (!app.Host.empty() && app.Host.back() == L'.')
                    {
                        app.Host.pop_back();
                    }
                }

                if (!app.Host.empty() && app.Host.size() <= MaximumOscHostLength)
                {
                    apps.push_back(std::move(app));
                }
            }

            std::sort(apps.begin(), apps.end(), [](OscFoundApp const& a, OscFoundApp const& b)
                {
                    return CompareStringOrdinal(a.Name.c_str(), -1, b.Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                });

            return apps;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    void OscDiscovery::Advertise(std::vector<OscAdvertisement> wanted) noexcept
    {
        try
        {
            {
                std::scoped_lock guard{ m_lock };

                if (m_shutDown)
                {
                    return;
                }

                auto const same = wanted.size() == m_wanted.size() &&
                    std::equal(wanted.begin(), wanted.end(), m_wanted.begin(), [](OscAdvertisement const& a, OscAdvertisement const& b)
                        {
                            return a.Port == b.Port && a.Label == b.Label;
                        });

                if (same)
                {
                    return;
                }

                m_wanted = std::move(wanted);
                m_wantedVersion++;

                if (!m_worker.joinable() && !m_wanted.empty())
                {
                    m_worker = std::jthread([this](std::stop_token stopToken) { Work(stopToken); });
                }
            }

            m_changed.notify_all();
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to advertise the OSC connections.")
    }

    void OscDiscovery::Shutdown() noexcept
    {
        std::shared_ptr<Browser> browser{};
        std::jthread worker{};

        try
        {
            std::scoped_lock guard{ m_lock };

            m_shutDown = true;
            m_wanted.clear();
            m_wantedVersion++;

            browser = std::move(m_browser);
            worker = std::move(m_worker);
        }
        catch (...)
        {
        }

        // Not under the lock: the worker withdraws each advertisement, which waits on the DNS
        // client, and reads the wanted list under the same lock.
        worker.request_stop();
        m_changed.notify_all();

        if (worker.joinable())
        {
            worker.join();
        }

        if (browser != nullptr)
        {
            browser->Dnssd.Stop();
        }
    }

    std::vector<std::wstring> OscDiscovery::AdvertisedLabels() const noexcept
    {
        try
        {
            std::scoped_lock guard{ m_lock };
            return m_advertisedLabels;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    void OscDiscovery::Work(std::stop_token stopToken) noexcept
    {
        struct Entry
        {
            OscAdvertisement Wanted{};
            std::unique_ptr<dnssd::MidiDnssdAdvertiser> Registration{};

            // The name the DNS client registered, which a collision may have changed.
            std::wstring Registered{};
        };

        std::vector<Entry> current{};

        // Repeats each new registration's announcement correctly, as the transports do. A failure
        // costs only the repeats.
        dnssd::MidiDnssdFollowUpAnnouncer announcer{};
        auto announcing = false;

        auto const withdraw = [&announcer](Entry& entry) noexcept
            {
                if (!entry.Registered.empty())
                {
                    announcer.RemoveRegistration(entry.Registered);
                }

                if (entry.Registration != nullptr)
                {
                    entry.Registration->Unregister();
                }
            };

        try
        {
            uint64_t seen{ 0 };

            while (!stopToken.stop_requested())
            {
                std::vector<OscAdvertisement> wanted{};

                {
                    std::unique_lock lock{ m_lock };

                    if (!m_changed.wait(lock, stopToken, [&]() { return m_wantedVersion != seen; }))
                    {
                        break;
                    }

                    seen = m_wantedVersion;
                    wanted = m_wanted;
                }

                auto const isWanted = [&wanted](OscAdvertisement const& advertisement)
                    {
                        return std::any_of(wanted.begin(), wanted.end(), [&advertisement](OscAdvertisement const& w)
                            {
                                return w.Port == advertisement.Port && w.Label == advertisement.Label;
                            });
                    };

                for (auto it = current.begin(); it != current.end();)
                {
                    if (isWanted(it->Wanted))
                    {
                        ++it;
                        continue;
                    }

                    withdraw(*it);
                    it = current.erase(it);
                }

                for (auto const& advertisement : wanted)
                {
                    if (stopToken.stop_requested())
                    {
                        break;
                    }

                    if (std::any_of(current.begin(), current.end(), [&advertisement](Entry const& e)
                        {
                            return e.Wanted.Port == advertisement.Port && e.Wanted.Label == advertisement.Label;
                        }))
                    {
                        continue;
                    }

                    if (!announcing)
                    {
                        announcing = SUCCEEDED(announcer.Start(OscUdpServiceType));
                    }

                    Entry entry{};
                    entry.Wanted = advertisement;
                    entry.Registration = std::make_unique<dnssd::MidiDnssdAdvertiser>();

                    // DNS-SD requires a TXT record. OSC apps don't read one, so it only says which
                    // version of these records it is.
                    auto const hr = entry.Registration->Register(
                        advertisement.Label,
                        OscUdpServiceType,
                        std::wstring{},
                        advertisement.Port,
                        { { L"txtvers", L"1" } },
                        dnssd::MidiDnssdRegistrationTimeoutMilliseconds,
                        stopToken);

                    if (SUCCEEDED(hr))
                    {
                        entry.Registered = entry.Registration->RegisteredLabel();

                        if (announcing && !entry.Registered.empty())
                        {
                            announcer.AddRegistration(entry.Registered);
                        }
                    }
                    else if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
                    {
                        // A timeout leaves the request with the DNS client, which may finish it,
                        // so it's kept to be withdrawn later either way.
                        MIDI_PATCHBAY_LOG_WARNING(L"Unable to advertise an OSC connection on the network.");
                    }

                    current.push_back(std::move(entry));
                }

                std::vector<std::wstring> labels{};

                for (auto const& entry : current)
                {
                    labels.push_back(entry.Registered.empty() ? entry.Wanted.Label : entry.Registered);
                }

                std::scoped_lock guard{ m_lock };
                m_advertisedLabels = std::move(labels);
            }
        }
        catch (...)
        {
            MIDI_PATCHBAY_LOG_WARNING(L"Advertising the OSC connections stopped.");
        }

        for (auto& entry : current)
        {
            withdraw(entry);
        }

        announcer.Stop();

        try
        {
            std::scoped_lock guard{ m_lock };
            m_advertisedLabels.clear();
        }
        catch (...)
        {
        }
    }
}
