// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Windows MIDI Services
// Test-created software device node tracking and cleanup
// https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDITESTDEVICENODES_H
#define MIDITESTDEVICENODES_H

#include <windows.h>
#include <cfgmgr32.h>

#include <cwchar>
#include <cwctype>
#include <set>
#include <string>
#include <vector>

#include <WexTestClass.h>

#pragma comment(lib, "cfgmgr32.lib")

// Removes the software device nodes a single TAEF test method caused to be created.
//
// Windows MIDI Services deactivates an endpoint rather than deleting it. That is deliberate:
// CMidiDeviceManager::RemoveEndpoint only deactivates, so properties cached on the node - the
// user's custom endpoint name, the device-supplied name, the product instance id - survive a
// disconnect and come back on reconnect. Nothing in the product ever deletes one.
//
// A test which creates a loopback, a virtual device, a network host or a Bluetooth endpoint
// therefore leaves a node behind on every single run, along with a node for each MIDI 1.0 port
// created alongside it. After a few hundred runs there are thousands of them, enumeration slows
// down enough to fail timing-sensitive tests, and Device Manager takes seconds to open.
//
// Usage, from TEST_METHOD_SETUP and TEST_METHOD_CLEANUP so that running one test method on its
// own still cleans up after itself:
//
//     MidiTest::DeviceNodeTracker m_deviceNodeTracker{};
//
//     bool MyTests::TestSetup()   { m_deviceNodeTracker.Start(); return true; }
//     bool MyTests::TestCleanup() { m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart(); return true; }
//
namespace MidiTest
{
    namespace DeviceNodeDetails
    {
        // The two SWD enumerator branches MIDI device nodes are published under. MIDISRV carries
        // the UMP endpoints the service creates, MMDEVAPI the MIDI 1.0 ports which face WinMM.
        inline PCWSTR const MidiEnumeratorBranches[] =
        {
            L"SWD\\MIDISRV",
            L"SWD\\MMDEVAPI"
        };

        // These represent physical instruments. Their nodes carry configuration for hardware
        // which may simply be switched off or out of Bluetooth range, so they are never removed
        // even if one happened to appear while a test was running.
        //
        // BLEMIDI is the current Bluetooth transport's TRANSPORT_CODE. BLE10 and BLE20 are the
        // in-box WinRT Bluetooth MIDI 1.0 stack, which still has nodes on machines which used it.
        inline PCWSTR const HardwareBackedTransportCodes[] =
        {
            L"KS",
            L"KSA",
            L"BLEMIDI",
            L"BLE10",
            L"BLE20"
        };

        inline std::wstring ToUpper(_In_ std::wstring const& value)
        {
            std::wstring result{ value };

            for (auto& character : result)
            {
                character = static_cast<wchar_t>(towupper(character));
            }

            return result;
        }

        inline bool StartsWith(_In_ std::wstring const& value, _In_ std::wstring const& prefix)
        {
            return value.compare(0, prefix.length(), prefix) == 0;
        }

        // Transport code out of the leaf of a device instance id:
        //
        //   MIDIU_LOOP_A_Contoso    -> LOOP     a service-created UMP endpoint
        //   MIDII_51C733B7.NET2UDP  -> NET2UDP  a WinMM-facing MIDI 1.0 port
        //
        // Anything which does not parse returns an empty string and is never a candidate for
        // removal. That is what keeps the real audio endpoints under SWD\MMDEVAPI, whose leaves
        // look like {0.0.0.00000000}.{guid}, out of range.
        inline std::wstring TransportCodeFromInstanceId(_In_ std::wstring const& instanceId)
        {
            auto const lastSeparator = instanceId.find_last_of(L'\\');
            auto const leaf = (lastSeparator == std::wstring::npos)
                ? instanceId
                : instanceId.substr(lastSeparator + 1);

            if (StartsWith(leaf, L"MIDIU_"))
            {
                auto const codeStart = wcslen(L"MIDIU_");
                auto const codeEnd = leaf.find(L'_', codeStart);

                if (codeEnd == std::wstring::npos || codeEnd == codeStart)
                {
                    return {};
                }

                return ToUpper(leaf.substr(codeStart, codeEnd - codeStart));
            }

            if (StartsWith(leaf, L"MIDII_"))
            {
                auto const codeStart = leaf.find_last_of(L'.');

                if (codeStart == std::wstring::npos || codeStart + 1 >= leaf.length())
                {
                    return {};
                }

                return ToUpper(leaf.substr(codeStart + 1));
            }

            return {};
        }

        inline bool IsHardwareBackedTransport(_In_ std::wstring const& transportCode)
        {
            for (auto const& hardwareCode : HardwareBackedTransportCodes)
            {
                if (transportCode == hardwareCode)
                {
                    return true;
                }
            }

            return false;
        }

