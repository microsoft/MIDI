// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// DNS-SD registration through the Windows DNS client (DnsServiceRegister), shared by the Network
// MIDI 2.0 and rtpMIDI transports.
//
// A registration lasts until it is withdrawn here or the process ends, and the DNS client sends
// the goodbye either way. Unlike the WinRT registration, which lasts exactly as long as one
// socket, it can be withdrawn and made again while the host keeps its socket and its sessions.
//
// Every new registration is announced with the cache-flush bit set on the shared PTR record, which
// briefly removes other devices' instances of the service type from caches on the network. So a
// host registers once for its life, and the endpoint managers repeat the announcement correctly
// afterward. See midi_dnssd_announcer.h.
// ============================================================================

#pragma once

#include <windows.h>
#include <windns.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "Dnsapi.lib")

namespace WindowsMidiServicesInternal
{
    // Probing a name takes most of a second. A DNS client still busy after this is stuck.
    constexpr DWORD MidiDnssdRegistrationTimeoutMilliseconds = 10000;

    // how long withdrawing a registration waits for the DNS client to confirm it
    constexpr DWORD MidiDnssdDeregistrationWaitMilliseconds = 2000;

    // One DNS-SD registration. Register and Unregister are called by one owner at a time.
    // RegisteredLabel and WasRenamed may be called from any thread.
    class MidiDnssdAdvertiser
    {
    public:
        MidiDnssdAdvertiser() = default;
        ~MidiDnssdAdvertiser() { Unregister(); }

        MidiDnssdAdvertiser(_In_ MidiDnssdAdvertiser const&) = delete;
        MidiDnssdAdvertiser& operator=(_In_ MidiDnssdAdvertiser const&) = delete;

