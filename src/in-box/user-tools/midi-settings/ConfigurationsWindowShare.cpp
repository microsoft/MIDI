// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This window reads and writes the Windows MIDI Services
// configuration file directly, through the configuration engine. Microsoft does not support direct
// manipulation of that file by anything other than the in-box MIDI tools. Its format, its name, its
// location and the registry value that selects it are implementation details and can change in any
// release without notice. Applications and third-party tools must use the Windows MIDI Services API
// (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

// The Share page: one package that sets up another PC the same way, pictures included. It only
// reads, so it works without administrator rights.

#include "pch.h"
#include "ConfigurationsWindow.xaml.h"

#include "AppSettings.h"
#include "BackgroundWork.h"
#include "ConfigShare.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace cfg = ::midisettings::configurations;
namespace engine = ::midisettings::configengine;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;

namespace winrt::midisettings::implementation
{
    namespace
    {
        struct ShareRow
        {
            engine::ConfigPart Part{ engine::ConfigPart::None };
            wchar_t Glyph{ 0 };
            std::wstring_view LabelKey{};
            winrt::hstring What{};
        };

        winrt::hstring HostsAndConnections(_In_ size_t const hosts, _In_ size_t const connections)
        {
            std::vector<winrt::hstring> parts{};

            if (hosts > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountHosts", hosts));
            }

            if (connections > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountConnections", connections));
            }

            return cfg::ListText(parts);
        }

        winrt::hstring OtherSettingsText(
            _In_ engine::ConfigInventory const& inventory,
            _In_ bool const hasPortNames)
        {
            std::vector<winrt::hstring> parts{};

            if (!inventory.SynthMode.empty())
            {
                parts.push_back(res::GetString(L"CfgShareSynthMode"));
            }

            if (hasPortNames)
            {
                parts.push_back(res::GetString(parts.empty() ? L"CfgSharePortNamesFirst" : L"CfgSharePortNames"));
            }

            if (!inventory.OtherSections.empty())
            {
                parts.push_back(cfg::CountText(parts.empty() ? L"CfgShareOtherTransportsFirst" : L"CfgShareOtherTransports", inventory.OtherSections.size()));
            }

            return cfg::ListText(parts);
        }
    }

    void ConfigurationsWindow::RenderSharePage() noexcept
    {
        try
        {
            // The one in use to start with, or the first there is.
            if (FindConfiguration(m_shareFileName) == nullptr)
            {
                auto const* inUse = FindConfiguration(RunningFileName());

                m_shareFileName = inUse != nullptr ? inUse->FileName :
                    m_configurations.empty() ? std::wstring{} : m_configurations.front().FileName;
            }

            m_suppressShareHandling = true;

            auto const combo = ShareConfigurationComboBox();
            combo.Items().Clear();

            int32_t selected{ -1 };
            auto const running = RunningFileName();

            for (auto const& entry : m_configurations)
            {
                auto const name = cfg::ConfigDisplayName(entry);

                controls::ComboBoxItem item{};
                item.Content(winrt::box_value(engine::SameText(entry.FileName, running) ?
                    res::FormatString(L"CfgNameInUseFormat", name) :
                    winrt::hstring{ name }));
                item.Tag(winrt::box_value(winrt::hstring{ entry.FileName }));

                if (engine::SameText(entry.FileName, m_shareFileName))
                {
                    selected = static_cast<int32_t>(combo.Items().Size());
                }

                combo.Items().Append(item);
            }

            combo.SelectedIndex(selected);

            m_suppressShareHandling = false;

            // A new pick starts with its own name. A name typed for this one stays.
            if (!engine::SameText(m_sharePackageNameFor, m_shareFileName))
            {
                auto const* entry = FindConfiguration(m_shareFileName);

                SharePackageNameTextBox().Text(entry != nullptr ? winrt::hstring{ cfg::ConfigDisplayName(*entry) } : winrt::hstring{});
                m_sharePackageNameFor = m_shareFileName;
            }

            RenderShareIncludes();
            ApplyWorkState();
        }
        catch (...)
        {
            m_suppressShareHandling = false;
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to show what can be shared.");
        }
    }

