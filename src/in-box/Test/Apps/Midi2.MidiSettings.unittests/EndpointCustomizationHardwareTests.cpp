// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "Feature_Servicing_MIDI2EndpointCustomizationEnhancements.h"

#include "EndpointSettings.h"
#include "EndpointCustomizationHardwareTests.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace custom = ::midiapp::customizations;
namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;
namespace midi2config = ::winrt::Windows::Devices::Midi2::ServiceConfig;

namespace
{
    constexpr wchar_t EndpointDeviceIdParameter[] = L"EndpointDeviceId";

    // easy to spot if a failed run leaves them behind
    constexpr wchar_t FirstInputName[] = L"Hardware test input 1";
    constexpr wchar_t SecondInputName[] = L"Hardware test input 2";
    constexpr wchar_t ThirdInputName[] = L"Hardware test input 3";
    constexpr wchar_t TestEndpointName[] = L"Hardware test endpoint";

    constexpr int64_t TestLatencyTicks{ 50000 };

    constexpr std::chrono::milliseconds ServiceWait{ 30000 };
    constexpr std::chrono::milliseconds EndpointGoneWait{ 5000 };
    constexpr std::chrono::milliseconds EndpointWait{ 60000 };
    constexpr std::chrono::milliseconds PropertyWait{ 10000 };
    constexpr std::chrono::milliseconds PollInterval{ 250 };

    using ServiceHandle = std::unique_ptr<std::remove_pointer_t<SC_HANDLE>, decltype(&::CloseServiceHandle)>;

    struct TestEndpoint
    {
        winrt::hstring EndpointDeviceId{};
        winrt::hstring DeviceInstanceId{};
        winrt::guid TransportId{};

        // kept across saves, as the app keeps it
        custom::ClearedPortNames ClearedPortNames{};
    };

    bool WaitFor(_In_ std::function<bool()> const& condition, _In_ std::chrono::milliseconds const limit)
    {
        auto const deadline = std::chrono::steady_clock::now() + limit;

        while (!condition())
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return false;
            }

