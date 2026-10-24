// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppFirewall.h"
#include "midi_network_adapters.h"

#include <oleauto.h>
#include <netfw.h>
#include <netlistmgr.h>
#include <lm.h>
#include <shellapi.h>

#pragma comment(lib, "netapi32.lib")

// Again, now that oleauto.h is in: that is what makes WIL define unique_bstr and unique_variant.
#include <wil/resource.h>

namespace midiapp::firewall
{
    static_assert(DomainNetworks == NET_FW_PROFILE2_DOMAIN);
    static_assert(PrivateNetworks == NET_FW_PROFILE2_PRIVATE);
    static_assert(PublicNetworks == NET_FW_PROFILE2_PUBLIC);

    namespace
    {
        struct NetworkTypeArgument
        {
            std::wstring_view Name;
            long Type;
        };

        constexpr std::array<NetworkTypeArgument, 3> NetworkTypeArguments
        { {
            { L"domain", DomainNetworks },
            { L"private", PrivateNetworks },
            { L"public", PublicNetworks },
        } };

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

        std::wstring NetworksArgument(_In_ long const networks)
        {
            std::wstring text{};

            for (auto const& argument : NetworkTypeArguments)
            {
                if ((networks & argument.Type) == 0)
                {
                    continue;
                }

                text += text.empty() ? L"" : L",";
                text += argument.Name;
            }

            return text;
        }