        // Registers "instanceLabel.serviceType" and waits for the DNS client to finish probing the
        // name, giving up early when stopToken is signaled. An empty host name is this PC's DNS
        // host name in .local. A non-zero interface index advertises on that adapter only.
        // HRESULT_FROM_WIN32(ERROR_TIMEOUT) leaves the request with the DNS client, which may still
        // complete it. Unregister withdraws it either way.
        HRESULT Register(
            _In_ std::wstring const& instanceLabel,
            _In_ std::wstring const& serviceType,
            _In_ std::wstring const& hostName,
            _In_ uint16_t const port,
            _In_ std::vector<std::pair<std::wstring, std::wstring>> const& textAttributes,
            _In_ DWORD const timeoutMilliseconds,
            _In_ std::stop_token const& stopToken,
            _In_ uint32_t const interfaceIndex = 0) noexcept
        {
            try
            {
                if (m_registration != nullptr) return E_ILLEGAL_STATE_CHANGE;

                // Without a pair the TXT record goes out empty, which RFC 6763 section 6.1 does not allow
                if (instanceLabel.empty() || serviceType.empty() || textAttributes.empty()) return E_INVALIDARG;

                auto const requestedFullName = instanceLabel + L"." + serviceType;

                auto host = hostName;

                if (host.empty())
                {
                    wchar_t computerName[256]{};
                    DWORD size = ARRAYSIZE(computerName);

                    if (!GetComputerNameExW(ComputerNameDnsHostname, computerName, &size)) return HRESULT_FROM_WIN32(GetLastError());

                    host = std::wstring{ computerName } + L".local";
                }

                // copied by the DNS client when it builds the instance
                std::vector<PCWSTR> keys{};
                std::vector<PCWSTR> values{};

                for (auto const& attribute : textAttributes)
                {
                    keys.push_back(attribute.first.c_str());
                    values.push_back(attribute.second.c_str());
                }

                auto const registration = std::make_shared<Registration>();

                registration->Instance = DnsServiceConstructInstance(
                    requestedFullName.c_str(), host.c_str(), nullptr, nullptr, port, 0, 0,
                    static_cast<DWORD>(keys.size()), keys.data(), values.data());

                if (registration->Instance == nullptr)
                {
                    auto const error = GetLastError();
                    return error != ERROR_SUCCESS ? HRESULT_FROM_WIN32(error) : E_OUTOFMEMORY;
                }

                registration->Request.Version = DNS_QUERY_REQUEST_VERSION1;
                registration->Request.InterfaceIndex = interfaceIndex;
                registration->Request.pServiceInstance = registration->Instance;
                registration->Request.pRegisterCompletionCallback = &MidiDnssdAdvertiser::Completed;
                registration->Request.pQueryContext = registration.get();
                registration->Request.unicastEnabled = FALSE;

                // owed before the call, because the callback can come before the call returns
                registration->OweCallback(registration);

                auto const status = DnsServiceRegister(&registration->Request, &registration->Cancel);

                if (status != DNS_REQUEST_PENDING)
                {
                    // nothing started, so no callback is coming
                    registration->ForgiveCallback();
                    return status == ERROR_SUCCESS ? E_UNEXPECTED : HRESULT_FROM_WIN32(status);
                }

                {
                    auto namesLock = std::scoped_lock{ m_namesLock };
                    m_serviceType = serviceType;
                    m_requestedFullName = requestedFullName;
                    m_registeredFullName.clear();
                }

                m_registration = registration;

                auto lock = std::unique_lock{ registration->Lock };

                if (!registration->Changed.wait_for(lock, stopToken, std::chrono::milliseconds(timeoutMilliseconds),
                    [&]() { return registration->Completions > 0; }))
                {
                    return stopToken.stop_requested() ? HRESULT_FROM_WIN32(ERROR_CANCELLED) : HRESULT_FROM_WIN32(ERROR_TIMEOUT);
                }

                if (registration->Status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(registration->Status);

                auto registeredFullName = registration->RegisteredFullName;
                lock.unlock();

                {
                    auto namesLock = std::scoped_lock{ m_namesLock };
                    m_registeredFullName = std::move(registeredFullName);
                }

                return S_OK;
            }
            catch (std::bad_alloc const&)
            {
                return E_OUTOFMEMORY;
            }
            catch (...)
            {
                return E_FAIL;
            }
        }

        // Sends the goodbye and waits a short while for the DNS client to confirm it. A callback
        // still to come after that finds its registration alive, because the registration keeps
        // itself alive until every callback it is owed has come.
        //
        // The DNS client withdraws whichever registration has the name it is given, whoever made
        // it. So a renamed registration is withdrawn by its new name, and one the DNS client never
        // confirmed is not withdrawn at all, because the name asked for may belong to another.
        void Unregister() noexcept
        {
            auto const registration = std::exchange(m_registration, nullptr);
            if (registration == nullptr) return;

            try
            {
                uint32_t completions{ 0 };
                DWORD status{ ERROR_SUCCESS };
                std::wstring registeredFullName{};

                {
                    auto lock = std::scoped_lock{ registration->Lock };
                    completions = registration->Completions;
                }

                // A registration still checking its name is canceled. The DNS client answers at
                // once and registers nothing, unless the registration finished first. Then the
                // cancel does nothing, and the registration is withdrawn below.
                if (completions == 0) DnsServiceRegisterCancel(&registration->Cancel);

                {
                    auto lock = std::unique_lock{ registration->Lock };

                    registration->Changed.wait_for(lock, std::chrono::milliseconds(MidiDnssdDeregistrationWaitMilliseconds),
                        [&]() { return registration->Completions > 0; });

                    completions = registration->Completions;
                    status = registration->Status;
                    registeredFullName = registration->RegisteredFullName;
                }

                // Nothing was registered, or the DNS client has not said under what name. A
                // registration it confirms later still goes when the process ends.
                if (completions == 0 || status != ERROR_SUCCESS) return;

                auto const requested = registration->Instance;

                // the request still carries the name asked for
                if (!registeredFullName.empty() &&
                    (requested->pszInstanceName == nullptr || _wcsicmp(registeredFullName.c_str(), requested->pszInstanceName) != 0))
                {
                    registration->RegisteredInstance = DnsServiceConstructInstance(
                        registeredFullName.c_str(), requested->pszHostName, requested->ip4Address, requested->ip6Address,
                        requested->wPort, requested->wPriority, requested->wWeight, requested->dwPropertyCount,
                        const_cast<PCWSTR*>(requested->keys), const_cast<PCWSTR*>(requested->values));

                    // left for the end of the process, rather than withdrawing a name that may be another's
                    if (registration->RegisteredInstance == nullptr) return;

                    registration->Request.pServiceInstance = registration->RegisteredInstance;
                }

                registration->OweCallback(registration);

                // the goodbye for the records is sent by the DNS client service, not by this process
                if (DnsServiceDeRegister(&registration->Request, nullptr) == DNS_REQUEST_PENDING)
                {
                    auto lock = std::unique_lock{ registration->Lock };
                    registration->Changed.wait_for(lock, std::chrono::milliseconds(MidiDnssdDeregistrationWaitMilliseconds),
                        [&]() { return registration->Completions > completions; });
                }
                else
                {
                    registration->ForgiveCallback();
                }
            }
            catch (...)
            {
                // leaving now is safe for the same reason
            }
        }

        // The label actually on the network, or the requested one until the DNS client says. The
        // responder renames a colliding label rather than refusing it. Empty if it cannot be copied.
        std::wstring RegisteredLabel() const noexcept
        {
            try
            {
                auto lock = std::scoped_lock{ m_namesLock };

                std::wstring const suffix = L"." + m_serviceType;
                auto name = m_registeredFullName.empty() ? m_requestedFullName : m_registeredFullName;

                if (name.size() > suffix.size() && _wcsicmp(name.c_str() + name.size() - suffix.size(), suffix.c_str()) == 0)
                {
                    name.resize(name.size() - suffix.size());
                }

                return name;
            }
            catch (...)
            {
                return {};
            }
        }

        bool WasRenamed() const noexcept
        {
            try
            {
                auto lock = std::scoped_lock{ m_namesLock };
                return !m_registeredFullName.empty() && _wcsicmp(m_registeredFullName.c_str(), m_requestedFullName.c_str()) != 0;
            }
            catch (...)
            {
                return false;
            }
        }

    private:
        // Everything a DNS client callback touches. A canceled request may or may not still get
        // its callback, and nothing can wait for that, so the registration holds a reference to
        // itself while any callback is owed to it.
        struct Registration
        {
            Registration() = default;

            ~Registration()
            {
                if (Instance != nullptr) DnsServiceFreeInstance(Instance);
                if (RegisteredInstance != nullptr) DnsServiceFreeInstance(RegisteredInstance);
            }

            Registration(_In_ Registration const&) = delete;
            Registration& operator=(_In_ Registration const&) = delete;

            void OweCallback(_In_ std::shared_ptr<Registration> const& self)
            {
                auto lock = std::scoped_lock{ Lock };

                CallbacksOwed++;
                KeepAlive = self;
            }

            // for a request which did not start, so its callback is not coming
            void ForgiveCallback()
            {
                std::shared_ptr<Registration> release{ nullptr };

                auto lock = std::scoped_lock{ Lock };
                if (CallbacksOwed > 0 && --CallbacksOwed == 0) release = std::move(KeepAlive);
            }

            PDNS_SERVICE_INSTANCE Instance{ nullptr };

            // the same records under the name the DNS client chose, for withdrawing a renamed registration
            PDNS_SERVICE_INSTANCE RegisteredInstance{ nullptr };

            DNS_SERVICE_REGISTER_REQUEST Request{};
            DNS_SERVICE_CANCEL Cancel{};

            std::mutex Lock;
            std::condition_variable_any Changed;
            uint32_t Completions{ 0 };
            uint32_t CallbacksOwed{ 0 };
            DWORD Status{ ERROR_SUCCESS };
            std::wstring RegisteredFullName;
            std::shared_ptr<Registration> KeepAlive{ nullptr };
        };

        // Runs on a DNS client thread, where an exception would end the process
        static VOID WINAPI Completed(_In_ DWORD status, _In_ PVOID context, _In_opt_ PDNS_SERVICE_INSTANCE instance) noexcept
        {
            auto const registration = static_cast<Registration*>(context);

            if (registration != nullptr)
            {
                try
                {
                    // the last callback owed lets the registration go, once this one is done with it
                    std::shared_ptr<Registration> release{ nullptr };

                    {
                        auto lock = std::scoped_lock{ registration->Lock };

                        registration->Status = status;
                        registration->Completions++;

                        if (registration->CallbacksOwed > 0 && --registration->CallbacksOwed == 0) release = std::move(registration->KeepAlive);

                        if (instance != nullptr && instance->pszInstanceName != nullptr)
                        {
                            try
                            {
                                registration->RegisteredFullName = instance->pszInstanceName;
                            }
                            catch (...)
                            {
                                // the label is only reported, so the requested one stands in
                            }
                        }
                    }

                    registration->Changed.notify_all();
                }
                catch (...)
                {
                }
            }

            if (instance != nullptr) DnsServiceFreeInstance(instance);
        }

        std::shared_ptr<Registration> m_registration{ nullptr };

        mutable std::mutex m_namesLock;
        std::wstring m_serviceType;
        std::wstring m_requestedFullName;
        std::wstring m_registeredFullName;
    };
}