        // Every MIDI device node under both enumerators, present or not. The Configuration
        // Manager is used rather than the registry because the Enum subtree is access-checked
        // and because a phantom node has to be visible here for it to be removable at all.
        inline std::vector<std::wstring> EnumerateMidiDeviceNodeIds()
        {
            std::vector<std::wstring> deviceNodeIds{};

            for (auto const& branch : MidiEnumeratorBranches)
            {
                // The list can grow between sizing it and fetching it, so the pair is retried.
                for (int attempt = 0; attempt < 8; attempt++)
                {
                    ULONG characterCount{ 0 };

                    if (CM_Get_Device_ID_List_SizeW(&characterCount, branch, CM_GETIDLIST_FILTER_ENUMERATOR) != CR_SUCCESS)
                    {
                        break;
                    }

                    if (characterCount < 2)
                    {
                        break;
                    }

                    std::vector<wchar_t> buffer(characterCount, L'\0');

                    auto const result = CM_Get_Device_ID_ListW(branch, buffer.data(), characterCount, CM_GETIDLIST_FILTER_ENUMERATOR);

                    if (result == CR_BUFFER_SMALL)
                    {
                        continue;
                    }

                    if (result != CR_SUCCESS)
                    {
                        break;
                    }

                    for (PCWSTR entry = buffer.data(); *entry != L'\0'; entry += wcslen(entry) + 1)
                    {
                        deviceNodeIds.push_back(entry);
                    }

                    break;
                }
            }

            return deviceNodeIds;
        }

