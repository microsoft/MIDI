// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ServiceFirewall.h"
#include "StringResources.h"

#include <oleauto.h>
#include <netfw.h>
#include <netlistmgr.h>

// Again, now that oleauto.h is in: that is what makes WIL define unique_bstr and unique_variant.
#include <wil/resource.h>

namespace res = ::midinetworksetup::resources;

namespace midinetworksetup::firewall
{
    static_assert(PrivateNetworks == (NET_FW_PROFILE2_DOMAIN | NET_FW_PROFILE2_PRIVATE));
    static_assert(PublicNetworks == NET_FW_PROFILE2_PUBLIC);

    namespace
    {
        constexpr wchar_t MidiServiceName[] = L"MidiSrv";
        constexpr std::wstring_view MidiServiceFileName{ L"\\midisrv.exe" };

        // The elevated copy is started with exactly this switch and one choice of networks. The
        // switch has to be the first argument, which a protocol activation can never produce:
        // its first argument is always the URI.
        constexpr std::wstring_view AllowSwitch{ L"--allow-midi-service-through-firewall" };
        constexpr std::wstring_view PrivateArgument{ L"private" };
        constexpr std::wstring_view PublicArgument{ L"public" };
        constexpr std::wstring_view BothArgument{ L"private,public" };

        // Changing one rule takes well under a second. This only stops a stuck copy from holding
        // the page forever.
        constexpr DWORD AllowRequestTimeoutMilliseconds{ 60 * 1000 };

        // Works on any thread: one with no apartment joins the MTA, and an STA stays an STA.
        class ComScope
        {
        public:
            ComScope() noexcept :
                m_result{ ::CoInitializeEx(nullptr, COINIT_MULTITHREADED) }
            {
            }

            ~ComScope() noexcept
            {
                if (SUCCEEDED(m_result))
                {
                    ::CoUninitialize();
                }
            }

            ComScope(ComScope const&) = delete;
            ComScope& operator=(ComScope const&) = delete;

        private:
            HRESULT const m_result;
        };

        HRESULT LastErrorResult() noexcept
        {
            auto const error = ::GetLastError();

            return error == ERROR_SUCCESS ? E_FAIL : HRESULT_FROM_WIN32(error);
        }

        std::wstring TextOf(_In_ wil::unique_bstr const& value)
        {
            return value ? std::wstring{ value.get(), ::SysStringLen(value.get()) } : std::wstring{};
        }

        bool EqualsNoCase(_In_ std::wstring_view const first, _In_ std::wstring_view const second) noexcept
        {
            return ::CompareStringOrdinal(
                first.data(), static_cast<int>(first.size()),
                second.data(), static_cast<int>(second.size()),
                TRUE) == CSTR_EQUAL;
        }

        bool EndsWithNoCase(_In_ std::wstring_view const value, _In_ std::wstring_view const ending) noexcept
        {
            return value.size() >= ending.size() && EqualsNoCase(value.substr(value.size() - ending.size()), ending);
        }

        // Rules and service image paths can both use environment variables like %SystemRoot%.
        std::wstring Expanded(_In_ std::wstring const& value)
        {
            auto const required = ::ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);

            if (required == 0)
            {
                return value;
            }

            std::wstring expanded(required, L'\0');

            auto const written = ::ExpandEnvironmentStringsW(value.c_str(), expanded.data(), required);

            if (written == 0 || written > required)
            {
                return value;
            }

            expanded.resize(written - 1);

            return expanded;
        }

        // A service's image path can be quoted, and can carry arguments after the executable.
        std::wstring ExecutableFromImagePath(_In_ std::wstring const& imagePath)
        {
            std::wstring path{ imagePath };

            if (!path.empty() && path.front() == L'"')
            {
                auto const closing = path.find(L'"', 1);

                path = closing == std::wstring::npos ? path.substr(1) : path.substr(1, closing - 1);
            }
            else
            {
                std::wstring lowered{ path };

                std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                if (auto const extension = lowered.find(L".exe"); extension != std::wstring::npos)
                {
                    path = path.substr(0, extension + 4);
                }
            }

            return Expanded(path);
        }