        // Exactly what NetworksArgument writes: known types, each once. 0 for anything else.
        long ParseNetworksArgument(_In_ std::wstring_view text) noexcept
        {
            long networks{ 0 };

            for (;;)
            {
                auto const comma = text.find(L',');
                auto const word = text.substr(0, comma);

                auto const found = std::find_if(NetworkTypeArguments.begin(), NetworkTypeArguments.end(),
                    [&](NetworkTypeArgument const& argument) { return argument.Name == word; });

                if (found == NetworkTypeArguments.end() || (networks & found->Type) != 0)
                {
                    return 0;
                }

                networks |= found->Type;

                if (comma == std::wstring_view::npos)
                {
                    return networks;
                }

                text.remove_prefix(comma + 1);
            }
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
        HRESULT GetServiceExecutablePath(_In_ std::wstring const& serviceName, _Out_ std::wstring& path) noexcept
        {
            path.clear();

            try
            {
                wil::unique_schandle manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };

                if (!manager)
                {
                    return LastErrorResult();
                }

                wil::unique_schandle service{ ::OpenServiceW(manager.get(), serviceName.c_str(), SERVICE_QUERY_CONFIG) };

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

        // The program a rule has to name: the one given, or the service's executable.
        HRESULT GetProgramPath(_In_ FirewallTarget const& target, _Out_ std::wstring& path) noexcept
        {
            path.clear();

            if (!target.ProgramPath.empty())
            {
                try
                {
                    path = target.ProgramPath;
                    return S_OK;
                }
                CATCH_RETURN();
            }

            if (target.ServiceName.empty())
            {
                return E_INVALIDARG;
            }

            return GetServiceExecutablePath(target.ServiceName, path);
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

        // The adapters a network is reached through, by the names Windows Settings shows
        std::vector<std::wstring> AdapterNamesOf(
            _In_ INetwork* const network,
            _In_ std::vector<::WindowsMidiServicesInternal::MidiNetworkAdapterInfo> const& adapters)
        {
            std::vector<std::wstring> names{};

            wil::com_ptr_nothrow<IEnumNetworkConnections> connections{};

            if (FAILED(network->GetNetworkConnections(connections.put())) || !connections)
            {
                return names;
            }

            for (;;)
            {
                wil::com_ptr_nothrow<INetworkConnection> connection{};
                ULONG fetched{ 0 };
                GUID adapterId{};

                if (connections->Next(1, connection.put(), &fetched) != S_OK || fetched == 0)
                {
                    break;
                }

                if (FAILED(connection->GetAdapterId(&adapterId)))
                {
                    continue;
                }

                auto const adapter = std::find_if(adapters.begin(), adapters.end(),
                    [&](::WindowsMidiServicesInternal::MidiNetworkAdapterInfo const& candidate) { return IsEqualGUID(candidate.Id, adapterId) != FALSE; });

                if (adapter != adapters.end() && !adapter->Name.empty() &&
                    std::find(names.begin(), names.end(), adapter->Name) == names.end())
                {
                    names.push_back(adapter->Name);
                }
            }

            return names;
        }

        // The networks Windows lists as connected. The firewall's own list of active types also
        // counts virtual switches, like Hyper-V's Default Switch.
        HRESULT GetConnectedNetworks(_Out_ std::vector<ConnectedNetwork>& networks) noexcept
        {
            networks.clear();

            try
            {
                wil::com_ptr_nothrow<INetworkListManager> networkList{};
                RETURN_IF_FAILED(::CoCreateInstance(__uuidof(NetworkListManager), nullptr, CLSCTX_ALL, IID_PPV_ARGS(networkList.put())));

                wil::com_ptr_nothrow<IEnumNetworks> connected{};
                RETURN_IF_FAILED(networkList->GetNetworks(NLM_ENUM_NETWORK_CONNECTED, connected.put()));
                RETURN_HR_IF_NULL(E_UNEXPECTED, connected);

                auto const adapters = ::WindowsMidiServicesInternal::GetMidiNetworkAdapters();

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

                    ConnectedNetwork entry{};

                    entry.Type =
                        category == NLM_NETWORK_CATEGORY_PRIVATE ? PrivateNetworks :
                        category == NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED ? DomainNetworks :
                        PublicNetworks;

                    if (wil::unique_bstr name{}; SUCCEEDED(network->GetName(name.put())))
                    {
                        entry.Name = TextOf(name);
                    }

                    entry.AdapterNames = AdapterNamesOf(network.get(), adapters);

                    networks.push_back(std::move(entry));
                }

                return S_OK;
            }
            CATCH_RETURN();
        }

        bool IsJoinedToDomain() noexcept
        {
            LPWSTR name{ nullptr };
            NETSETUP_JOIN_STATUS status{ NetSetupUnknownStatus };

            if (::NetGetJoinInformation(nullptr, &name, &status) != NERR_Success)
            {
                return false;
            }

            ::NetApiBufferFree(name);

            return status == NetSetupDomainName;
        }

        // A rule reaches the program when it names the executable the program runs from, or
        // names the service the program runs as. A rule for every program on the PC is not counted.
        bool AppliesToProgram(_In_ INetFwRule* const rule, _In_ FirewallTarget const& target, _In_ std::wstring const& programPath)
        {
            wil::unique_bstr application{};
            wil::unique_bstr service{};

            rule->get_ApplicationName(application.put());
            rule->get_ServiceName(service.put());

            auto const serviceName = TextOf(service);

            if (!serviceName.empty() && serviceName != L"*" &&
                (target.ServiceName.empty() || !EqualsNoCase(serviceName, target.ServiceName)))
            {
                return false;
            }

            auto const applicationPath = TextOf(application);

            if (applicationPath.empty())
            {
                return !target.ServiceName.empty() && EqualsNoCase(serviceName, target.ServiceName);
            }

            return EqualsNoCase(Expanded(applicationPath), programPath);
        }

        // Recognized by its name, which is also what Windows Firewall shows for it. Not by its
        // path: a program can move between a preview and the copy that comes with Windows.
        bool IsOwnRule(_In_ INetFwRule* const rule, _In_ FirewallTarget const& target)
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
                EqualsNoCase(TextOf(name), target.RuleName) &&
                EndsWithNoCase(Expanded(TextOf(application)), target.ProgramFileName);
        }

        // The protocols a target listens with. Network MIDI 2.0 and RTP-MIDI use UDP, and so does
        // OSC unless it's set to TCP.
        std::vector<long> ProtocolsOf(_In_ FirewallTarget const& target)
        {
            return target.Protocols.empty() ? std::vector<long>{ UdpProtocol } : target.Protocols;
        }

        // Ports are ones a host can choose, so each rule lets in one protocol on any port and
        // nothing else.
        HRESULT ApplyRuleSettings(_In_ INetFwRule* const rule, _In_ BSTR const application, _In_ long const networks, _In_ long const protocol, _In_ std::wstring const& remoteAddresses) noexcept
        {
            RETURN_IF_FAILED(rule->put_ApplicationName(application));
            RETURN_IF_FAILED(rule->put_Protocol(protocol));
            RETURN_IF_FAILED(rule->put_Direction(NET_FW_RULE_DIR_IN));
            RETURN_IF_FAILED(rule->put_Action(NET_FW_ACTION_ALLOW));
            RETURN_IF_FAILED(rule->put_Profiles(networks));

            if (!remoteAddresses.empty())
            {
                auto const addresses = wil::make_bstr_nothrow(remoteAddresses.c_str());
                RETURN_IF_NULL_ALLOC(addresses);
                RETURN_IF_FAILED(rule->put_RemoteAddresses(addresses.get()));
            }
            RETURN_IF_FAILED(rule->put_Enabled(VARIANT_TRUE));

            return S_OK;
        }

        // Runs in the elevated copy. Updates the rule this tool added before, wherever the program
        // ran from then, or adds one.
        HRESULT AllowThroughFirewall(_In_ FirewallTarget const& target, _In_ long const networks) noexcept
        {
            try
            {
                std::wstring programPath{};

                if (auto const found = GetProgramPath(target, programPath); FAILED(found))
                {
                    return found;
                }

                ComScope com{};

                wil::com_ptr_nothrow<INetFwPolicy2> policy{};
                RETURN_IF_FAILED(::CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(policy.put())));

                wil::com_ptr_nothrow<INetFwRules> rules{};
                RETURN_IF_FAILED(policy->get_Rules(rules.put()));

                // gathered first, so no rule is changed while the collection is being walked
                std::vector<wil::com_ptr_nothrow<INetFwRule>> ownRules{};

                RETURN_IF_FAILED(ForEachRule(rules.get(), [&ownRules, &target](INetFwRule* const rule)
                    {
                        if (IsOwnRule(rule, target))
                        {
                            ownRules.emplace_back(rule);
                        }
                    }));

                auto const application = wil::make_bstr_nothrow(programPath.c_str());
                RETURN_IF_NULL_ALLOC(application);

                auto const protocols = ProtocolsOf(target);
                std::vector<bool> covered(protocols.size(), false);

                // Each rule keeps a protocol the program listens with. Any other protocol becomes
                // the first one no rule has yet.
                for (auto const& rule : ownRules)
                {
                    long protocol{ 0 };

                    if (FAILED(rule->get_Protocol(&protocol)))
                    {
                        protocol = 0;
                    }

                    auto index = static_cast<size_t>(std::find(protocols.begin(), protocols.end(), protocol) - protocols.begin());

                    if (index == protocols.size())
                    {
                        index = static_cast<size_t>(std::find(covered.begin(), covered.end(), false) - covered.begin());

                        if (index == protocols.size())
                        {
                            index = 0;
                        }
                    }

                    RETURN_IF_FAILED(ApplyRuleSettings(rule.get(), application.get(), networks, protocols[index], target.RemoteAddresses));
                    covered[index] = true;
                }

                for (size_t index = 0; index < protocols.size(); index++)
                {
                    if (covered[index])
                    {
                        continue;
                    }

                    auto const name = wil::make_bstr_nothrow(target.RuleName.c_str());
                    auto const description = wil::make_bstr_nothrow(target.RuleDescription.c_str());
                    RETURN_IF_NULL_ALLOC(name);
                    RETURN_IF_NULL_ALLOC(description);

                    wil::com_ptr_nothrow<INetFwRule> rule{};
                    RETURN_IF_FAILED(::CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(rule.put())));

                    // No group: Allowed apps would then list only the group's name, with no path
                    RETURN_IF_FAILED(rule->put_Name(name.get()));
                    RETURN_IF_FAILED(rule->put_Description(description.get()));
                    RETURN_IF_FAILED(ApplyRuleSettings(rule.get(), application.get(), networks, protocols[index], target.RemoteAddresses));
                    RETURN_IF_FAILED(rules->Add(rule.get()));
                }

                return S_OK;
            }
            CATCH_RETURN();
        }
    }

    std::wstring CurrentProgramPath()
    {
        std::wstring path(MAX_PATH, L'\0');

        for (;;)
        {
            auto const length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

            if (length == 0)
            {
                return {};
            }

            if (length < path.size())
            {
                path.resize(length);
                return path;
            }

            if (path.size() >= 32768)
            {
                return {};
            }

            path.resize(path.size() * 2);
        }
    }

    _Use_decl_annotations_
    FirewallState QueryState(FirewallTarget const& target) noexcept
    {
        FirewallState state{};

        try
        {
            if (auto const found = GetProgramPath(target, state.ProgramPath); FAILED(found))
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
            if (SUCCEEDED(GetConnectedNetworks(state.Networks)))
            {
                for (auto const& network : state.Networks)
                {
                    connected |= network.Type;
                }
            }
            else
            {
                if (FAILED(policy->get_CurrentProfileTypes(&connected)))
                {
                    return state;
                }

                state.Networks.clear();

                for (auto const type : { DomainNetworks, PrivateNetworks, PublicNetworks })
                {
                    if ((connected & type) != 0)
                    {
                        state.Networks.push_back(ConnectedNetwork{ .Type = type });
                    }
                }
            }

            state.ConnectedNetworks = connected & AllNetworks;
            state.JoinedToDomain = IsJoinedToDomain();

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

            auto const protocols = ProtocolsOf(target);

            // What lets each protocol in, and what this tool's own rules cover, protocol by protocol
            std::vector<long> allowedFor(protocols.size(), 0);
            std::vector<long> ownRuleFor(protocols.size(), 0);
            long blocked{ 0 };

            auto const walked = ForEachRule(rules.get(), [&](INetFwRule* const rule)
                {
                    NET_FW_RULE_DIRECTION direction{ NET_FW_RULE_DIR_OUT };
                    VARIANT_BOOL ruleEnabled{ VARIANT_FALSE };
                    long protocol{ 0 };

                    if (FAILED(rule->get_Direction(&direction)) || direction != NET_FW_RULE_DIR_IN ||
                        FAILED(rule->get_Enabled(&ruleEnabled)) || ruleEnabled == VARIANT_FALSE ||
                        FAILED(rule->get_Protocol(&protocol)) ||
                        (protocol != NET_FW_IP_PROTOCOL_ANY && std::find(protocols.begin(), protocols.end(), protocol) == protocols.end()) ||
                        !AppliesToProgram(rule, target, state.ProgramPath))
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
                        auto const own = IsOwnRule(rule, target);

                        for (size_t index = 0; index < protocols.size(); index++)
                        {
                            if (protocol == NET_FW_IP_PROTOCOL_ANY || protocol == protocols[index])
                            {
                                allowedFor[index] |= profiles;

                                if (own)
                                {
                                    ownRuleFor[index] |= profiles;
                                }
                            }
                        }
                    }
                    else
                    {
                        blocked |= profiles;

                        if ((profiles & state.ConnectedNetworks) != 0)
                        {
                            if (wil::unique_bstr name{}; SUCCEEDED(rule->get_Name(name.put())))
                            {
                                auto text = TextOf(name);

                                if (!text.empty() &&
                                    std::find(state.BlockingRuleNames.begin(), state.BlockingRuleNames.end(), text) == state.BlockingRuleNames.end())
                                {
                                    state.BlockingRuleNames.push_back(std::move(text));
                                }
                            }
                        }
                    }
                });

            if (FAILED(walked))
            {
                return state;
            }

            // A network lets the program in only when it lets in every protocol it listens with.
            long allowed{ AllNetworks };
            long ownRule{ AllNetworks };

            for (size_t index = 0; index < protocols.size(); index++)
            {
                allowed &= allowedFor[index];
                ownRule &= ownRuleFor[index];
            }

            state.OwnRuleNetworks = ownRule;

            for (auto& network : state.Networks)
            {
                auto const type = network.Type;

                network.Status =
                    (enabled & type) == 0 ? FirewallStatus::Off :
                    (blockAll & type) != 0 ? FirewallStatus::ClosedForAll :
                    (blocked & type) != 0 ? FirewallStatus::ClosedByRule :
                    ((allowed | defaultAllow) & type) == 0 ? FirewallStatus::Closed :
                    FirewallStatus::Open;
            }

            // Where the firewall is on, "block all incoming connections" beats every rule, a rule
            // that blocks beats one that allows, and otherwise something has to let the program in.
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
    HRESULT RequestAllow(HWND const owner, FirewallTarget const& target, long const networks) noexcept
    {
        try
        {
            if (networks == 0 || (networks & ~AllNetworks) != 0 || target.AllowSwitch.empty())
            {
                return E_INVALIDARG;
            }

            wchar_t modulePath[MAX_PATH]{};

            auto const length = ::GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));

            if (length == 0 || length >= ARRAYSIZE(modulePath))
            {
                return length == 0 ? LastErrorResult() : HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }

            std::wstring parameters{ target.AllowSwitch };
            parameters += L' ';
            parameters += NetworksArgument(networks);

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
    bool IsAllowRequest(std::wstring_view allowSwitch) noexcept
    {
        int count{ 0 };
        wil::unique_hlocal_ptr<PWSTR[]> arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

        return arguments && count >= 2 && !allowSwitch.empty() && std::wstring_view{ arguments[1] } == allowSwitch;
    }

    _Use_decl_annotations_
    bool TryRunAllowRequest(FirewallTarget const& target, HRESULT& result, long& networks) noexcept
    {
        result = S_OK;
        networks = 0;

        int count{ 0 };
        wil::unique_hlocal_ptr<PWSTR[]> arguments{ ::CommandLineToArgvW(::GetCommandLineW(), &count) };

        // The switch has to be the first argument, which a protocol or file activation can never
        // produce: its first argument is always the URI or the file.
        if (!arguments || count < 2 || target.AllowSwitch.empty() || std::wstring_view{ arguments[1] } != target.AllowSwitch)
        {
            return false;
        }

        // Exactly what RequestAllow passes, and nothing else
        networks = count == 3 ? ParseNetworksArgument(arguments[2]) : 0;

        result = networks == 0 ? E_INVALIDARG : AllowThroughFirewall(target, networks);

        return true;
    }
}