        // Presence is decided by CM_LOCATE_DEVNODE_NORMAL, which succeeds only for a device
        // currently configured in the device tree.
        inline bool IsDeviceNodePresent(_In_ std::wstring const& instanceId)
        {
            DEVINST deviceInstance{};

            return CM_Locate_DevNodeW(
                &deviceInstance,
                const_cast<DEVINSTID_W>(instanceId.c_str()),
                CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS;
        }

        inline std::wstring GetDeviceNodeFriendlyName(_In_ std::wstring const& instanceId)
        {
            // DEVPKEY_Device_FriendlyName, spelled out so that this header does not have to
            // drag INITGUID and devpkey.h into every test project which includes it.
            static const DEVPROPKEY friendlyNameKey
            {
                { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
                14
            };

            DEVINST deviceInstance{};

            if (CM_Locate_DevNodeW(
                    &deviceInstance,
                    const_cast<DEVINSTID_W>(instanceId.c_str()),
                    CM_LOCATE_DEVNODE_PHANTOM) != CR_SUCCESS)
            {
                return {};
            }

            DEVPROPTYPE propertyType{ 0 };
            ULONG byteCount{ 0 };

            CM_Get_DevNode_PropertyW(deviceInstance, &friendlyNameKey, &propertyType, nullptr, &byteCount, 0);

            if (byteCount < sizeof(wchar_t))
            {
                return {};
            }

            std::vector<BYTE> buffer(byteCount, static_cast<BYTE>(0));

            if (CM_Get_DevNode_PropertyW(deviceInstance, &friendlyNameKey, &propertyType, buffer.data(), &byteCount, 0) != CR_SUCCESS ||
                propertyType != DEVPROP_TYPE_STRING)
            {
                return {};
            }

            std::wstring name{ reinterpret_cast<PCWSTR>(buffer.data()), (byteCount / sizeof(wchar_t)) - 1 };

            return name;
        }

        // Locates the node as a phantom, so one which became present since it was last checked
        // is refused here rather than removed out from under a running endpoint.
        inline CONFIGRET RemoveDeviceNode(_In_ std::wstring const& instanceId)
        {
            DEVINST deviceInstance{};

            auto const located = CM_Locate_DevNodeW(
                &deviceInstance,
                const_cast<DEVINSTID_W>(instanceId.c_str()),
                CM_LOCATE_DEVNODE_PHANTOM);

            if (located != CR_SUCCESS)
            {
                return located;
            }

            return CM_Uninstall_DevNode(deviceInstance, 0);
        }
    }

    // How long to wait for the service to finish deactivating the endpoints a test asked it to
    // remove. Teardown is asynchronous, and a node which is still present is never removed.
    inline constexpr uint32_t DefaultDeviceNodeTeardownWaitMilliseconds = 5000;

    class DeviceNodeTracker
    {
    public:

        // Records every MIDI device node which already exists. Call this from TEST_METHOD_SETUP.
        void Start() noexcept try
        {
            m_started = true;
            m_baseline.clear();

            for (auto const& instanceId : DeviceNodeDetails::EnumerateMidiDeviceNodeIds())
            {
                m_baseline.insert(DeviceNodeDetails::ToUpper(instanceId));
            }
        }
        catch (...)
        {
            // A tracker which could not take a baseline removes nothing, rather than mistaking
            // every node on the machine for one this test created.
            m_started = false;
            m_baseline.clear();
        }

        // The MIDI device nodes which did not exist when Start() was called. Hardware-backed
        // transports are excluded, so an instrument switched on while the test was running is
        // never a candidate.
        std::vector<std::wstring> FindDeviceNodesCreatedSinceStart() const
        {
            std::vector<std::wstring> created{};

            if (!m_started)
            {
                return created;
            }

            for (auto const& instanceId : DeviceNodeDetails::EnumerateMidiDeviceNodeIds())
            {
                if (m_baseline.count(DeviceNodeDetails::ToUpper(instanceId)) > 0)
                {
                    continue;
                }

                auto const transportCode = DeviceNodeDetails::TransportCodeFromInstanceId(instanceId);

                if (transportCode.empty() || DeviceNodeDetails::IsHardwareBackedTransport(transportCode))
                {
                    continue;
                }

                created.push_back(instanceId);
            }

            return created;
        }

        // Removes the UMP endpoints and MIDI 1.0 ports this test caused to be created, and
        // returns how many nodes were removed. Call this from TEST_METHOD_CLEANUP.
        //
        // Never fails a test. A node which cannot be removed is reported and left alone: the
        // point is to keep a development machine tidy, not to turn a passing test red.
        uint32_t RemoveDeviceNodesCreatedSinceStart(
            _In_ uint32_t const teardownWaitMilliseconds = DefaultDeviceNodeTeardownWaitMilliseconds) noexcept try
        {
            auto created = FindDeviceNodesCreatedSinceStart();

            m_started = false;
            m_baseline.clear();

            if (created.empty())
            {
                return 0;
            }

            WaitForDeviceNodeTeardown(created, teardownWaitMilliseconds);

            uint32_t removedCount{ 0 };
            uint32_t stillPresentCount{ 0 };
            uint32_t failedCount{ 0 };
            CONFIGRET firstFailure{ CR_SUCCESS };

            for (auto const& instanceId : created)
            {
                // A node which is still configured belongs to an endpoint the test left running.
                // Removing one would yank it out from under whatever still has it open.
                if (DeviceNodeDetails::IsDeviceNodePresent(instanceId))
                {
                    stillPresentCount++;
                    continue;
                }

                auto const friendlyName = DeviceNodeDetails::GetDeviceNodeFriendlyName(instanceId);
                auto const result = DeviceNodeDetails::RemoveDeviceNode(instanceId);

                auto const description = instanceId +
                    (friendlyName.empty() ? std::wstring{} : L" (" + friendlyName + L")");

                if (result == CR_SUCCESS)
                {
                    removedCount++;

                    WEX::Logging::Log::Comment((L"Removed test-created device node " + description).c_str());
                }
                else
                {
                    failedCount++;

                    if (firstFailure == CR_SUCCESS)
                    {
                        firstFailure = result;
                    }

                    WEX::Logging::Log::Comment((L"Left test-created device node " + description).c_str());
                }
            }

            // One warning for the whole pass rather than one per node, because the usual cause
            // is a single condition which applies to all of them.
            if (failedCount > 0)
            {
                std::wstring message =
                    std::to_wstring(failedCount) +
                    L" test-created device node(s) could not be removed. CM_Uninstall_DevNode returned " +
                    std::to_wstring(firstFailure) + L".";

                if (firstFailure == CR_ACCESS_DENIED)
                {
                    message += L" Removing a device node needs an elevated prompt, so run the tests from one.";
                }

                WEX::Logging::Log::Warning(message.c_str());
            }

            if (stillPresentCount > 0)
            {
                WEX::Logging::Log::Warning(
                    (std::to_wstring(stillPresentCount) +
                     L" test-created device node(s) were still present after teardown and were left in place. "
                     L"Run build\\remove_deactivated_midi_devices.ps1 to clear them.").c_str());
            }

            return removedCount;
        }
        catch (...)
        {
            return 0;
        }

    private:

        // Gives the service time to finish deactivating what the test asked it to remove.
        // Returns as soon as everything has gone, so a test which cleaned up promptly does not
        // pay the whole wait.
        static void WaitForDeviceNodeTeardown(
            _In_ std::vector<std::wstring> const& instanceIds,
            _In_ uint32_t const waitMilliseconds)
        {
            constexpr uint32_t pollIntervalMilliseconds = 250;

            auto const deadline = GetTickCount64() + waitMilliseconds;

            while (GetTickCount64() < deadline)
            {
                bool anyStillPresent{ false };

                for (auto const& instanceId : instanceIds)
                {
                    if (DeviceNodeDetails::IsDeviceNodePresent(instanceId))
                    {
                        anyStillPresent = true;
                        break;
                    }
                }

                if (!anyStillPresent)
                {
                    return;
                }

                Sleep(pollIntervalMilliseconds);
            }
        }

        bool m_started{ false };
        std::set<std::wstring> m_baseline{};
    };
}

#endif