    void ConfigurationsWindow::RenderShareIncludes() noexcept
    {
        try
        {
            // what was unticked stays unticked when the page is drawn again for the same one
            std::vector<engine::ConfigPart> unticked{};

            for (auto const& [part, check] : m_shareChecks)
            {
                if (check.IsChecked() == nullptr || !check.IsChecked().GetBoolean())
                {
                    unticked.push_back(part);
                }
            }

            auto const keepUnticked = engine::SameText(m_shareChecksFor, m_shareFileName) && !m_shareChecks.empty();

            auto const panel = ShareIncludePanel();
            panel.Children().Clear();
            m_shareChecks.clear();
            m_shareChecksFor = m_shareFileName;

            auto const* entry = FindConfiguration(m_shareFileName);

            if (entry == nullptr)
            {
                return;
            }

            auto const& inventory = entry->Inventory;
            auto const shareable = engine::ShareableParts(m_environment, inventory);
            auto const hasPortNames = (shareable & engine::PartBit(engine::ConfigPart::OtherSettings)) != 0 &&
                m_environment.ReadPortNaming && m_environment.ReadPortNaming() != engine::PortNaming::None;

            std::vector<winrt::hstring> deviceParts{};

            if (!inventory.DeviceNames.empty())
            {
                deviceParts.push_back(cfg::CountText(L"CfgCountNames", inventory.DeviceNames.size()));
            }

            if (!inventory.Pictures.empty())
            {
                deviceParts.push_back(cfg::CountText(L"CfgCountPictures", inventory.Pictures.size()));
            }

            std::vector<ShareRow> const rows
            {
                { engine::ConfigPart::DeviceNames, 0xE8EC, L"CfgPartDeviceNames", cfg::ListText(deviceParts) },
                { engine::ConfigPart::Loopbacks, 0xE8EE, L"CfgPartLoopbacks", cfg::CountText(L"CfgCountLoopbacks", inventory.Loopbacks.size()) },
                { engine::ConfigPart::NetworkMidi2, 0xE839, L"CfgPartNetworkMidi2",
                    HostsAndConnections(inventory.HostCount(engine::NetworkKind::NetworkMidi2), inventory.ClientCount(engine::NetworkKind::NetworkMidi2)) },
                { engine::ConfigPart::RtpMidi, 0xE71B, L"CfgPartRtpMidi",
                    HostsAndConnections(inventory.HostCount(engine::NetworkKind::RtpMidi), inventory.ClientCount(engine::NetworkKind::RtpMidi)) },
                { engine::ConfigPart::Bluetooth, 0xE702, L"CfgPartBluetooth", cfg::CountText(L"CfgCountDevices", inventory.BluetoothDevices.size()) },
                { engine::ConfigPart::OtherSettings, 0xE713, L"CfgPartOtherSettings", OtherSettingsText(inventory, hasPortNames) },
            };

            auto weak = get_weak();

            for (size_t index = 0; index < rows.size(); ++index)
            {
                auto const& row = rows[index];
                auto const available = (shareable & engine::PartBit(row.Part)) != 0;
                auto const label = res::GetString(row.LabelKey);
                auto const what = available ? row.What : res::GetString(L"CfgShareNothing");

                controls::Border line{};
                line.Style(StyleNamed(L"CfgShareRowStyle"));

                if (index + 1 == rows.size())
                {
                    line.BorderThickness(xaml::Thickness{ 0, 0, 0, 0 });
                }

                controls::CheckBox check{};
                check.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);
                check.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
                check.VerticalAlignment(xaml::VerticalAlignment::Center);
                check.IsEnabled(available);
                check.IsChecked(available && !(keepUnticked && std::find(unticked.begin(), unticked.end(), row.Part) != unticked.end()));

                controls::Grid content{};
                content.ColumnSpacing(12);

                for (auto const width : { xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star), xaml::GridLengthHelper::Auto() })
                {
                    controls::ColumnDefinition column{};
                    column.Width(width);
                    content.ColumnDefinitions().Append(column);
                }

                auto glyph = MakeGlyph(row.Glyph, L"CfgContentsGlyphStyle");
                glyph.FontSize(16);
                glyph.Width(18);
                content.Children().Append(glyph);

                auto labelText = MakeText(label, L"CfgRowTitleStyle");
                labelText.VerticalAlignment(xaml::VerticalAlignment::Center);
                controls::Grid::SetColumn(labelText, 1);
                content.Children().Append(labelText);

                auto whatText = MakeText(what, L"CfgItemStateStyle");
                controls::Grid::SetColumn(whatText, 2);
                content.Children().Append(whatText);

                check.Content(content);
                automation::AutomationProperties::SetName(check, res::FormatString(L"CfgShareIncludeNameFormat", label, what));

                check.Click([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->ShareSavedBar().IsOpen(false);
                            strong->ApplyWorkState();
                        }
                    });

