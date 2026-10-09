// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiDnssdAdvertiserTests.h"

#include <windns.h>

#include <stop_token>

#include "midi_dnssd_advertiser.h"

using namespace WindowsMidiServicesInternal;
using namespace WEX::Common;
using namespace WEX::Logging;

namespace
{
    constexpr wchar_t ServiceType[] = L"_wmsprobe._udp.local";

    // a curly apostrophe and two German letters, the kind of name a customer gives a host
    constexpr wchar_t NonAsciiName[] = L"Gr\u00FC\u00DFe \u2019 Studio";

    // Unique per run, so a registration left behind by an earlier run cannot collide
    std::wstring UniqueLabel(_In_ wchar_t const* purpose)
    {
        return std::wstring{ L"WMS " } + purpose + L" " + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64() % 1000000);
    }

    struct ResolveResult
    {
        bool Found{ false };
        std::wstring HostName;
        uint16_t Port{ 0 };
        std::vector<std::pair<std::wstring, std::wstring>> Text;
    };

    // Shared with the callback, which can still come after the caller stopped waiting for it
    struct ResolveState
    {
        ResolveState() : Done{ CreateEventW(nullptr, TRUE, FALSE, nullptr) } { }
        ~ResolveState() { if (Done != nullptr) CloseHandle(Done); }

        ResolveState(_In_ ResolveState const&) = delete;
        ResolveState& operator=(_In_ ResolveState const&) = delete;

        HANDLE Done{ nullptr };
        std::mutex Lock;
        ResolveResult Result;
    };

    VOID WINAPI OnResolved(_In_ DWORD status, _In_ PVOID context, _In_opt_ PDNS_SERVICE_INSTANCE instance) noexcept
    {
        // the reference the request held
        std::unique_ptr<std::shared_ptr<ResolveState>> const reference{ static_cast<std::shared_ptr<ResolveState>*>(context) };

        try
        {
            auto const& state = *reference;

            {
                auto lock = std::scoped_lock{ state->Lock };

                if (status == ERROR_SUCCESS && instance != nullptr)
                {
                    state->Result.Found = true;
                    state->Result.HostName = instance->pszHostName != nullptr ? instance->pszHostName : L"";
                    state->Result.Port = instance->wPort;

                    for (DWORD i = 0; i < instance->dwPropertyCount; i++)
                    {
                        state->Result.Text.emplace_back(
                            instance->keys[i] != nullptr ? instance->keys[i] : L"",
                            instance->values[i] != nullptr ? instance->values[i] : L"");
                    }
                }
            }

            SetEvent(state->Done);
        }
        catch (...)
        {
        }

        if (instance != nullptr) DnsServiceFreeInstance(instance);
    }

    // What DnsServiceResolve finds for a full instance name within the wait
    ResolveResult Resolve(_In_ std::wstring const& fullName, _In_ DWORD const waitMilliseconds)
    {
        auto const state = std::make_shared<ResolveState>();

        DNS_SERVICE_RESOLVE_REQUEST request{};
        request.Version = DNS_QUERY_REQUEST_VERSION1;
        request.QueryName = const_cast<PWSTR>(fullName.c_str());
        request.pResolveCompletionCallback = &OnResolved;
        request.pQueryContext = new std::shared_ptr<ResolveState>(state);

        DNS_SERVICE_CANCEL cancel{};

        if (DnsServiceResolve(&request, &cancel) != DNS_REQUEST_PENDING)
        {
            delete static_cast<std::shared_ptr<ResolveState>*>(request.pQueryContext);
            return ResolveResult{ };
        }

        if (WaitForSingleObject(state->Done, waitMilliseconds) != WAIT_OBJECT_0)
        {
            // A canceled lookup may still call back, and its reference keeps the state alive if it does
            DnsServiceResolveCancel(&cancel);
            WaitForSingleObject(state->Done, 2000);
        }

        auto lock = std::scoped_lock{ state->Lock };
        return state->Result;
    }

    std::wstring TextValue(_In_ ResolveResult const& result, _In_ std::wstring const& key)
    {
        for (auto const& [name, value] : result.Text)
        {
            if (name == key) return value;
        }

        return L"(missing)";
    }

    bool IsLocalHostName(_In_ std::wstring const& hostName)
    {
        std::wstring const suffix{ L".local" };

        return hostName.size() > suffix.size() &&
            _wcsicmp(hostName.c_str() + hostName.size() - suffix.size(), suffix.c_str()) == 0;
    }

    // The DNS client confirms a withdrawal a moment before it acts on it (measured: still found
    // at once, gone 63 ms later), so this looks a few times
    bool IsGoneSoon(_In_ std::wstring const& fullName)
    {
        for (int attempt = 0; attempt < 5; attempt++)
        {
            Sleep(100);
            if (!Resolve(fullName, 500).Found) return true;
        }

        return false;
    }
}


