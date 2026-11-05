// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// Before anything that brings in the JSON projection after windows.h.
#include "ContentProvenance.h"

#include "DeviceLinkDiscovery.h"
#include "DeviceLinkPairing.h"

#include <midi_dnssd_browser.h>
#include <midi_dnssd_advertiser.h>
#include <midi_dnssd_announcer.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>

namespace midiapp
{
    namespace dnssd = ::WindowsMidiServicesInternal;

    namespace
    {
        constexpr wchar_t TextVersion[] = L"txtvers";
        constexpr wchar_t TextKey[] = L"key";
        constexpr wchar_t TextName[] = L"name";

        // An address or a .local name, and nothing a resolver would read as something else.
        constexpr size_t MaximumHostLength = 255;
    }

    struct DeviceLinkBrowser::State
    {
        std::mutex Lock{};
        std::unique_ptr<dnssd::MidiDnssdBrowser> Browser{};
    };

    DeviceLinkBrowser::DeviceLinkBrowser() noexcept
    {
        try
        {
            m_state = std::make_unique<State>();
        }
        catch (...)
        {
        }
    }

    DeviceLinkBrowser::~DeviceLinkBrowser()
    {
        Stop();
    }

    _Use_decl_annotations_
    HRESULT DeviceLinkBrowser::Start(std::wstring const& serviceType) noexcept
    {
        try
        {
            if (m_state == nullptr)
            {
                return E_OUTOFMEMORY;
            }

            std::scoped_lock guard{ m_state->Lock };

            if (m_state->Browser != nullptr)
            {
                return S_FALSE;
            }

            auto browser = std::make_unique<dnssd::MidiDnssdBrowser>();

            // The handlers do nothing: the list is read when it's wanted.
            auto const hr = browser->Start(serviceType,
                [](dnssd::MidiDnssdService const&) {},
                [](dnssd::MidiDnssdService const&, uint32_t) {},
                [](std::wstring const&, std::wstring const&) {});

            if (FAILED(hr))
            {
                return hr;
            }

            m_state->Browser = std::move(browser);

            return S_OK;
        }
        catch (...)
        {
            return E_OUTOFMEMORY;
        }
    }

    _Use_decl_annotations_
    std::vector<DeviceLinkFoundDevice> DeviceLinkBrowser::Found(DeviceKeyHash const* ownKey) const noexcept
    {
        try
        {
            if (m_state == nullptr)
            {
                return {};
            }

            std::vector<dnssd::MidiDnssdService> services{};

            {
                std::scoped_lock guard{ m_state->Lock };

                if (m_state->Browser == nullptr)
                {
                    return {};
                }

                services = m_state->Browser->EnumeratedServices();
            }

            std::vector<DeviceLinkFoundDevice> found{};

            for (auto const& service : services)
            {
                if (!service.IsResolved())
                {
                    continue;
                }

                DeviceLinkFoundDevice device{};
                device.Port = service.Port;
                device.Key = DeviceKeyHashFromHex(service.TextAttribute(TextKey));

                if (ownKey != nullptr && device.Key && SameDeviceKey(*device.Key, *ownKey))
                {
                    continue;
                }

                device.Name = CleanDeviceLinkName(service.TextAttribute(TextName));

                if (device.Name.empty())
                {
                    device.Name = CleanDeviceLinkName(service.ServiceInstanceName);
                }

                // An address works at once, where a .local name needs the network to answer for it.
                if (!service.IPv4Addresses.empty())
                {
                    device.Host = service.IPv4Addresses.front();
                }
                else
                {
                    device.Host = service.HostName;

                    while (!device.Host.empty() && device.Host.back() == L'.')
                    {
                        device.Host.pop_back();
                    }
                }

                if (device.Host.empty() || device.Host.size() > MaximumHostLength || device.Name.empty())
                {
                    continue;
                }

                // One entry per key, in case a PC is seen on two adapters.
                if (device.Key && std::any_of(found.begin(), found.end(), [&device](DeviceLinkFoundDevice const& other)
                    {
                        return other.Key && SameDeviceKey(*other.Key, *device.Key);
                    }))
                {
                    continue;
                }

                found.push_back(std::move(device));
            }

            std::sort(found.begin(), found.end(), [](DeviceLinkFoundDevice const& a, DeviceLinkFoundDevice const& b)
                {
                    return ::CompareStringOrdinal(a.Name.c_str(), -1, b.Name.c_str(), -1, TRUE) == CSTR_LESS_THAN;
                });

            return found;
        }
        catch (...)
        {
            return {};
        }
    }

    void DeviceLinkBrowser::Stop() noexcept
    {
        if (m_state == nullptr)
        {
            return;
        }

        std::unique_ptr<dnssd::MidiDnssdBrowser> browser{};

        try
        {
            std::scoped_lock guard{ m_state->Lock };
            browser = std::move(m_state->Browser);
        }
        catch (...)
        {
        }

        // Not under the lock: stopping waits for the DNS client.
        if (browser != nullptr)
        {
            browser->Stop();
        }
    }

    DeviceLinkAdvertiser::~DeviceLinkAdvertiser()
    {
        Stop();
    }

    _Use_decl_annotations_
    void DeviceLinkAdvertiser::Start(
        std::wstring const& label,
        std::wstring const& serviceType,
        uint16_t port,
        std::wstring const& deviceName,
        DeviceKeyHash const& key) noexcept
    {
        Stop();

        try
        {
            std::vector<std::pair<std::wstring, std::wstring>> text{
                { TextVersion, L"1" },
                { TextKey, DeviceKeyHashToHex(key) },
                { TextName, CleanDeviceLinkName(deviceName) },
            };

            // A DNS label is at most 63 bytes of UTF-8.
            auto instance = CleanDeviceLinkName(label);

            while (!instance.empty() && DeviceLinkUtf8(instance).size() > 63)
            {
                instance.pop_back();

                if (!instance.empty() && instance.back() >= 0xD800 && instance.back() <= 0xDBFF)
                {
                    instance.pop_back();
                }
            }

            if (instance.empty())
            {
                instance = L"Device";
            }

            m_worker = std::jthread([instance, serviceType, port, text](std::stop_token stopToken) noexcept
                {
                    try
                    {
                        dnssd::MidiDnssdFollowUpAnnouncer announcer{};
                        auto const announcing = SUCCEEDED(announcer.Start(serviceType));

                        dnssd::MidiDnssdAdvertiser registration{};

                        auto const hr = registration.Register(
                            instance, serviceType, std::wstring{}, port, text,
                            dnssd::MidiDnssdRegistrationTimeoutMilliseconds, stopToken);

                        std::wstring registered{};

                        if (SUCCEEDED(hr))
                        {
                            registered = registration.RegisteredLabel();

                            if (announcing && !registered.empty())
                            {
                                announcer.AddRegistration(registered);
                            }
                        }

                        // Until the receive screen closes.
                        std::mutex lock{};
                        std::condition_variable_any idle{};
                        std::unique_lock held{ lock };
                        idle.wait(held, stopToken, []() { return false; });

                        if (announcing && !registered.empty())
                        {
                            announcer.RemoveRegistration(registered);
                        }

                        // A timeout leaves the request with the DNS client, so it's withdrawn either way.
                        registration.Unregister();

                        if (announcing)
                        {
                            announcer.Stop();
                        }
                    }
                    catch (...)
                    {
                    }
                });
        }
        catch (...)
        {
        }
    }

    void DeviceLinkAdvertiser::Stop() noexcept
    {
        if (m_worker.joinable())
        {
            m_worker.request_stop();
            m_worker.join();
        }

        m_worker = std::jthread{};
    }
}