            std::this_thread::sleep_for(PollInterval);
        }

        return true;
    }

    std::wstring RuntimeParameter(_In_ PCWSTR name)
    {
        String value;

        if (SUCCEEDED(RuntimeParameters::TryGetValue(name, value)) && !value.IsEmpty())
        {
            return std::wstring{ static_cast<PCWSTR>(value) };
        }

        return {};
    }

    bool IsElevated()
    {
        HANDLE token{ nullptr };

        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            return false;
        }

        TOKEN_ELEVATION elevation{};
        DWORD size{ 0 };

        auto const queried = ::GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
        ::CloseHandle(token);

        return queried && elevation.TokenIsElevated != 0;
    }

    bool IsPresent(_In_ winrt::hstring const& endpointDeviceId)
    {
        try
        {
            for (auto const& endpoint : midi2enum::MidiEndpointDeviceInformation::FindAll())
            {
                if (::_wcsicmp(endpoint.EndpointDeviceId().c_str(), endpointDeviceId.c_str()) == 0)
                {
                    return true;
                }
            }
        }
        catch (...)
        {
            // while the service restarts, read as not there yet
        }

        return false;
    }

    midi2enum::MidiEndpointDeviceInformation Endpoint(_In_ TestEndpoint const& endpoint)
    {
        return midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpoint.EndpointDeviceId);
    }

    // what the endpoint shows, without the app's record of the names it cleared
    custom::EndpointSettings Shown(_In_ TestEndpoint const& endpoint)
    {
        custom::ClearedPortNames none{};

        return custom::ReadEndpointSettings(Endpoint(endpoint), none);
    }

    // what the service holds for the endpoint, or nullptr
    midi2config::MidiServiceEndpointCustomization StoredCustomization(_In_ TestEndpoint const& endpoint)
    {
        for (auto const& customization :
            midi2config::MidiServiceTransportPluginConfigManager::GetEndpointCustomizations(endpoint.TransportId))
        {
            if (::_wcsicmp(customization.ResolvedEndpointDeviceId().c_str(), endpoint.EndpointDeviceId.c_str()) == 0)
            {
                return customization;
            }
        }

        return nullptr;
    }

    bool HasCustomization(_In_ TestEndpoint const& endpoint)
    {
        if (StoredCustomization(endpoint) != nullptr)
        {
            return true;
        }

        auto const shown = Shown(endpoint);

        return custom::HasNonDisplayContent(shown) ||
            !shown.Name.empty() || !shown.Description.empty() || !shown.ImageFileName.empty();
    }

    uint32_t CountInputs(_In_ midi2enum::MidiEndpointDeviceInformation const& endpoint)
    {
        uint32_t inputs{ 0 };

        for (auto const& entry : endpoint.GetNameTable())
        {
            if (entry.Group() != nullptr && entry.Flow() == midi2enum::Midi1PortFlow::MidiMessageSource)
            {
                inputs++;
            }
        }

        return inputs;
    }

    // Skips the test, and returns false, where it cannot run
    bool TryPrepare(_Inout_ TestEndpoint& endpoint, _In_ bool const restartsService)
    {
        auto const endpointDeviceId = RuntimeParameter(EndpointDeviceIdParameter);

        if (endpointDeviceId.empty())
        {
            Log::Result(TestResults::Skipped, String().Format(
                L"Needs a device. Run with /p:%s=<id>, using an id from 'midi enum endpoints --show-endpoint-id'.",
                EndpointDeviceIdParameter));

            return false;
        }

        if (restartsService && !IsElevated())
        {
            Log::Result(TestResults::Skipped, L"Needs an elevated prompt, to restart the MIDI service.");
            return false;
        }

        auto const info = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(endpointDeviceId);

        VERIFY_IS_TRUE(info != nullptr && IsPresent(info.EndpointDeviceId()), L"the endpoint is connected");
        VERIFY_IS_TRUE(custom::HasEditablePortNames(info), L"the endpoint is a KS or KSA endpoint, whose ports MIDI Settings names");

        endpoint.EndpointDeviceId = info.EndpointDeviceId();
        endpoint.DeviceInstanceId = info.DeviceInstanceId();
        endpoint.TransportId = info.GetTransportSuppliedInfo().TransportId();

        if (CountInputs(info) < 3)
        {
            Log::Result(TestResults::Skipped, L"Needs an endpoint with at least three MIDI 1.0 inputs.");
            return false;
        }

        if (HasCustomization(endpoint))
        {
            Log::Result(TestResults::Skipped,
                L"The endpoint already has a customization, which these tests would remove. Pick another device, or remove it in MIDI Settings first.");

            return false;
        }

        return true;
    }

    void SendAndSave(_In_ midi2config::MidiServiceEndpointCustomizationConfig const& config)
    {
        auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

        VERIFY_IS_TRUE(response != nullptr && response.Status() == midi2config::MidiServiceConfigResponseStatus::Success,
            L"the service applies the change");

        auto const saved = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

        VERIFY_IS_TRUE(saved != nullptr && saved.Success(), L"the change is saved");
    }

    // Deletes the endpoint's entry, as the customize dialog does when every box is emptied
    bool Remove(_In_ TestEndpoint const& endpoint) noexcept
    {
        try
        {
            auto const stored = StoredCustomization(endpoint);

            if (stored == nullptr)
            {
                return true;
            }

            midi2config::MidiServiceEndpointCustomizationRemovalConfig removal{ endpoint.TransportId, stored.MatchCriteria() };

            auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(removal);
            auto const saved = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(removal);

            return response != nullptr && response.Status() == midi2config::MidiServiceConfigResponseStatus::Success &&
                saved != nullptr && saved.Success();
        }
        catch (...)
        {
            return false;
        }
    }

    // Removes what the test saved, also when a check fails partway
    class RemoveWhenDone
    {
    public:
        explicit RemoveWhenDone(_In_ TestEndpoint const& endpoint) noexcept : m_endpoint{ endpoint } {}

        ~RemoveWhenDone()
        {
            if (!Remove(m_endpoint))
            {
                Log::Warning(L"Unable to remove the test customization. Remove it in MIDI Settings.");
            }
        }

        RemoveWhenDone(RemoveWhenDone const&) = delete;
        RemoveWhenDone& operator=(RemoveWhenDone const&) = delete;

    private:
        TestEndpoint const& m_endpoint;
    };

    midi2config::MidiServiceEndpointCustomizationConfig NewConfig(_In_ TestEndpoint const& endpoint)
    {
        midi2config::MidiServiceEndpointCustomizationConfig config{ endpoint.TransportId };

        config.MatchCriteria().EndpointDeviceId(endpoint.EndpointDeviceId);
        config.MatchCriteria().DeviceInstanceId(endpoint.DeviceInstanceId);

        return config;
    }

    // What a latency measurement and the naming style setting leave behind
    void SaveLatencyAndNamingStyle(_In_ TestEndpoint const& endpoint)
    {
        auto const config = NewConfig(endpoint);

        config.OutgoingLatencyTicks(TestLatencyTicks);
        config.UseCustomOutgoingLatency(true);
        config.Midi1PortNamingApproach(midi2enum::Midi1PortNamingApproach::UseNewStyle);

        SendAndSave(config);
    }

    // The port names dialog: each input opens with its stored name, the customer edits some of
    // them, and Save sends everything the endpoint has
    void SaveInputNames(_Inout_ TestEndpoint& endpoint, _In_ std::initializer_list<std::pair<int, PCWSTR>> const edits)
    {
        auto settings = custom::ReadEndpointSettings(Endpoint(endpoint), endpoint.ClearedPortNames);

        std::map<uint8_t, custom::PortNameRow> rows{};

        for (auto const& stored : settings.SourcePortNames)
        {
            rows[stored.first] = { stored.first, stored.second, stored.second };
        }

        for (auto const& edit : edits)
        {
            auto const groupIndex = static_cast<uint8_t>(edit.first);
            auto& row = rows[groupIndex];

            row.GroupIndex = groupIndex;
            row.Text = winrt::hstring{ edit.second };
        }

        std::vector<custom::PortNameRow> inputs{};

        for (auto const& row : rows)
        {
            inputs.push_back(row.second);
        }

        std::vector<custom::ClearedPortName> cleared{};
        custom::CollectPortNames(inputs, midi2enum::Midi1PortFlow::MidiMessageSource, settings.SourcePortNames, cleared);

        auto const config = NewConfig(endpoint);
        custom::CopyInto(settings, config);
        custom::AddClearedPortNames(cleared, config);

        SendAndSave(config);

        endpoint.ClearedPortNames.Saved(settings, cleared);
    }

    // The customize dialog's Save, with the name box changed
    void SaveEndpointName(_Inout_ TestEndpoint& endpoint, _In_ winrt::hstring const& name)
    {
        auto const settings = custom::ReadEndpointSettings(Endpoint(endpoint), endpoint.ClearedPortNames);

        auto const config = NewConfig(endpoint);
        custom::CopyInto(settings, config);

        config.Name(name);
        config.Description(settings.Description);
        config.ImageFileName(settings.ImageFileName);
        config.ClearDisplayProperties(true);

        SendAndSave(config);
    }

    // The endpoint's properties change a moment after the call that changed them, so each check
    // waits for its value
    void VerifyInputName(_In_ TestEndpoint const& endpoint, _In_ uint8_t const groupIndex, _In_ PCWSTR expected)
    {
        winrt::hstring shown{};

        auto const matched = WaitFor([&]
            {
                auto const settings = Shown(endpoint);
                auto const found = settings.SourcePortNames.find(groupIndex);

                shown = found == settings.SourcePortNames.end() ? winrt::hstring{} : found->second;

                return shown == expected;
            }, PropertyWait);

        if (!matched)
        {
            Log::Comment(String().Format(L"Input %u shows \"%s\", not \"%s\"",
                static_cast<uint32_t>(groupIndex) + 1, shown.c_str(), expected));
        }

        VERIFY_IS_TRUE(matched, L"the input shows the expected custom name");
    }

    void VerifyEndpointName(_In_ TestEndpoint const& endpoint, _In_ PCWSTR expected)
    {
        winrt::hstring shown{};

        auto const matched = WaitFor([&]
            {
                shown = Shown(endpoint).Name;

                return shown == expected;
            }, PropertyWait);

        if (!matched)
        {
            Log::Comment(String().Format(L"The endpoint shows the name \"%s\", not \"%s\"", shown.c_str(), expected));
        }

        VERIFY_IS_TRUE(matched, L"the endpoint keeps its custom name");
    }

    void VerifyLatencyAndNamingStyle(_In_ TestEndpoint const& endpoint)
    {
        custom::EndpointSettings shown{};

        auto const matched = WaitFor([&]
            {
                shown = Shown(endpoint);

                return shown.OutgoingLatencyTicks == TestLatencyTicks &&
                    shown.Midi1PortNamingApproach == midi2enum::Midi1PortNamingApproach::UseNewStyle;
            }, PropertyWait);

        if (!matched)
        {
            Log::Comment(String().Format(L"The endpoint shows a latency of %lld ticks and naming style %d",
                shown.OutgoingLatencyTicks, static_cast<int32_t>(shown.Midi1PortNamingApproach)));
        }

        VERIFY_IS_TRUE(matched, L"the endpoint keeps its latency and naming style");
    }

    void RestartService(_In_ TestEndpoint const& endpoint)
    {
        ServiceHandle const manager{ ::OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT), &::CloseServiceHandle };
        VERIFY_IS_TRUE(manager != nullptr, L"the service control manager opens");

        ServiceHandle const service{
            ::OpenServiceW(manager.get(), L"midisrv", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS),
            &::CloseServiceHandle };

        VERIFY_IS_TRUE(service != nullptr, L"the MIDI service opens");

        auto const reaches = [&](DWORD const state)
            {
                return WaitFor([&]
                    {
                        SERVICE_STATUS_PROCESS status{};
                        DWORD needed{ 0 };

                        return ::QueryServiceStatusEx(service.get(), SC_STATUS_PROCESS_INFO,
                            reinterpret_cast<LPBYTE>(&status), sizeof(status), &needed) &&
                            status.dwCurrentState == state;
                    }, ServiceWait);
            };

        // The first stop can fail while a client still holds the service, where a second one
        // works (issue #1174)
        bool stopped{ false };

        for (uint32_t attempt = 0; attempt < 2 && !stopped; attempt++)
        {
            SERVICE_STATUS status{};
            ::ControlService(service.get(), SERVICE_CONTROL_STOP, &status);

            stopped = reaches(SERVICE_STOPPED);
        }

        VERIFY_IS_TRUE(stopped, L"the MIDI service stops. If it does not, close every MIDI app and try again.");

        // read too soon, the old endpoint's properties would still be there
        WaitFor([&] { return !IsPresent(endpoint.EndpointDeviceId); }, EndpointGoneWait);

        if (!::StartServiceW(service.get(), 0, nullptr))
        {
            // any call into the service starts it on demand, which may have happened first
            VERIFY_ARE_EQUAL(::GetLastError(), static_cast<DWORD>(ERROR_SERVICE_ALREADY_RUNNING));
        }

        VERIFY_IS_TRUE(reaches(SERVICE_RUNNING), L"the MIDI service starts");
        VERIFY_IS_TRUE(WaitFor([&] { return IsPresent(endpoint.EndpointDeviceId); }, EndpointWait), L"the endpoint comes back");
    }
}


