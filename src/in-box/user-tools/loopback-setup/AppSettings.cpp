// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "AppSettings.h"

namespace midiloopbacksetup
{
    namespace
    {
        constexpr wchar_t SettingsKeyPath[] = LR"(Software\Microsoft\Windows MIDI Services\Tools\midiloopbacksetup)";

        constexpr wchar_t ValueRefreshIntervalSeconds[] = L"RefreshIntervalSeconds";
        constexpr wchar_t ValueSelectedPageIndex[] = L"SelectedPageIndex";
        constexpr wchar_t ValueLoopbackOrder[] = L"LoopbackOrder";
        constexpr wchar_t ValueBasicLoopbackOrder[] = L"BasicLoopbackOrder";

        // an association id never contains one, so a single string holds the whole list
        constexpr wchar_t OrderSeparator{ L';' };

        std::vector<std::wstring> SplitOrder(_In_ std::wstring const& text) noexcept
        {
            std::vector<std::wstring> ids{};

            try
            {
                size_t start{ 0 };

                while (start < text.size())
                {
                    auto end = text.find(OrderSeparator, start);

                    if (end == std::wstring::npos)
                    {
                        end = text.size();
                    }

                    if (end > start)
                    {
                        ids.push_back(text.substr(start, end - start));
                    }

                    start = end + 1;
                }
            }
            catch (...)
            {
                ids.clear();
            }

            return ids;
        }

        std::wstring JoinOrder(_In_ std::vector<std::wstring> const& ids) noexcept
        {
            try
            {
                std::wstring text{};

                for (auto const& id : ids)
                {
                    if (!text.empty())
                    {
                        text += OrderSeparator;
                    }

                    text += id;
                }

                return text;
            }
            catch (...)
            {
                return {};
            }
        }
    }

    AppSettings::AppSettings() noexcept :
        midiapp::MidiAppSettings(SettingsKeyPath)
    {
    }

    AppSettings& AppSettings::Current() noexcept
    {
        static AppSettings instance{};
        return instance;
    }

    void AppSettings::Load() noexcept
    {
        LoadShared();

        m_refreshIntervalSeconds = std::clamp(
            ReadDword(ValueRefreshIntervalSeconds, DefaultRefreshIntervalSeconds),
            MinimumRefreshIntervalSeconds,
            MaximumRefreshIntervalSeconds);

        auto const page = ReadDword(ValueSelectedPageIndex, PageIndexBasicLoopbacks);
        m_selectedPageIndex = page > PageIndexLoopbacks ? PageIndexBasicLoopbacks : page;

        m_loopbackOrder = SplitOrder(ReadString(ValueLoopbackOrder, {}));
        m_basicLoopbackOrder = SplitOrder(ReadString(ValueBasicLoopbackOrder, {}));
    }

    void AppSettings::RefreshIntervalSeconds(uint32_t value) noexcept
    {
        m_refreshIntervalSeconds = std::clamp(value, MinimumRefreshIntervalSeconds, MaximumRefreshIntervalSeconds);
        WriteDword(ValueRefreshIntervalSeconds, m_refreshIntervalSeconds);
    }

    void AppSettings::SelectedPageIndex(uint32_t value) noexcept
    {
        m_selectedPageIndex = value > PageIndexLoopbacks ? PageIndexBasicLoopbacks : value;
        WriteDword(ValueSelectedPageIndex, m_selectedPageIndex);
    }

    _Use_decl_annotations_
    void AppSettings::LoopbackOrder(std::vector<std::wstring> const& value) noexcept
    {
        try
        {
            m_loopbackOrder = value;
            WriteString(ValueLoopbackOrder, JoinOrder(m_loopbackOrder));
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void AppSettings::BasicLoopbackOrder(std::vector<std::wstring> const& value) noexcept
    {
        try
        {
            m_basicLoopbackOrder = value;
            WriteString(ValueBasicLoopbackOrder, JoinOrder(m_basicLoopbackOrder));
        }
        catch (...)
        {
        }
    }
}
