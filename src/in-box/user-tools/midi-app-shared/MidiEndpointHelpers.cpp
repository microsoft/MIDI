// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiEndpointHelpers.h"

namespace midi2 = ::winrt::Windows::Devices::Midi2;
namespace midi2enum = ::winrt::Windows::Devices::Midi2::Enumeration;

namespace midiapp
{
    namespace
    {
        constexpr wchar_t EndpointImageFolder[] = LR"(%allusersprofile%\Microsoft\MIDI\Assets\Endpoints\)";

        // Marks an image the customer supplied, so the folder stays readable next to the
        // defaults which ship with the product.
        constexpr wchar_t EndpointImageNamePrefix[] = L"ep-";

        // An endpoint image is a small icon or photo. The cap is here so that picking a huge
        // file fails quickly instead of reading it all in to compare it.
        constexpr uint64_t MaxEndpointImageBytes = 32ull * 1024 * 1024;

        winrt::hstring ExpandPath(std::wstring const& path) noexcept
        {
            wchar_t expanded[MAX_PATH + 1]{};

            if (::ExpandEnvironmentStringsW(path.c_str(), expanded, ARRAYSIZE(expanded)) == 0)
            {
                return {};
            }

            return winrt::hstring{ expanded };
        }

        std::vector<uint8_t> ReadAllBytes(std::wstring const& path) noexcept
        {
            std::vector<uint8_t> bytes{};

            wil::unique_hfile file{ ::CreateFileW(
                path.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr) };

            if (!file)
            {
                return bytes;
            }

            LARGE_INTEGER size{};

            if (!::GetFileSizeEx(file.get(), &size) ||
                size.QuadPart <= 0 ||
                static_cast<uint64_t>(size.QuadPart) > MaxEndpointImageBytes)
            {
                return bytes;
            }

            bytes.resize(static_cast<size_t>(size.QuadPart));

            DWORD read{ 0 };

            if (!::ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
                read != bytes.size())
            {
                bytes.clear();
            }

            return bytes;
        }
    }

    // The stored value is a bare file name inside the shared assets folder. Anything with a
    // path separator, a wildcard, or an environment variable is rejected rather than
    // resolved, so a tampered configuration cannot point us at an arbitrary file.
    winrt::hstring ResolveEndpointImagePath(winrt::hstring const& imageFileName) noexcept
    {
        try
        {
            if (imageFileName.empty() || imageFileName.size() > MAX_PATH)
            {
                return {};
            }

            for (auto const ch : imageFileName)
            {
                if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'%' || ch == L'?' || ch == L'*' || ch == L'"')
                {
                    return {};
                }
            }

            if (imageFileName == L"." || imageFileName == L"..")
            {
                return {};
            }

            auto const fullPath = ExpandPath(std::wstring{ EndpointImageFolder } + std::wstring{ imageFileName });

            if (fullPath.empty() || !::PathFileExistsW(fullPath.c_str()))
            {
                return {};
            }

            return fullPath;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return {};
    }