void MidiDnssdAdvertiserTests::TestRegistersThePortAndText()
{
    auto const label = UniqueLabel(L"Advertiser Text");

    MidiDnssdAdvertiser advertiser;

    VERIFY_ARE_EQUAL(advertiser.Register(
        label,
        ServiceType,
        L"",
        5047,
        { { L"UMPEndpointName", NonAsciiName }, { L"ProductInstanceId", L"TEST-1249" } },
        MidiDnssdRegistrationTimeoutMilliseconds,
        std::stop_token{ }), S_OK);

    VERIFY_IS_TRUE(advertiser.RegisteredLabel() == label, L"registered under the label asked for");
    VERIFY_IS_FALSE(advertiser.WasRenamed());

    auto const found = Resolve(label + L"." + ServiceType, 5000);

    Log::Comment(String().Format(L"Resolved [%s] on host [%s], port %u", label.c_str(), found.HostName.c_str(), static_cast<unsigned int>(found.Port)));

    VERIFY_IS_TRUE(found.Found, L"the DNS client knows the registration");
    VERIFY_ARE_EQUAL(static_cast<unsigned int>(found.Port), 5047u);
    VERIFY_IS_TRUE(IsLocalHostName(found.HostName), L"an empty host name is this PC's name in .local");
    VERIFY_IS_TRUE(TextValue(found, L"UMPEndpointName") == NonAsciiName, L"a name outside ASCII comes back exactly");
    VERIFY_IS_TRUE(TextValue(found, L"ProductInstanceId") == L"TEST-1249");

    advertiser.Unregister();
}