void EndpointCustomizationHardwareTests::TestPortNamesFromTwoSavesSurviveARestart()
{
    TestEndpoint endpoint{};

    if (!TryPrepare(endpoint, true))
    {
        return;
    }

    RemoveWhenDone const cleanup{ endpoint };

    SaveInputNames(endpoint, { { 0, FirstInputName } });
    RestartService(endpoint);

    VerifyInputName(endpoint, 0, FirstInputName);

    // a save which changes only input 2 used to drop input 1 from the stored list
    SaveInputNames(endpoint, { { 1, SecondInputName } });
    RestartService(endpoint);

    VerifyInputName(endpoint, 1, SecondInputName);
    VerifyInputName(endpoint, 0, FirstInputName);
}


void EndpointCustomizationHardwareTests::TestEverySaveKeepsWhatItDoesNotShow()
{
    TestEndpoint endpoint{};

    if (!TryPrepare(endpoint, true))
    {
        return;
    }

    RemoveWhenDone const cleanup{ endpoint };

    SaveLatencyAndNamingStyle(endpoint);
    VerifyLatencyAndNamingStyle(endpoint);

    // the port names dialog shows neither the latency nor the endpoint's name
    SaveInputNames(endpoint, { { 0, FirstInputName } });
    VerifyLatencyAndNamingStyle(endpoint);

    // and the customize dialog shows neither the latency nor the port names
    SaveEndpointName(endpoint, TestEndpointName);
    VerifyLatencyAndNamingStyle(endpoint);
    VerifyInputName(endpoint, 0, FirstInputName);

    SaveInputNames(endpoint, { { 1, SecondInputName } });
    VerifyLatencyAndNamingStyle(endpoint);
    VerifyEndpointName(endpoint, TestEndpointName);
    VerifyInputName(endpoint, 0, FirstInputName);

    RestartService(endpoint);

    VerifyInputName(endpoint, 1, SecondInputName);
    VerifyInputName(endpoint, 0, FirstInputName);
    VerifyEndpointName(endpoint, TestEndpointName);
    VerifyLatencyAndNamingStyle(endpoint);

    // no dialog sends the choice to use the latency, so it stays as it was saved
    auto const stored = StoredCustomization(endpoint);

    VERIFY_IS_TRUE(stored != nullptr, L"the service holds the customization");
    VERIFY_IS_TRUE(stored.UseCustomOutgoingLatency(), L"the stored customization still uses the latency");
}