                line.Child(check);
                panel.Children().Append(line);

                m_shareChecks.emplace_back(row.Part, check);
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show what to include.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnShareConfigurationChanged(foundation::IInspectable const&, controls::SelectionChangedEventArgs const&)
    {
        try
        {
            if (m_suppressShareHandling)
            {
                return;
            }

            auto const item = ShareConfigurationComboBox().SelectedItem().try_as<controls::ComboBoxItem>();

            if (item == nullptr)
            {
                return;
            }

            m_shareFileName = std::wstring{ winrt::unbox_value_or<winrt::hstring>(item.Tag(), winrt::hstring{}) };

            ShareSavedBar().IsOpen(false);
            ShareStatusBar().IsOpen(false);

            RenderSharePage();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change what's shared.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget ConfigurationsWindow::OnSavePackageClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto lifetime = get_strong();

        try
        {
            auto const* entry = FindConfiguration(m_shareFileName);

            if (entry == nullptr)
            {
                co_return;
            }

            uint32_t parts{ 0 };

            for (auto const& [part, check] : m_shareChecks)
            {
                if (check.IsEnabled() && check.IsChecked() != nullptr && check.IsChecked().GetBoolean())
                {
                    parts |= engine::PartBit(part);
                }
            }

            if (parts == 0)
            {
                ShowStatus(ShareStatusBar(), controls::InfoBarSeverity::Warning, res::GetString(L"CfgShareNothingChosenTitle"), res::GetString(L"CfgShareNothingChosenMessage"));
                co_return;
            }

            auto packageName = engine::CleanConfigName(std::wstring{ SharePackageNameTextBox().Text() });

            if (packageName.empty())
            {
                packageName = cfg::ConfigDisplayName(*entry);
            }

            std::wstring const note{ ShareNoteTextBox().Text() };
            auto const fileName = m_shareFileName;

            if (!BeginWork())
            {
                co_return;
            }

            auto const path = cfg::ShowSavePackageDialog(
                WindowHandle(),
                cfg::SafeFileName(packageName) + std::wstring{ engine::PackageFileSuffix },
                native::AppSettings::Current().LastPackageFolder());

            if (path.empty() || m_closing)
            {
                EndWork();
                co_return;
            }

            auto const environment = m_environment;
            engine::ShareResult shared{};

            co_await native::RunOnBackgroundAsync([&shared, &environment, &fileName, parts, &packageName, &note, &path]()
                {
                    shared = engine::ShareConfiguration(environment, fileName, parts, packageName, note, path);
                });

            if (m_closing)
            {
                co_return;
            }

            if (shared.Succeeded())
            {
                native::AppSettings::Current().LastPackageFolder(cfg::ParentFolder(path));

                m_savedPackagePath = path;

                auto const separator = path.find_last_of(L"\\/");

                ShareStatusBar().IsOpen(false);
                ShareSavedBar().IsOpen(false);
                ShareSavedBar().Title(res::FormatString(L"CfgSavedTitleFormat", separator == std::wstring::npos ? path : path.substr(separator + 1)));
                ShareSavedBar().Message(shared.PicturesMissing > 0 ?
                    res::FormatString(L"CfgTwoSentencesFormat", res::GetString(L"CfgSavedMessage"), cfg::CountText(L"CfgSavedPicturesMissing", shared.PicturesMissing)) :
                    res::GetString(L"CfgSavedMessage"));
                ShareSavedBar().Severity(shared.PicturesMissing > 0 ? controls::InfoBarSeverity::Warning : controls::InfoBarSeverity::Success);
                ShareSavedBar().IsOpen(true);
            }
            else
            {
                ShareSavedBar().IsOpen(false);
                ShowFailure(ShareStatusBar(), res::GetString(L"CfgShareFailedTitle"), shared);
            }

            EndWork();
        }
        catch (...)
        {
            EndWork();
            MIDI_SETTINGS_LOG_GENERAL_EXCEPTION(L"Unable to save a package.");
        }
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnShowInFolderClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        cfg::ShowInFolder(m_savedPackagePath);
    }
}