void MidiDnssdAdvertiserTests::TestWithdrawingRemovesTheRegistration()
{
    auto const label = UniqueLabel(L"Advertiser Withdraw");
    auto const fullName = label + L"." + ServiceType;

    MidiDnssdAdvertiser advertiser;

    VERIFY_ARE_EQUAL(advertiser.Register(label, ServiceType, L"", 5047, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);
    VERIFY_IS_TRUE(Resolve(fullName, 5000).Found, L"registered");

    advertiser.Unregister();

    VERIFY_IS_TRUE(IsGoneSoon(fullName), L"gone once withdrawn");

    // nothing left for a second withdrawal, or the destructor's, to do
    advertiser.Unregister();
}

void MidiDnssdAdvertiserTests::TestReportsTheLabelTheDnsClientChose()
{
    auto const label = UniqueLabel(L"Advertiser Collision");

    MidiDnssdAdvertiser first;
    MidiDnssdAdvertiser second;

    VERIFY_ARE_EQUAL(first.Register(label, ServiceType, L"", 5047, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);

    // The same name again, which the DNS client gives another label rather than refusing
    VERIFY_ARE_EQUAL(second.Register(label, ServiceType, L"", 5048, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);

    auto const chosen = second.RegisteredLabel();

    Log::Comment(String().Format(L"Asked for [%s], the DNS client chose [%s]", label.c_str(), chosen.c_str()));

    VERIFY_IS_FALSE(first.WasRenamed(), L"the first registration keeps its label");
    VERIFY_IS_TRUE(second.WasRenamed(), L"the second is reported as renamed");
    VERIFY_IS_FALSE(chosen.empty());
    VERIFY_IS_TRUE(chosen != label);

    auto const found = Resolve(chosen + L"." + ServiceType, 5000);

    VERIFY_IS_TRUE(found.Found, L"the label reported is the one registered");
    VERIFY_ARE_EQUAL(static_cast<unsigned int>(found.Port), 5048u);

    second.Unregister();
    first.Unregister();
}

void MidiDnssdAdvertiserTests::TestWithdrawsOnlyItsOwnRegistration()
{
    auto const label = UniqueLabel(L"Advertiser Renamed Withdraw");
    auto const fullName = label + L"." + ServiceType;

    MidiDnssdAdvertiser first;
    MidiDnssdAdvertiser second;

    VERIFY_ARE_EQUAL(first.Register(label, ServiceType, L"", 5047, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);
    VERIFY_ARE_EQUAL(second.Register(label, ServiceType, L"", 5048, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);
    VERIFY_IS_TRUE(second.WasRenamed());

    auto const renamedFullName = second.RegisteredLabel() + L"." + ServiceType;

    // The DNS client withdraws by name. Withdrawing by the name asked for took down the first
    // registration instead, and left the renamed one on the network until Windows restarted.
    second.Unregister();

    VERIFY_IS_TRUE(IsGoneSoon(renamedFullName), L"the renamed registration is gone");

    auto const stillThere = Resolve(fullName, 5000);

    VERIFY_IS_TRUE(stillThere.Found, L"the first registration is still there");
    VERIFY_ARE_EQUAL(static_cast<unsigned int>(stillThere.Port), 5047u);

    first.Unregister();

    VERIFY_IS_TRUE(IsGoneSoon(fullName), L"and goes when it is withdrawn");
}

void MidiDnssdAdvertiserTests::TestCancelingLeavesAnotherRegistrationAlone()
{
    auto const label = UniqueLabel(L"Advertiser Cancel");
    auto const fullName = label + L"." + ServiceType;

    MidiDnssdAdvertiser first;
    MidiDnssdAdvertiser second;

    VERIFY_ARE_EQUAL(first.Register(label, ServiceType, L"", 5047, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);

    // the same name, given up on while the DNS client is still checking it
    std::stop_source stop;
    stop.request_stop();

    VERIFY_ARE_EQUAL(second.Register(label, ServiceType, L"", 5048, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, stop.get_token()), HRESULT_FROM_WIN32(ERROR_CANCELLED));

    second.Unregister();

    // a withdrawal acts within about 63 ms, so a wrong one would show by now
    Sleep(500);

    auto const stillThere = Resolve(fullName, 5000);

    VERIFY_IS_TRUE(stillThere.Found, L"the first registration is still there");
    VERIFY_ARE_EQUAL(static_cast<unsigned int>(stillThere.Port), 5047u);

    first.Unregister();

    VERIFY_IS_TRUE(IsGoneSoon(fullName), L"and goes when it is withdrawn");
}

void MidiDnssdAdvertiserTests::TestRefusesWhatCannotBeRegistered()
{
    auto const label = UniqueLabel(L"Advertiser Refusal");

    MidiDnssdAdvertiser advertiser;

    // RFC 6763 section 6.1: a TXT record may not be empty
    VERIFY_ARE_EQUAL(advertiser.Register(label, ServiceType, L"", 5047, { }, 1000, std::stop_token{ }), E_INVALIDARG);
    VERIFY_ARE_EQUAL(advertiser.Register(L"", ServiceType, L"", 5047, { { L"txtvers", L"1" } }, 1000, std::stop_token{ }), E_INVALIDARG);
    VERIFY_ARE_EQUAL(advertiser.Register(label, L"", L"", 5047, { { L"txtvers", L"1" } }, 1000, std::stop_token{ }), E_INVALIDARG);

    VERIFY_ARE_EQUAL(advertiser.Register(label, ServiceType, L"", 5047, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, std::stop_token{ }), S_OK);

    // one registration per advertiser
    VERIFY_ARE_EQUAL(advertiser.Register(label, ServiceType, L"", 5048, { { L"txtvers", L"1" } }, 1000, std::stop_token{ }), E_ILLEGAL_STATE_CHANGE);

    advertiser.Unregister();
}

void MidiDnssdAdvertiserTests::TestGivesUpOnceStopped()
{
    std::stop_source stop;
    stop.request_stop();

    MidiDnssdAdvertiser advertiser;

    auto const started = GetTickCount64();
    auto const result = advertiser.Register(UniqueLabel(L"Advertiser Stop"), ServiceType, L"", 5047, { { L"txtvers", L"1" } }, MidiDnssdRegistrationTimeoutMilliseconds, stop.get_token());
    auto const elapsed = static_cast<unsigned int>(GetTickCount64() - started);

    Log::Comment(String().Format(L"Returned 0x%08X after %u ms", static_cast<unsigned int>(result), elapsed));

    VERIFY_ARE_EQUAL(result, HRESULT_FROM_WIN32(ERROR_CANCELLED));
    VERIFY_IS_LESS_THAN(elapsed, 500u, L"without waiting for the DNS client to finish probing");

    // still with the DNS client, which the cancel ends
    advertiser.Unregister();
}