void EndpointCustomizationHardwareTests::TestClearedPortNameStaysCleared()
{
    TestEndpoint endpoint{};

    if (!TryPrepare(endpoint, true))
    {
        return;
    }

    RemoveWhenDone const cleanup{ endpoint };

    SaveInputNames(endpoint, { { 0, FirstInputName }, { 1, SecondInputName } });
    VerifyInputName(endpoint, 0, FirstInputName);
    VerifyInputName(endpoint, 1, SecondInputName);

    // the customer empties input 1
    SaveInputNames(endpoint, { { 0, L"" } });
    VerifyInputName(endpoint, 1, SecondInputName);

    if (Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
    {
        // before the fix, the name stayed on the endpoint until the service restarted
        VerifyInputName(endpoint, 0, L"");
    }

    // a later save must not store the cleared name again
    SaveInputNames(endpoint, { { 2, ThirdInputName } });
    RestartService(endpoint);

    VerifyInputName(endpoint, 2, ThirdInputName);
    VerifyInputName(endpoint, 1, SecondInputName);
    VerifyInputName(endpoint, 0, L"");
}


void EndpointCustomizationHardwareTests::TestRemovalClearsPortNamesAtOnce()
{
    if (!Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Without the endpoint customization fix, port names stay until the service restarts.");
        return;
    }

    TestEndpoint endpoint{};

    if (!TryPrepare(endpoint, false))
    {
        return;
    }

    RemoveWhenDone const cleanup{ endpoint };

    SaveInputNames(endpoint, { { 0, FirstInputName } });
    VerifyInputName(endpoint, 0, FirstInputName);

    VERIFY_IS_TRUE(Remove(endpoint), L"the customization is removed");
    VERIFY_IS_TRUE(StoredCustomization(endpoint) == nullptr, L"the service no longer holds it");

    VerifyInputName(endpoint, 0, L"");
}


void EndpointCustomizationHardwareTests::TestClearedPortNameLeavesTheEndpointAtOnce()
{
    if (!Feature_Servicing_MIDI2EndpointCustomizationEnhancements::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Without the endpoint customization fix, a cleared port name stays until the service restarts.");
        return;
    }

    TestEndpoint endpoint{};

    if (!TryPrepare(endpoint, false))
    {
        return;
    }

    RemoveWhenDone const cleanup{ endpoint };

    SaveInputNames(endpoint, { { 0, FirstInputName }, { 1, SecondInputName } });
    VerifyInputName(endpoint, 0, FirstInputName);
    VerifyInputName(endpoint, 1, SecondInputName);

    // the customer empties input 1
    SaveInputNames(endpoint, { { 0, L"" } });

    VerifyInputName(endpoint, 1, SecondInputName);
    VerifyInputName(endpoint, 0, L"");

    auto const stored = StoredCustomization(endpoint);

    VERIFY_IS_TRUE(stored != nullptr, L"the service holds the customization");
    VERIFY_IS_FALSE(stored.Midi1SourcePortCustomNames().HasKey(0), L"the service no longer holds the cleared name");
    VERIFY_IS_TRUE(stored.Midi1SourcePortCustomNames().HasKey(1), L"the service still holds the other name");
}