        // Asks the service control manager rather than assuming a folder: the service is in
        // System32 when it comes with Windows, and under Program Files for a preview.
        HRESULT GetServiceExecutablePath(_Out_ std::wstring& path) noexcept
        {
            path.clear();

            try
            {
                wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

                if (!manager)
                {
                    return LastErrorResult();
                }

                wil::unique_schandle service{ ::OpenServiceW(manager.get(), MidiServiceName, SERVICE_QUERY_CONFIG) };

                if (!service)
                {
                    return LastErrorResult();
                }

                DWORD size{ 0 };

                if (!::QueryServiceConfigW(service.get(), nullptr, 0, &size) &&
                    ::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                {
                    return LastErrorResult();
                }

                if (size < sizeof(QUERY_SERVICE_CONFIGW))
                {
                    return E_UNEXPECTED;
                }

                std::vector<uint8_t> buffer(size);
                auto const config = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());

                if (!::QueryServiceConfigW(service.get(), config, size, &size))
                {
                    return LastErrorResult();
                }

                if (config->lpBinaryPathName != nullptr)
                {
                    path = ExecutableFromImagePath(config->lpBinaryPathName);
                }

                return path.empty() ? E_UNEXPECTED : S_OK;
            }
            CATCH_RETURN();
        }

        template <typename TCallback>
        HRESULT ForEachRule(_In_ INetFwRules* const rules, _In_ TCallback&& callback)
        {
            wil::com_ptr_nothrow<IUnknown> unknown{};
            RETURN_IF_FAILED(rules->get__NewEnum(unknown.put()));

            auto const enumerator = unknown.try_query<IEnumVARIANT>();
            RETURN_HR_IF_NULL(E_NOINTERFACE, enumerator);

            for (;;)
            {
                wil::unique_variant item{};
                ULONG fetched{ 0 };

                if (enumerator->Next(1, item.addressof(), &fetched) != S_OK || fetched == 0)
                {
                    break;
                }

                if (item.vt != VT_DISPATCH || item.pdispVal == nullptr)
                {
                    continue;
                }

                if (auto const rule = wil::try_com_query_nothrow<INetFwRule>(item.pdispVal))
                {
                    callback(rule.get());
                }
            }

            return S_OK;
        }

        // The networks Windows lists as connected, as firewall network types. The firewall's own list
        // of active types also counts virtual switches, like Hyper-V's Default Switch.
        HRESULT GetConnectedNetworks(_Out_ long& networks) noexcept
        {
            networks = 0;

            wil::com_ptr_nothrow<INetworkListManager> networkList{};
            RETURN_IF_FAILED(::CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_ALL, IID_PPV_ARGS(networkList.put())));