    _Use_decl_annotations_
    winrt::hstring ImportEndpointImage(winrt::hstring const& sourcePath) noexcept
    {
        try
        {
            if (sourcePath.empty())
            {
                return {};
            }

            std::wstring const source{ sourcePath };

            auto const separator = source.find_last_of(L"\\/");

            std::wstring name = separator == std::wstring::npos ?
                source :
                source.substr(separator + 1);

            // The stored value is read back as a bare name, so anything the resolver would
            // later refuse is dropped here rather than stored and silently ignored.
            std::wstring cleaned{};

            for (auto const ch : name)
            {
                if (ch == L'\\' || ch == L'/' || ch == L':' || ch == L'%' || ch == L'?' ||
                    ch == L'*' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|')
                {
                    continue;
                }

                cleaned += ch;
            }

            if (cleaned.empty() || cleaned == L"." || cleaned == L".." || cleaned.size() > MAX_PATH)
            {
                return {};
            }

            if (_wcsnicmp(cleaned.c_str(), EndpointImageNamePrefix, wcslen(EndpointImageNamePrefix)) != 0)
            {
                cleaned = EndpointImageNamePrefix + cleaned;
            }

            auto const folder = ExpandPath(EndpointImageFolder);

            if (folder.empty())
            {
                return {};
            }

            std::wstring const folderPath{ folder };

            if (!::PathFileExistsW(folderPath.c_str()))
            {
                ::CreateDirectoryW(folderPath.c_str(), nullptr);
            }

            auto const incoming = ReadAllBytes(source);

            if (incoming.empty())
            {
                return {};
            }

            auto const dot = cleaned.find_last_of(L'.');

            auto const stem = dot == std::wstring::npos ? cleaned : cleaned.substr(0, dot);
            auto const extension = dot == std::wstring::npos ? std::wstring{} : cleaned.substr(dot);

            // A name already in use by the same image is reused. One in use by a different image
            // belongs to another endpoint, so this gets its own name instead of replacing it.
            for (uint32_t attempt = 1; attempt <= 64; attempt++)
            {
                auto const candidate = attempt == 1 ?
                    stem + extension :
                    stem + L" (" + std::to_wstring(attempt) + L")" + extension;

                auto const destination = folderPath + candidate;

                if (::PathFileExistsW(destination.c_str()))
                {
                    if (ReadAllBytes(destination) == incoming)
                    {
                        return winrt::hstring{ candidate };
                    }

                    continue;
                }

                if (::CopyFileW(source.c_str(), destination.c_str(), TRUE))
                {
                    return winrt::hstring{ candidate };
                }

                return {};
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return {};
    }

    winrt::hstring DescribeGroup(
        midi2enum::MidiEndpointDeviceInformation const& endpoint,
        uint8_t groupIndex) noexcept
    {
        try
        {
            if (endpoint != nullptr)
            {
                midi2::MidiGroup const group{ groupIndex };

                for (auto const& functionBlock : endpoint.GetDeclaredFunctionBlocks())
                {
                    if (functionBlock.IncludesGroup(group) && !functionBlock.Name().empty())
                    {
                        return functionBlock.Name();
                    }
                }

                for (auto const& terminalBlock : endpoint.GetGroupTerminalBlocks())
                {
                    if (terminalBlock.IncludesGroup(group) && !terminalBlock.Name().empty())
                    {
                        return terminalBlock.Name();
                    }
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        return {};
    }

    // A block covers GroupCount contiguous groups starting at FirstGroup, so FirstGroup 3
    // with GroupCount 2 contributes groups 3 and 4.
    std::array<bool, 16> DeclaredGroups(midi2enum::MidiEndpointDeviceInformation const& endpoint) noexcept
    {
        std::array<bool, 16> declared{};

        try
        {
            if (endpoint != nullptr)
            {
                auto const cover = [&declared](uint8_t firstGroupIndex, uint8_t groupCount) noexcept
                    {
                        auto const last = static_cast<uint32_t>(firstGroupIndex) + groupCount;

                        for (uint32_t i = firstGroupIndex; i < last && i < declared.size(); i++)
                        {
                            declared[i] = true;
                        }
                    };

                for (auto const& functionBlock : endpoint.GetDeclaredFunctionBlocks())
                {
                    if (auto const first = functionBlock.FirstGroup())
                    {
                        cover(first.Index(), functionBlock.GroupCount());
                    }
                }

                for (auto const& terminalBlock : endpoint.GetGroupTerminalBlocks())
                {
                    if (auto const first = terminalBlock.FirstGroup())
                    {
                        cover(first.Index(), terminalBlock.GroupCount());
                    }
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }

        // a device that declares nothing still has to be usable
        if (std::none_of(declared.begin(), declared.end(), [](bool value) { return value; }))
        {
            declared.fill(true);
        }

        return declared;
    }

    std::vector<midi2enum::MidiEndpointDeviceInformation> SortedEndpoints(
        midi2enum::MidiEndpointDeviceWatcher const& watcher) noexcept
    {
        std::vector<midi2enum::MidiEndpointDeviceInformation> devices{};

        if (watcher == nullptr)
        {
            return devices;
        }

        // The watcher rewrites this map from its own thread as devices arrive, and enumerating a
        // WinRT collection while it changes throws E_CHANGED_STATE. Taking another pass is the
        // remedy; during discovery the map settles within one or two.
        for (int attempt = 0; attempt < 5; attempt++)
        {
            devices.clear();

            try
            {
                for (auto const& entry : watcher.EnumeratedEndpointDevices())
                {
                    devices.push_back(entry.Value());
                }

                break;
            }
            catch (winrt::hresult_changed_state const&)
            {
                continue;
            }
            catch (winrt::hresult_error const& ex)
            {
                // Not LOG_CAUGHT_EXCEPTION: without wil/cppwinrt.h in the consuming app, WIL
                // cannot classify a cppwinrt exception and fail-fasts the process instead.
                LOG_HR(static_cast<HRESULT>(ex.code()));

                devices.clear();

                return devices;
            }
            catch (...)
            {
                LOG_HR(E_UNEXPECTED);

                devices.clear();

                return devices;
            }
        }

        try
        {
            std::sort(devices.begin(), devices.end(),
                [](auto const& left, auto const& right)
                {
                    return ::CompareStringOrdinal(
                        left.Name().c_str(), -1, right.Name().c_str(), -1, TRUE) == CSTR_LESS_THAN;
                });
        }
        catch (...)
        {
            LOG_HR(E_UNEXPECTED);
        }

        return devices;
    }

    bool EndpointIdsMatch(winrt::hstring const& left, winrt::hstring const& right) noexcept
    {
        return ::CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
    }
}