            wil::com_ptr_nothrow<IEnumNetworks> connected{};
            RETURN_IF_FAILED(networkList->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, connected.put()));
            RETURN_HR_IF_NULL(E_UNEXPECTED, connected);

            for (;;)
            {
                wil::com_ptr_nothrow<INetwork> network{};
                ULONG fetched{ 0 };

                if (connected->Next(1, network.put(), &fetched) != S_OK || fetched == 0)
                {
                    break;
                }

                NLM_NETWORK_CATEGORY category{ NLM_NETWORK_CATEGORY_PUBLIC };

                if (FAILED(network->GetCategory(&category)))
                {
                    continue;
                }

                networks |=
                    category == NLM_NETWORK_CATEGORY_PRIVATE ? NET_FW_PROFILE2_PRIVATE :
                    category == NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED ? NET_FW_PROFILE2_DOMAIN :
                    NET_FW_PROFILE2_PUBLIC;
            }

            return S_OK;
        }

        // A rule reaches the service when it names the executable the service runs from, or names
        // the service itself. A rule for every program on the PC is not counted.
        bool AppliesToService(_In_ INetFwRule* const rule, _In_ std::wstring const& servicePath)
        {
            wil::unique_bstr application{};
            wil::unique_bstr service{};

            rule->get_ApplicationName(application.put());
            rule->get_ServiceName(service.put());

            auto const serviceName = TextOf(service);

            if (!serviceName.empty() && serviceName != L"*" && !EqualsNoCase(serviceName, MidiServiceName))
            {
                return false;
            }

            auto const applicationPath = TextOf(application);

            if (applicationPath.empty())
            {
                return EqualsNoCase(serviceName, MidiServiceName);
            }

            return EqualsNoCase(Expanded(applicationPath), servicePath);
        }

        // Recognized by its name, which is also what Windows Firewall shows for it. Not by its
        // path: the service can move between a preview and the copy that comes with Windows.
        bool IsOwnRule(_In_ INetFwRule* const rule, _In_ std::wstring_view const ownRuleName)
        {
            wil::unique_bstr name{};
            wil::unique_bstr application{};
            NET_FW_RULE_DIRECTION direction{ NET_FW_RULE_DIR_OUT };

            if (FAILED(rule->get_Name(name.put())) ||
                FAILED(rule->get_ApplicationName(application.put())) ||
                FAILED(rule->get_Direction(&direction)))
            {
                return false;
            }

            return direction == NET_FW_RULE_DIR_IN &&
                EqualsNoCase(TextOf(name), ownRuleName) &&
                EndsWithNoCase(Expanded(TextOf(application)), MidiServiceFileName);
        }

        // Network MIDI 2.0 and RTP-MIDI both use UDP, on ports a host can choose, so the rule
        // lets in UDP on any port and nothing else.
        HRESULT ApplyRuleSettings(_In_ INetFwRule* const rule, _In_ BSTR const application, _In_ long const networks) noexcept
        {
            RETURN_IF_FAILED(rule->put_ApplicationName(application));
            RETURN_IF_FAILED(rule->put_Protocol(NET_FW_IP_PROTOCOL_UDP));
            RETURN_IF_FAILED(rule->put_Direction(NET_FW_RULE_DIR_IN));
            RETURN_IF_FAILED(rule->put_Action(NET_FW_ACTION_ALLOW));
            RETURN_IF_FAILED(rule->put_Profiles(networks));
            RETURN_IF_FAILED(rule->put_Enabled(VARIANT_TRUE));

            return S_OK;
        }

        // Runs in the elevated copy. Updates the rule this app added before, wherever the service
        // ran from then, or adds one.
        HRESULT AllowServiceThroughFirewall(_In_ long const networks) noexcept
        {
            try
            {
                std::wstring servicePath{};

                if (auto const found = GetServiceExecutablePath(servicePath); FAILED(found))
                {
                    return found;
                }

                ComScope com{};

                wil::com_ptr_nothrow<INetFwPolicy2> policy{};
                RETURN_IF_FAILED(::CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(policy.put())));

                wil::com_ptr_nothrow<INetFwRules> rules{};
                RETURN_IF_FAILED(policy->get_Rules(rules.put()));

                auto const ownRuleName = res::GetString(L"FirewallRuleName");

                // gathered first, so no rule is changed while the collection is being walked
                std::vector<wil::com_ptr_nothrow<INetFwRule>> ownRules{};

                RETURN_IF_FAILED(ForEachRule(rules.get(), [&ownRules, &ownRuleName](INetFwRule* const rule)
                    {
                        if (IsOwnRule(rule, ownRuleName))
                        {
                            ownRules.emplace_back(rule);
                        }
                    }));

                auto const application = wil::make_bstr_nothrow(servicePath.c_str());
                RETURN_IF_NULL_ALLOC(application);

                if (!ownRules.empty())
                {
                    for (auto const& rule : ownRules)
                    {
                        RETURN_IF_FAILED(ApplyRuleSettings(rule.get(), application.get(), networks));
                    }

                    return S_OK;
                }

                auto const name = wil::make_bstr_nothrow(ownRuleName.c_str());
                auto const description = wil::make_bstr_nothrow(res::GetString(L"FirewallRuleDescription").c_str());
                RETURN_IF_NULL_ALLOC(name);
                RETURN_IF_NULL_ALLOC(description);

                wil::com_ptr_nothrow<INetFwRule> rule{};
                RETURN_IF_FAILED(::CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(rule.put())));

                // No group: Allowed apps would then list only the group's name, with no path
                RETURN_IF_FAILED(rule->put_Name(name.get()));
                RETURN_IF_FAILED(rule->put_Description(description.get()));
                RETURN_IF_FAILED(ApplyRuleSettings(rule.get(), application.get(), networks));
                RETURN_IF_FAILED(rules->Add(rule.get()));

                return S_OK;
            }
            CATCH_RETURN();
        }
    }

    FirewallState QueryState() noexcept
    {
        FirewallState state{};

        try
        {
            if (auto const found = GetServiceExecutablePath(state.ServicePath); FAILED(found))
            {
                if (found == HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST))
                {
                    state.Status = FirewallStatus::ServiceNotFound;
                }

                return state;
            }

            ComScope com{};

            wil::com_ptr_nothrow<INetFwPolicy2> policy{};

            if (FAILED(::CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(policy.put()))))
            {
                return state;
            }

            long connected{ 0 };

            // the firewall's own view is only the fallback, because it counts virtual switches too
            if (FAILED(GetConnectedNetworks(connected)) &&
                FAILED(policy->get_CurrentProfileTypes(&connected)))
            {
                return state;
            }

            state.ConnectedNetworks = connected & AllNetworks;

            NET_FW_MODIFY_STATE modifyState{ NET_FW_MODIFY_STATE_OK };

            if (SUCCEEDED(policy->get_LocalPolicyModifyState(&modifyState)))
            {
                state.ManagedByPolicy = modifyState == NET_FW_MODIFY_STATE_GP_OVERRIDE;
            }

            long enabled{ 0 };
            long blockAll{ 0 };
            long defaultAllow{ 0 };

            for (auto const profile : { NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC })
            {
                VARIANT_BOOL isEnabled{ VARIANT_TRUE };
                VARIANT_BOOL isBlockAll{ VARIANT_FALSE };
                NET_FW_ACTION defaultInbound{ NET_FW_ACTION_BLOCK };

                if (FAILED(policy->get_FirewallEnabled(profile, &isEnabled)) ||
                    FAILED(policy->get_BlockAllInboundTraffic(profile, &isBlockAll)) ||
                    FAILED(policy->get_DefaultInboundAction(profile, &defaultInbound)))
                {
                    return state;
                }

                enabled |= isEnabled != VARIANT_FALSE ? profile : 0;
                blockAll |= isBlockAll != VARIANT_FALSE ? profile : 0;
                defaultAllow |= defaultInbound == NET_FW_ACTION_ALLOW ? profile : 0;
            }

            wil::com_ptr_nothrow<INetFwRules> rules{};

            if (FAILED(policy->get_Rules(rules.put())))
            {
                return state;
            }

            long allowed{ 0 };
            long blocked{ 0 };
            long ownRule{ 0 };

            auto const ownRuleName = res::GetString(L"FirewallRuleName");

            auto const walked = ForEachRule(rules.get(), [&](INetFwRule* const rule)
                {
                    NET_FW_RULE_DIRECTION direction{ NET_FW_RULE_DIR_OUT };
                    VARIANT_BOOL ruleEnabled{ VARIANT_FALSE };
                    long protocol{ 0 };

                    if (FAILED(rule->get_Direction(&direction)) || direction != NET_FW_RULE_DIR_IN ||
                        FAILED(rule->get_Enabled(&ruleEnabled)) || ruleEnabled == VARIANT_FALSE ||
                        FAILED(rule->get_Protocol(&protocol)) ||
                        (protocol != NET_FW_IP_PROTOCOL_UDP && protocol != NET_FW_IP_PROTOCOL_ANY) ||
                        !AppliesToService(rule, state.ServicePath))
                    {
                        return;
                    }

                    NET_FW_ACTION action{ NET_FW_ACTION_BLOCK };
                    long profiles{ 0 };

                    if (FAILED(rule->get_Action(&action)) || FAILED(rule->get_Profiles(&profiles)))
                    {
                        return;
                    }

                    profiles &= AllNetworks;

                    if (action == NET_FW_ACTION_ALLOW)
                    {
                        allowed |= profiles;

                        if (IsOwnRule(rule, ownRuleName))
                        {
                            ownRule |= profiles;
                        }
                    }
                    else
                    {
                        blocked |= profiles;
                    }
                });

            if (FAILED(walked))
            {
                return state;
            }

            state.OwnRuleNetworks = ownRule;

            // Where the firewall is on, "block all incoming connections" beats every rule, a rule
            // that blocks beats one that allows, and otherwise something has to let the service in.
            auto const guarded = state.ConnectedNetworks & enabled;

            if (state.ConnectedNetworks == 0)
            {
                state.Status = FirewallStatus::NotConnected;
            }
            else if ((guarded & blockAll) != 0)
            {
                state.Status = FirewallStatus::ClosedForAll;
            }
            else if ((guarded & blocked) != 0)
            {
                state.Status = FirewallStatus::ClosedByRule;
            }
            else if ((guarded & ~(allowed | defaultAllow)) != 0)
            {
                state.Status = FirewallStatus::Closed;
            }
            else
            {
                state.Status = guarded == 0 ? FirewallStatus::Off : FirewallStatus::Open;
            }
        }
        catch (...)
        {
            state.Status = FirewallStatus::Unknown;
        }

        return state;
    }

    _Use_decl_annotations_
    HRESULT RequestAllow(HWND const owner, long const networks) noexcept
    {
        try
        {
            std::wstring_view argument{};

            switch (networks)
            {
            case PrivateNetworks:
                argument = PrivateArgument;
                break;

            case PublicNetworks:
                argument = PublicArgument;
                break;

            case AllNetworks:
                argument = BothArgument;
                break;

            default:
                return E_INVALIDARG;
            }

            wchar_t modulePath[MAX_PATH]{};

            auto const length = ::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));

            if (length == 0 || length >= ARRAYSIZE(modulePath))
            {
                return length == 0 ? LastErrorResult() : HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }

            std::wstring parameters{ AllowSwitch };
            parameters += L' ';
            parameters += argument;

            // ShellExecuteEx can hand work to shell extensions, which need COM
            ComScope com{};

            SHELLEXECUTEINFOW info{};

            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.hwnd = owner;
            info.lpVerb = L"runas";
            info.lpFile = modulePath;
            info.lpParameters = parameters.c_str();
            info.nShow = SW_HIDE;

            if (!::ShellExecuteExW(&info))
            {
                return LastErrorResult();
            }

            wil::unique_handle process{ info.hProcess };

            if (!process)
            {
                return E_UNEXPECTED;
            }

            auto const waited = ::WaitForSingleObject(process.get(), AllowRequestTimeoutMilliseconds);

            if (waited == WAIT_TIMEOUT)
            {
                return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
            }

            if (waited != WAIT_OBJECT_0)
            {
                return LastErrorResult();
            }

            DWORD exitCode{ 0 };

            if (!::GetExitCodeProcess(process.get(), &exitCode))
            {
                return LastErrorResult();
            }

            return static_cast<HRESULT>(exitCode);
        }
        CATCH_RETURN();
    }

    _Use_decl_annotations_
    bool TryRunAllowRequest(int& exitCode) noexcept
    {
        exitCode = 0;

        int count{ 0 };
        wil::unique_hlocal_ptr<PWSTR[]> arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

        if (!arguments || count < 2 || std::wstring_view{ arguments[1] } != AllowSwitch)
        {
            return false;
        }

        // Exactly what RequestAllow passes, and nothing else
        long networks{ 0 };

        if (count == 3)
        {
            std::wstring_view const choice{ arguments[2] };

            networks =
                choice == PrivateArgument ? PrivateNetworks :
                choice == PublicArgument ? PublicNetworks :
                choice == BothArgument ? AllNetworks :
                0;
        }

        auto const result = networks == 0 ? E_INVALIDARG : AllowServiceThroughFirewall(networks);

        // the event name and level are part of the event's metadata, so each needs its own write
        if (SUCCEEDED(result))
        {
            TraceLoggingWrite(
                MidiNetworkSetupTelemetryProvider::Provider(),
                MIDI_NETSETUP_TRACE_EVENT_INFO,
                TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_INFO),
                TraceLoggingWideString(L"Allowed the MIDI service through Windows Firewall.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                TraceLoggingLong(networks, "networks"));
        }
        else
        {
            TraceLoggingWrite(
                MidiNetworkSetupTelemetryProvider::Provider(),
                MIDI_NETSETUP_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_NETSETUP_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Unable to allow the MIDI service through Windows Firewall.", MIDI_NETSETUP_TRACE_MESSAGE_FIELD),
                TraceLoggingLong(networks, "networks"),
                TraceLoggingHResult(result, MIDI_NETSETUP_TRACE_HRESULT_FIELD));
        }

        exitCode = static_cast<int>(result);

        return true;
    }
}
