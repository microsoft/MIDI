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

// What will happen: every part of a package, with a choice, and a plain line for every item in
// it, before anything changes.

#include "pch.h"
#include "ConfigurationsWindow.xaml.h"

#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace cfg = ::midisettings::configurations;
namespace engine = ::midisettings::configengine;
namespace automation = ::winrt::Microsoft::UI::Xaml::Automation;
namespace imaging = ::winrt::Microsoft::UI::Xaml::Media::Imaging;
namespace streams = ::winrt::Windows::Storage::Streams;

namespace winrt::midisettings::implementation
{
    namespace
    {
        constexpr wchar_t GlyphDone{ 0xE73E };
        constexpr wchar_t GlyphWarning{ 0xE7BA };
        constexpr wchar_t GlyphLoses{ 0xE711 };
        constexpr wchar_t GlyphLeftOut{ 0xE738 };
        constexpr wchar_t GlyphExpanded{ 0xE70E };
        constexpr wchar_t GlyphCollapsed{ 0xE70D };

        // Rows a group shows before Show all.
        constexpr size_t RowsShownAtFirst{ 3 };

        constexpr double ChoiceWidth{ 168 };
        constexpr double PicturePixels{ 56 };

        // A note this long is someone's essay, not a note, and it's cut there.
        constexpr size_t MaximumNoteLength{ 400 };

        constexpr engine::ConfigPart PartOrder[]
        {
            engine::ConfigPart::DeviceNames,
            engine::ConfigPart::Loopbacks,
            engine::ConfigPart::NetworkMidi2,
            engine::ConfigPart::RtpMidi,
            engine::ConfigPart::Bluetooth,
            engine::ConfigPart::OtherSettings,
        };

        std::wstring_view PartKey(_In_ engine::ConfigPart const part) noexcept
        {
            switch (part)
            {
            case engine::ConfigPart::DeviceNames: return L"devices";
            case engine::ConfigPart::Loopbacks: return L"loopbacks";
            case engine::ConfigPart::NetworkMidi2: return L"network";
            case engine::ConfigPart::RtpMidi: return L"rtp";
            case engine::ConfigPart::Bluetooth: return L"bluetooth";
            default: return L"other";
            }
        }

        wchar_t PartGlyph(_In_ engine::ConfigPart const part) noexcept
        {
            switch (part)
            {
            case engine::ConfigPart::DeviceNames: return 0xE8EC;
            case engine::ConfigPart::Loopbacks: return 0xE8EE;
            case engine::ConfigPart::NetworkMidi2: return 0xE839;
            case engine::ConfigPart::RtpMidi: return 0xE71B;
            case engine::ConfigPart::Bluetooth: return 0xE702;
            default: return 0xE713;
            }
        }

        std::wstring_view PartTitleKey(_In_ engine::ConfigPart const part) noexcept
        {
            switch (part)
            {
            case engine::ConfigPart::DeviceNames: return L"CfgPartDeviceNames";
            case engine::ConfigPart::Loopbacks: return L"CfgPartLoopbacks";
            case engine::ConfigPart::NetworkMidi2: return L"CfgPartNetworkMidi2";
            case engine::ConfigPart::RtpMidi: return L"CfgPartRtpMidi";
            case engine::ConfigPart::Bluetooth: return L"CfgPartBluetooth";
            default: return L"CfgPartOtherSettings";
            }
        }

        engine::PartChoice PartChoiceFor(_In_ engine::ImportChoices const& choices, _In_ engine::ConfigPart const part) noexcept
        {
            switch (part)
            {
            case engine::ConfigPart::Loopbacks: return choices.Loopbacks;
            case engine::ConfigPart::NetworkMidi2: return choices.NetworkMidi2;
            case engine::ConfigPart::RtpMidi: return choices.RtpMidi;
            case engine::ConfigPart::Bluetooth: return choices.Bluetooth;
            default: return engine::PartChoice::Add;
            }
        }

        void SetPartChoice(_Inout_ engine::ImportChoices& choices, _In_ engine::ConfigPart const part, _In_ engine::PartChoice const choice) noexcept
        {
            switch (part)
            {
            case engine::ConfigPart::Loopbacks: choices.Loopbacks = choice; break;
            case engine::ConfigPart::NetworkMidi2: choices.NetworkMidi2 = choice; break;
            case engine::ConfigPart::RtpMidi: choices.RtpMidi = choice; break;
            case engine::ConfigPart::Bluetooth: choices.Bluetooth = choice; break;
            default: break;
            }
        }

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

        winrt::hstring PortNamingText(_In_ engine::PortNaming const naming)
        {
            switch (naming)
            {
            case engine::PortNaming::ClassicCompatible: return res::GetString(L"CfgPortNamingClassic");
            case engine::PortNaming::NewStyle: return res::GetString(L"CfgPortNamingNewStyle");
            case engine::PortNaming::Automatic: return res::GetString(L"CfgPortNamingAutomatic");
            default: return res::GetString(L"CfgNoValue");
            }
        }

        bool Contains(_In_ std::vector<std::wstring> const& keys, _In_ std::wstring_view const key)
        {
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        }

        winrt::hstring Joined(_In_ winrt::hstring const& first, _In_ winrt::hstring const& second)
        {
            if (first.empty())
            {
                return second;
            }

            if (second.empty())
            {
                return first;
            }

            return res::FormatString(L"CfgDotSeparatedFormat", first, second);
        }

        // Pictures from a package are bytes, not files, so they're decoded from memory.
        winrt::fire_and_forget LoadPictureBytes(controls::Image image, std::vector<uint8_t> bytes, bool svg) noexcept
        {
            try
            {
                streams::InMemoryRandomAccessStream stream{};
                streams::DataWriter writer{ stream };

                writer.WriteBytes(bytes);
                co_await writer.StoreAsync();
                writer.DetachStream();

                stream.Seek(0);

                if (svg)
                {
                    imaging::SvgImageSource source{};
                    source.RasterizePixelHeight(PicturePixels);

                    co_await source.SetSourceAsync(stream);

                    image.Source(source);
                }
                else
                {
                    imaging::BitmapImage bitmap{};
                    bitmap.DecodePixelHeight(static_cast<int32_t>(PicturePixels));

                    co_await bitmap.SetSourceAsync(stream);

                    image.Source(bitmap);
                }
            }
            catch (...)
            {
                // a picture that won't decode is left blank
            }
        }

        struct PartCounts
        {
            size_t Total{ 0 };
            size_t Added{ 0 };
            size_t Here{ 0 };
            size_t Taken{ 0 };
            size_t Hosts{ 0 };
            size_t Clients{ 0 };
        };

        PartCounts CountsFor(
            _In_ std::vector<engine::PlanItem const*> const& items,
            _In_ engine::ImportChoices const& choices)
        {
            PartCounts counts{};

            for (auto const* item : items)
            {
                if (item->Status == engine::ItemStatus::OnlyHere)
                {
                    continue;
                }

                counts.Total++;

                if (item->Kind == engine::ItemKind::Host)
                {
                    counts.Hosts++;
                }
                else if (item->Kind == engine::ItemKind::Client)
                {
                    counts.Clients++;
                }

                switch (engine::OutcomeFor(*item, choices))
                {
                case engine::ItemOutcome::WillBeAdded:
                case engine::ItemOutcome::AddedUnderNewName:
                case engine::ItemOutcome::TakesThisPcName:
                case engine::ItemOutcome::KeepsOtherPcName:
                case engine::ItemOutcome::GetsThePackagesName:
                    counts.Added++;
                    break;

                case engine::ItemOutcome::StaysAsItIs:
                case engine::ItemOutcome::KeepsYourName:
                    counts.Here++;
                    break;

                case engine::ItemOutcome::LeftOutNameTaken:
                    counts.Taken++;
                    break;

                default:
                    break;
                }
            }

            return counts;
        }
    }

    void ConfigurationsWindow::RenderImportReview() noexcept
    {
        try
        {
            auto const& plan = m_importPlan;

            ImportSourceNameText().Text(ImportPackageName());

            winrt::hstring chip{};

            if (plan.IsPlainConfiguration)
            {
                chip = res::GetString(L"CfgPlainFileChip");
            }
            else if (!plan.Source.ComputerName.empty())
            {
                chip = res::FormatString(L"CfgFromPcFormat", plan.Source.ComputerName);
            }

            ImportSourceChipText().Text(chip);
            ImportSourceChip().Visibility(chip.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            winrt::hstring detail{};

            if (plan.IsPlainConfiguration)
            {
                detail = res::GetString(L"CfgPlainFileDetail");
            }
            else
            {
                if (plan.Source.HasCreated)
                {
                    detail = res::FormatString(
                        plan.Source.Kind == engine::PackageKind::Backup ? L"CfgBackedUpOnFormat" : L"CfgSharedOnFormat",
                        cfg::DateText(plan.Source.Created, L"CfgDatePictureFull"));
                }

                if (!plan.Source.Note.empty())
                {
                    auto note = plan.Source.Note;

                    if (note.size() > MaximumNoteLength)
                    {
                        note.resize(MaximumNoteLength);
                        note += L'\x2026';
                    }

                    detail = Joined(detail, res::FormatString(L"CfgQuotedFormat", note));
                }
            }

            ImportSourceDetailText().Text(detail);
            ImportSourceDetailText().Visibility(detail.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            ImportElevationBar().IsOpen(!m_elevated);

            RenderImportTarget();
            RenderImportParts();
            RenderImportFooter();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show what an import will do.")
    }

    void ConfigurationsWindow::RenderImportTarget() noexcept
    {
        try
        {
            auto const& plan = m_importPlan;
            auto const intoNew = m_importChoices.Target == engine::ImportTarget::NewConfiguration;

            auto const inUseTitle = plan.HasTarget ?
                res::FormatString(L"CfgIntoInUseTitleFormat", plan.TargetConfigName) :
                res::GetString(L"CfgIntoInUseNoneTitle");

            auto const inUseDetail = !plan.HasTarget ? res::GetString(L"CfgIntoInUseNoneDetail") :
                m_importChoices.DeviceNames == engine::DeviceNamesChoice::ReplaceMine ? res::GetString(L"CfgIntoInUseReplaceDetail") :
                res::GetString(L"CfgIntoInUseDetail");

            auto const newDetail = res::FormatString(L"CfgIntoNewDetailFormat", plan.NewConfigurationName);

            ImportIntoInUseTitle().Text(inUseTitle);
            ImportIntoInUseDetail().Text(inUseDetail);
            ImportIntoNewDetail().Text(newDetail);

            automation::AutomationProperties::SetName(ImportIntoInUseRadio(), res::FormatString(L"CfgTwoSentencesFormat", inUseTitle, inUseDetail));
            automation::AutomationProperties::SetName(ImportIntoNewRadio(), res::FormatString(L"CfgTwoSentencesFormat", res::GetString(L"CfgIntoNewTitle"), newDetail));

            ImportIntoInUseRadio().IsEnabled(plan.HasTarget);
            ImportIntoInUseRadio().IsChecked(!intoNew);
            ImportIntoNewRadio().IsChecked(intoNew);

            ImportIntoInUseCard().Style(StyleNamed(intoNew ? L"CfgChoiceCardStyle" : L"CfgChoiceCardSelectedStyle"));
            ImportIntoNewCard().Style(StyleNamed(intoNew ? L"CfgChoiceCardSelectedStyle" : L"CfgChoiceCardStyle"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show where an import goes.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::OnImportTargetChanged(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const intoNew = sender == ImportIntoNewRadio();
            auto const target = intoNew ? engine::ImportTarget::NewConfiguration : engine::ImportTarget::InUse;

            if (target == m_importChoices.Target || (!intoNew && !m_importPlan.HasTarget))
            {
                return;
            }

            m_importChoices.Target = target;

            ReRenderImportPartsLater({});
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to change where an import goes.")
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::ReRenderImportPartsLater(std::wstring focusKey) noexcept
    {
        try
        {
            m_importFocusKey = std::move(focusKey);

            if (m_renderImportPending || m_dispatcherQueue == nullptr)
            {
                return;
            }

            m_renderImportPending = true;

            // Not from inside the handler of a control the redraw is about to replace.
            auto const queued = m_dispatcherQueue.TryEnqueue(
                winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                [weak = get_weak()]()
                {
                    if (auto strong = weak.get(); strong != nullptr && !strong->m_closing)
                    {
                        strong->m_renderImportPending = false;

                        if (strong->m_importPanel == ImportPanel::Review)
                        {
                            strong->RenderImportTarget();
                            strong->RenderImportParts();
                            strong->RenderImportFooter();
                        }
                    }
                });

            if (!queued)
            {
                m_renderImportPending = false;
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to redraw what an import will do.")
    }

    void ConfigurationsWindow::RenderImportParts() noexcept
    {
        try
        {
            m_importFocusTargets.clear();

            auto const panel = ImportPartsPanel();
            panel.Children().Clear();

            auto const& plan = m_importPlan;

            for (auto const part : PartOrder)
            {
                std::vector<engine::PlanItem const*> items{};

                for (auto const& item : plan.Items)
                {
                    if (item.Part == part)
                    {
                        items.push_back(&item);
                    }
                }

                // A package with no device names can only take this PC's away, so there's no choice.
                if (part == engine::ConfigPart::DeviceNames &&
                    std::none_of(items.begin(), items.end(), [](auto const* item) { return item->Status != engine::ItemStatus::OnlyHere; }))
                {
                    continue;
                }

                auto const otherSettings = part == engine::ConfigPart::OtherSettings &&
                    (plan.PackageHasSynthMode || plan.PackagePortNaming != engine::PortNaming::None);

                if (items.empty() && !otherSettings)
                {
                    continue;
                }

                panel.Children().Append(MakeImportPart(part, items));
            }

            if (panel.Children().Size() == 0)
            {
                panel.Children().Append(MakeText(res::GetString(L"CfgNothingToImport"), L"CfgCaptionStyle"));
            }

            m_importFocusKey.clear();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the parts of a package.")
    }

    _Use_decl_annotations_
    xaml::UIElement ConfigurationsWindow::MakeImportPart(
        engine::ConfigPart const part,
        std::vector<engine::PlanItem const*> const& items)
    {
        auto const& plan = m_importPlan;
        auto const& choices = m_importChoices;

        auto const key = std::wstring{ PartKey(part) };
        auto const title = res::GetString(PartTitleKey(part));
        auto const expanded = Contains(m_expandedImportParts, key);
        auto const intoNew = choices.Target == engine::ImportTarget::NewConfiguration;
        auto const counts = CountsFor(items, choices);

        // ----- the line under the title
        winrt::hstring amounts{};
        winrt::hstring status{};
        auto caution = false;

        auto const notImported = res::GetString(L"CfgPartWontBeImported");

        switch (part)
        {
        case engine::ConfigPart::DeviceNames:
        {
            std::vector<winrt::hstring> parts{ cfg::CountText(L"CfgCountDeviceNames", counts.Total) };

            if (plan.PackageDevicePictureCount > 0)
            {
                parts.push_back(cfg::CountText(L"CfgCountPictures", plan.PackageDevicePictureCount));
            }

            amounts = cfg::ListText(parts);

            status = choices.DeviceNames == engine::DeviceNamesChoice::DontImport ? notImported :
                intoNew ? res::GetString(L"CfgPartAllAdded") :
                choices.DeviceNames == engine::DeviceNamesChoice::ReplaceMine ? res::GetString(L"CfgPartYoursReplaced") :
                choices.SharedDevices == engine::SharedDevicesChoice::UseThePackages ? res::GetString(L"CfgPartPackagesNamesUsed") :
                res::GetString(L"CfgPartYoursKept");
            break;
        }

        case engine::ConfigPart::OtherSettings:
        {
            auto const portNames = plan.PackagePortNaming != engine::PortNaming::None;

            amounts = plan.PackageHasSynthMode && portNames ? res::GetString(L"CfgOtherSynthAndPortNames") :
                plan.PackageHasSynthMode ? res::GetString(L"CfgOtherSynthMode") :
                portNames ? res::GetString(L"CfgOtherPortNames") :
                cfg::CountText(L"CfgShareOtherTransportsFirst", items.size());

            if (choices.OtherSettings == engine::OtherSettingsChoice::KeepMine)
            {
                status = res::GetString(L"CfgPartYoursKept");
            }
            else
            {
                std::vector<winrt::hstring> changes{};

                if (engine::SynthModeWillChange(plan, choices))
                {
                    changes.push_back(res::FormatString(L"CfgSynthModeWillBeFormat", cfg::SynthModeText(plan.PackageSynthMode)));
                }

                if (portNames && !plan.PackagePortNamingUsable)
                {
                    changes.push_back(res::GetString(L"CfgPortNamesNotUsableShort"));
                }
                else if (engine::PortNamingWillChange(plan, choices))
                {
                    changes.push_back(res::GetString(L"CfgPortNamesWillChange"));
                    caution = true;
                }

                status = changes.empty() ? res::GetString(L"CfgPartSameAsYours") :
                    changes.size() == 1 ? changes.front() :
                    Joined(changes[0], changes[1]);

                // The changes name the settings themselves, so the list of what's in the part goes.
                if (!changes.empty())
                {
                    amounts = {};
                }
            }

            break;
        }

        default:
        {
            switch (part)
            {
            case engine::ConfigPart::Loopbacks:
                amounts = cfg::CountText(L"CfgCountLoopbacks", counts.Total);
                break;

            case engine::ConfigPart::Bluetooth:
                amounts = cfg::CountText(L"CfgCountDevices", counts.Total);
                break;

            default:
                amounts = HostsAndConnections(counts.Hosts, counts.Clients);
                break;
            }

            if (PartChoiceFor(choices, part) == engine::PartChoice::DontImport)
            {
                status = notImported;
            }
            else if (counts.Taken > 0)
            {
                status = cfg::CountText(L"CfgPartNamesTaken", counts.Taken);
            }
            else if (part == engine::ConfigPart::RtpMidi && !plan.RtpInstalled)
            {
                status = res::GetString(L"CfgPartRtpSaved");
            }
            else
            {
                std::vector<winrt::hstring> parts{};

                if (counts.Added > 0)
                {
                    parts.push_back(cfg::CountText(L"CfgPartWillBeAdded", counts.Added));
                }

                if (counts.Here > 0)
                {
                    parts.push_back(cfg::CountText(L"CfgPartAlreadyHere", counts.Here));
                }

                status = cfg::ListText(parts);
            }

            break;
        }
        }

        // ----- the head: icon, title and summary, choice, open and close
        controls::Border card{};
        card.Style(StyleNamed(L"CfgPartCardStyle"));

        controls::StackPanel stack{};

        controls::Grid head{};
        head.Padding(xaml::Thickness{ 16, 10, 8, 10 });
        head.MinHeight(64);
        head.ColumnSpacing(14);

        for (auto const width : {
            xaml::GridLengthHelper::Auto(),
            xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star),
            xaml::GridLengthHelper::Auto(),
            xaml::GridLengthHelper::Auto() })
        {
            controls::ColumnDefinition column{};
            column.Width(width);
            head.ColumnDefinitions().Append(column);
        }

        head.Children().Append(MakeGlyph(PartGlyph(part), L"CfgRowGlyphStyle"));

        controls::StackPanel text{};
        text.Spacing(2);
        text.VerticalAlignment(xaml::VerticalAlignment::Center);
        controls::Grid::SetColumn(text, 1);
        text.Children().Append(MakeText(title, L"CfgRowTitleStyle"));
        text.Children().Append(MakeText(Joined(amounts, status), caution ? L"CfgPartSubtitleCautionStyle" : L"CfgPartSubtitleStyle"));
        head.Children().Append(text);

        std::vector<winrt::hstring> options{};
        int32_t selected{ 0 };
        std::function<void(int32_t)> onChanged{};

        if (part == engine::ConfigPart::DeviceNames)
        {
            if (intoNew)
            {
                options = { res::GetString(L"CfgChoiceImport"), res::GetString(L"CfgChoiceDontImport") };
                selected = choices.DeviceNames == engine::DeviceNamesChoice::DontImport ? 1 : 0;
                onChanged = [this](int32_t const index)
                    {
                        m_importChoices.DeviceNames = index == 1 ? engine::DeviceNamesChoice::DontImport : engine::DeviceNamesChoice::AddToMine;
                    };
            }
            else
            {
                options = { res::GetString(L"CfgChoiceAddToMine"), res::GetString(L"CfgChoiceReplaceMine"), res::GetString(L"CfgChoiceDontImport") };
                selected = choices.DeviceNames == engine::DeviceNamesChoice::ReplaceMine ? 1 :
                    choices.DeviceNames == engine::DeviceNamesChoice::DontImport ? 2 : 0;
                onChanged = [this](int32_t const index)
                    {
                        m_importChoices.DeviceNames = index == 1 ? engine::DeviceNamesChoice::ReplaceMine :
                            index == 2 ? engine::DeviceNamesChoice::DontImport :
                            engine::DeviceNamesChoice::AddToMine;
                    };
            }
        }
        else if (part == engine::ConfigPart::OtherSettings)
        {
            options = { res::GetString(L"CfgChoiceKeepMine"), res::GetString(L"CfgChoiceUsePackages") };
            selected = choices.OtherSettings == engine::OtherSettingsChoice::UseThePackages ? 1 : 0;
            onChanged = [this](int32_t const index)
                {
                    m_importChoices.OtherSettings = index == 1 ? engine::OtherSettingsChoice::UseThePackages : engine::OtherSettingsChoice::KeepMine;
                };
        }
        else
        {
            options = { res::GetString(intoNew ? L"CfgChoiceImport" : L"CfgChoiceAddToMine"), res::GetString(L"CfgChoiceDontImport") };
            selected = PartChoiceFor(choices, part) == engine::PartChoice::DontImport ? 1 : 0;
            onChanged = [this, part](int32_t const index)
                {
                    SetPartChoice(m_importChoices, part, index == 1 ? engine::PartChoice::DontImport : engine::PartChoice::Add);
                };
        }

        auto combo = MakePartCombo(key + L":choice", options, selected, res::FormatString(L"CfgPartChoiceNameFormat", title), onChanged);
        controls::Grid::SetColumn(combo, 2);
        head.Children().Append(combo);

        controls::Button toggle{};
        toggle.Style(StyleNamed(L"CfgSubtleIconButtonStyle"));
        toggle.VerticalAlignment(xaml::VerticalAlignment::Center);

        controls::FontIcon chevron{};
        chevron.Glyph(winrt::hstring{ std::wstring(1, expanded ? GlyphExpanded : GlyphCollapsed) });
        chevron.FontSize(12);
        toggle.Content(chevron);

        auto const toggleName = res::FormatString(expanded ? L"CfgHideDetailsFormat" : L"CfgShowDetailsFormat", title);
        automation::AutomationProperties::SetName(toggle, toggleName);
        controls::ToolTipService::SetToolTip(toggle, winrt::box_value(toggleName));
        controls::Grid::SetColumn(toggle, 3);

        toggle.Click([weak = get_weak(), key](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    auto& open = strong->m_expandedImportParts;
                    auto const found = std::find(open.begin(), open.end(), key);

                    if (found == open.end())
                    {
                        open.push_back(key);
                    }
                    else
                    {
                        open.erase(found);
                    }

                    strong->ReRenderImportPartsLater(key + L":toggle");
                }
            });

        RememberFocusTarget(key + L":toggle", toggle);
        head.Children().Append(toggle);

        stack.Children().Append(head);

        // ----- every item, with what will happen to it
        if (expanded)
        {
            controls::Border body{};
            body.Style(StyleNamed(L"CfgPartBodyStyle"));

            controls::StackPanel lines{};
            lines.Spacing(2);

            if (part == engine::ConfigPart::DeviceNames)
            {
                AddDeviceNameItems(lines, items);
            }
            else if (part == engine::ConfigPart::OtherSettings)
            {
                AddOtherSettingsItems(lines, items);
            }
            else
            {
                AddEntryItems(lines, items);
            }

            body.Child(lines);
            stack.Children().Append(body);
        }

        card.Child(stack);

        return card;
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::AddDeviceNameItems(controls::StackPanel const& body, std::vector<engine::PlanItem const*> const& items)
    {
        auto const& choices = m_importChoices;
        auto const intoNew = choices.Target == engine::ImportTarget::NewConfiguration;

        if (choices.DeviceNames == engine::DeviceNamesChoice::DontImport)
        {
            body.Children().Append(MakeImportNote(res::GetString(L"CfgPartNotImportedNote")));
            return;
        }

        std::vector<engine::PlanItem const*> inBoth{};
        std::vector<engine::PlanItem const*> alreadyHere{};
        std::vector<engine::PlanItem const*> onlyHere{};
        std::vector<engine::PlanItem const*> fresh{};

        for (auto const* item : items)
        {
            switch (item->Status)
            {
            case engine::ItemStatus::OnlyHere:
                if (!intoNew)
                {
                    onlyHere.push_back(item);
                }
                break;

            case engine::ItemStatus::InBoth:
                (intoNew ? fresh : inBoth).push_back(item);
                break;

            case engine::ItemStatus::AlreadyHere:
                (intoNew ? fresh : alreadyHere).push_back(item);
                break;

            default:
                fresh.push_back(item);
                break;
            }
        }

        auto const addDevice = [this, &body, &choices](engine::PlanItem const& item)
            {
                auto const outcome = engine::OutcomeFor(item, choices);

                wchar_t glyph{ GlyphDone };
                std::wstring_view tone{ L"CfgDoneGlyphStyle" };
                winrt::hstring what{};
                std::wstring packagePicture{};
                std::wstring herePicture{};

                // An entry can hold a picture, port names or other settings without a name, so
                // each line falls back to what the entry really has.
                switch (outcome)
                {
                case engine::ItemOutcome::KeepsYourName:
                    tone = L"CfgSameGlyphStyle";
                    what = !item.YourName.empty() ? res::FormatString(L"CfgWillKeepYourNameFormat", item.YourName) :
                        res::GetString(item.YourImage.empty() ? L"CfgWillKeepYourSettings" : L"CfgWillKeepYourPicture");
                    herePicture = item.YourImage;
                    break;

                case engine::ItemOutcome::GetsThePackagesName:
                    if (item.Name.empty())
                    {
                        what = res::GetString(item.Image.empty() ? L"CfgWillGetPackagesSettings" : L"CfgWillGetPackagesPicture");
                    }
                    else
                    {
                        what = item.YourName.empty() ?
                            res::FormatString(L"CfgWillBeCalledFormat", item.Name) :
                            res::FormatString(L"CfgWillBeCalledInsteadFormat", item.Name, item.YourName);
                    }
                    packagePicture = item.Image;
                    break;

                case engine::ItemOutcome::LosesYourName:
                    glyph = GlyphLoses;
                    tone = L"CfgWarningGlyphStyle";
                    if (item.YourName.empty())
                    {
                        what = res::GetString(item.YourImage.empty() ? L"CfgWillLoseYourSettings" : L"CfgWillLoseYourPicture");
                    }
                    else
                    {
                        what = res::FormatString(item.YourImage.empty() ? L"CfgWillLoseYourNameFormat" : L"CfgWillLoseYourNameAndPictureFormat", item.YourName);
                    }
                    herePicture = item.YourImage;
                    break;

                case engine::ItemOutcome::StaysAsItIs:
                    tone = L"CfgSameGlyphStyle";
                    what = res::GetString(L"CfgAlreadyHereStays");
                    herePicture = item.YourImage.empty() ? item.Image : item.YourImage;
                    break;

                case engine::ItemOutcome::NotImported:
                    glyph = GlyphLeftOut;
                    tone = L"CfgSameGlyphStyle";
                    what = res::GetString(L"CfgWontBeImported");
                    break;

                default:
                    what = item.Name.empty() ? res::GetString(L"CfgWillBeAdded") : res::FormatString(L"CfgWillBeCalledFormat", item.Name);
                    packagePicture = item.Image;
                    break;
                }

                xaml::UIElement state{ nullptr };

                // Without the service, whether a device is here can't be told, so it isn't said.
                if (!m_liveEndpoints.empty())
                {
                    state = MakeText(res::GetString(IsPluggedIn(item) ? L"CfgPluggedIn" : L"CfgNotPluggedIn"), L"CfgItemStateStyle");
                }

                body.Children().Append(MakeImportItem(glyph, tone, MakeImportPicture(packagePicture, herePicture), DeviceDisplayName(item), what, state));
            };

        auto const addGroup = [this, &body, &addDevice](std::wstring const& groupKey, winrt::hstring const& heading, xaml::UIElement const& right, std::vector<engine::PlanItem const*> const& group)
            {
                if (group.empty())
                {
                    return;
                }

                body.Children().Append(MakeImportGroupRow(heading, right));

                auto const showAll = Contains(m_showAllImportGroups, groupKey);

                for (size_t index = 0; index < group.size() && (showAll || index < RowsShownAtFirst); ++index)
                {
                    addDevice(*group[index]);
                }

                if (!showAll && group.size() > RowsShownAtFirst)
                {
                    body.Children().Append(MakeShowAllLink(groupKey, group.size()));
                }
            };

        if (!intoNew && choices.DeviceNames == engine::DeviceNamesChoice::ReplaceMine)
        {
            addGroup(L"devices:lose", res::GetString(L"CfgGroupLoseNames"), nullptr, onlyHere);
            addGroup(L"devices:replaced", res::GetString(L"CfgGroupGetPackagesNames"), nullptr, inBoth);
        }
        else if (!intoNew && !inBoth.empty())
        {
            // the choice for devices both PCs have names for sits with them
            auto shared = MakePartCombo(
                L"devices:shared",
                { res::GetString(L"CfgChoiceKeepMine"), res::GetString(L"CfgChoiceUsePackages") },
                choices.SharedDevices == engine::SharedDevicesChoice::UseThePackages ? 1 : 0,
                res::GetString(L"CfgSharedDevicesChoiceName"),
                [this](int32_t const index)
                {
                    m_importChoices.SharedDevices = index == 1 ? engine::SharedDevicesChoice::UseThePackages : engine::SharedDevicesChoice::KeepMine;
                });

            addGroup(L"devices:shared", cfg::CountText(L"CfgGroupHaveYourNames", inBoth.size()), shared, inBoth);
        }

        addGroup(L"devices:here", cfg::CountText(L"CfgGroupAlreadyHere", alreadyHere.size()), nullptr, alreadyHere);
        addGroup(L"devices:new", cfg::CountText(L"CfgGroupNewNames", fresh.size()), nullptr, fresh);

        body.Children().Append(MakeImportNote(res::GetString(L"CfgDevicesNote")));
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::AddEntryItems(controls::StackPanel const& body, std::vector<engine::PlanItem const*> const& items)
    {
        if (items.empty())
        {
            return;
        }

        auto const& plan = m_importPlan;
        auto const& choices = m_importChoices;
        auto const part = items.front()->Part;

        if (PartChoiceFor(choices, part) == engine::PartChoice::DontImport)
        {
            body.Children().Append(MakeImportNote(res::GetString(L"CfgPartNotImportedNote")));
            return;
        }

        for (auto const* entry : items)
        {
            auto const& item = *entry;
            auto const outcome = engine::OutcomeFor(item, choices);
            auto const id = item.Id;

            winrt::hstring name{};
            winrt::hstring takenName{};

            switch (item.Kind)
            {
            case engine::ItemKind::Loopback:
                if (!item.BaseName.empty() && !engine::SameText(item.SectionKey, engine::transportids::BasicLoopback))
                {
                    name = res::FormatString(L"CfgLoopbackPairFormat", item.BaseName);
                    takenName = winrt::hstring{ item.BaseName };
                }
                else if (item.EndpointNames.size() >= 2)
                {
                    name = res::FormatString(L"CfgLoopbackEndsFormat", item.EndpointNames[0], item.EndpointNames[1]);
                    takenName = winrt::hstring{ item.EndpointNames[0] };
                }
                else
                {
                    name = winrt::hstring{ item.Name.empty() && !item.EndpointNames.empty() ? item.EndpointNames.front() : item.Name };
                    takenName = name;
                }
                break;

            case engine::ItemKind::Host:
                takenName = item.Name.empty() ? res::GetString(L"CfgThisPcsName") : winrt::hstring{ item.Name };
                name = res::FormatString(L"CfgHostItemFormat", takenName);
                break;

            case engine::ItemKind::Client:
                name = res::FormatString(L"CfgClientItemFormat", item.Name);
                break;

            default:
                name = winrt::hstring{ item.Name.empty() ? item.EntryKey : item.Name };
                break;
            }

            wchar_t glyph{ GlyphDone };
            std::wstring_view tone{ L"CfgDoneGlyphStyle" };
            winrt::hstring what{};
            xaml::UIElement right{ nullptr };

            auto const freeName = winrt::hstring{ item.FreeName };

            switch (outcome)
            {
            case engine::ItemOutcome::StaysAsItIs:
                tone = L"CfgSameGlyphStyle";
                what = res::GetString(L"CfgAlreadyHereStays");
                break;

            case engine::ItemOutcome::LeftOutNameTaken:
                glyph = GlyphWarning;
                tone = L"CfgWarningGlyphStyle";
                what = res::FormatString(item.Kind == engine::ItemKind::Loopback ? L"CfgLoopbackTakenFormat" : L"CfgHostTakenFormat", takenName);

                if (!freeName.empty())
                {
                    right = MakeItemCheck(id + L":free", res::FormatString(L"CfgAddAsFormat", freeName), false,
                        [this, id](bool const ticked) { m_importChoices.SetAddedUnderFreeName(id, ticked); });
                }
                break;

            case engine::ItemOutcome::AddedUnderNewName:
                what = res::FormatString(L"CfgWillBeAddedAsFormat", freeName);
                right = MakeItemCheck(id + L":free", res::FormatString(L"CfgAddAsFormat", freeName), true,
                    [this, id](bool const ticked) { m_importChoices.SetAddedUnderFreeName(id, ticked); });
                break;

            case engine::ItemOutcome::TakesThisPcName:
                what = res::FormatString(L"CfgTakesThisPcNameFormat", freeName.empty() ? winrt::hstring{ plan.ThisComputerName } : freeName);
                right = MakeItemCheck(id + L":thispc", res::GetString(L"CfgUseThisPcsName"), true,
                    [this, id](bool const ticked) { m_importChoices.SetUseThisPcName(id, ticked); });
                break;

            case engine::ItemOutcome::KeepsOtherPcName:
                glyph = GlyphWarning;
                tone = L"CfgWarningGlyphStyle";
                what = res::FormatString(L"CfgKeepsOtherPcNameFormat", takenName);
                right = MakeItemCheck(id + L":thispc", res::GetString(L"CfgUseThisPcsName"), false,
                    [this, id](bool const ticked) { m_importChoices.SetUseThisPcName(id, ticked); });
                break;

            case engine::ItemOutcome::NotImported:
                glyph = GlyphLeftOut;
                tone = L"CfgSameGlyphStyle";
                what = res::GetString(L"CfgWontBeImported");
                break;

            default:
                what = res::GetString(L"CfgWillBeAdded");
                break;
            }

            body.Children().Append(MakeImportItem(glyph, tone, nullptr, name, what, right));

            // the adapter it was set to is on the other PC
            if (item.Kind == engine::ItemKind::Host && item.UsesOtherAdapter && engine::IsImported(outcome))
            {
                body.Children().Append(MakeImportNote(res::GetString(L"CfgUsesAnyAdapter")));
            }
        }

        if (part == engine::ConfigPart::RtpMidi && !plan.RtpInstalled)
        {
            body.Children().Append(MakeImportNote(res::GetString(L"CfgRtpNotInstalledNote")));
        }
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::AddOtherSettingsItems(controls::StackPanel const& body, std::vector<engine::PlanItem const*> const& items)
    {
        auto const& plan = m_importPlan;
        auto const& choices = m_importChoices;
        auto const keepMine = choices.OtherSettings == engine::OtherSettingsChoice::KeepMine;

        if (plan.PackageHasSynthMode)
        {
            auto const changes = engine::SynthModeWillChange(plan, choices);

            auto const what = changes ?
                res::FormatString(L"CfgWillBeInsteadFormat", cfg::SynthModeText(plan.PackageSynthMode), cfg::SynthModeText(plan.HereSynthMode)) :
                keepMine ? res::FormatString(L"CfgStaysFormat", cfg::SynthModeText(plan.HereSynthMode)) :
                res::GetString(L"CfgPartSameAsYours");

            body.Children().Append(MakeImportItem(GlyphDone, changes ? L"CfgDoneGlyphStyle" : L"CfgSameGlyphStyle", nullptr, res::GetString(L"CfgOtherSynthMode"), what, nullptr));
        }

        if (plan.PackagePortNaming != engine::PortNaming::None)
        {
            wchar_t glyph{ GlyphDone };
            std::wstring_view tone{ L"CfgSameGlyphStyle" };
            winrt::hstring what{};

            if (!keepMine && !plan.PackagePortNamingUsable)
            {
                glyph = GlyphWarning;
                tone = L"CfgWarningGlyphStyle";
                what = res::GetString(L"CfgPortNamesNotUsable");
            }
            else if (engine::PortNamingWillChange(plan, choices))
            {
                glyph = GlyphWarning;
                tone = L"CfgWarningGlyphStyle";
                what = res::FormatString(L"CfgPortNamesWillChangeFormat", PortNamingText(plan.PackagePortNaming));
            }
            else
            {
                what = keepMine ?
                    res::FormatString(L"CfgStaysFormat", PortNamingText(plan.HerePortNaming)) :
                    res::GetString(L"CfgPartSameAsYours");
            }

            body.Children().Append(MakeImportItem(glyph, tone, nullptr, res::GetString(L"CfgOtherPortNames"), what, nullptr));
        }

        for (auto const* item : items)
        {
            if (item->Kind != engine::ItemKind::OtherSection)
            {
                continue;
            }

            auto const outcome = engine::OutcomeFor(*item, choices);
            auto const stays = outcome == engine::ItemOutcome::StaysAsItIs;

            body.Children().Append(MakeImportItem(
                GlyphDone,
                stays ? L"CfgSameGlyphStyle" : L"CfgDoneGlyphStyle",
                nullptr,
                res::FormatString(L"CfgOtherSectionItemFormat", item->SectionKey),
                res::GetString(stays ? L"CfgAlreadyHereStays" : L"CfgWillBeAdded"),
                nullptr));
        }
    }

    void ConfigurationsWindow::RenderImportFooter() noexcept
    {
        try
        {
            auto const& plan = m_importPlan;
            auto const& choices = m_importChoices;
            auto const portNames = engine::PortNamingWillChange(plan, choices);

            ImportFooterText().Text(choices.Target == engine::ImportTarget::InUse ?
                res::FormatString(portNames ? L"CfgFooterIntoPortsFormat" : L"CfgFooterIntoFormat", plan.TargetConfigName) :
                res::GetString(portNames ? L"CfgFooterNewPorts" : L"CfgFooterNew"));

            // Import is only offered when it would change something.
            m_importHasWork = portNames || engine::SynthModeWillChange(plan, choices) ||
                std::any_of(plan.Items.begin(), plan.Items.end(), [&choices](auto const& item)
                    {
                        auto const outcome = engine::OutcomeFor(item, choices);

                        return engine::IsImported(outcome) || outcome == engine::ItemOutcome::LosesYourName;
                    });

            ApplyWorkState();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show what Import will do.")
    }

    _Use_decl_annotations_
    xaml::FrameworkElement ConfigurationsWindow::MakeImportItem(
        wchar_t const glyph,
        std::wstring_view const toneKey,
        xaml::UIElement const& picture,
        winrt::hstring const& name,
        winrt::hstring const& what,
        xaml::UIElement const& right)
    {
        controls::Grid grid{};
        grid.ColumnSpacing(12);
        grid.MinHeight(36);

        int32_t column{ 0 };

        auto const addColumn = [&grid](xaml::GridLength const& width)
            {
                controls::ColumnDefinition definition{};
                definition.Width(width);
                grid.ColumnDefinitions().Append(definition);
            };

        auto const place = [&grid, &column](xaml::UIElement const& element)
            {
                controls::Grid::SetColumn(element.as<xaml::FrameworkElement>(), column++);
                grid.Children().Append(element);
            };

        addColumn(xaml::GridLengthHelper::Auto());
        place(MakeGlyph(glyph, toneKey));

        if (picture != nullptr)
        {
            addColumn(xaml::GridLengthHelper::Auto());
            place(picture);
        }

        addColumn(xaml::GridLengthHelper::Auto());

        auto nameText = MakeText(name, L"CfgItemNameStyle");
        nameText.MaxWidth(240);
        nameText.TextTrimming(xaml::TextTrimming::CharacterEllipsis);
        controls::ToolTipService::SetToolTip(nameText, winrt::box_value(name));
        place(nameText);

        addColumn(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
        place(MakeText(what, L"CfgItemWhatStyle"));

        if (right != nullptr)
        {
            addColumn(xaml::GridLengthHelper::Auto());

            auto const element = right.as<xaml::FrameworkElement>();
            element.VerticalAlignment(xaml::VerticalAlignment::Center);

            place(right);
        }

        return grid;
    }

    _Use_decl_annotations_
    xaml::FrameworkElement ConfigurationsWindow::MakeImportGroupRow(winrt::hstring const& text, xaml::UIElement const& right)
    {
        controls::Grid grid{};
        grid.MinHeight(30);
        grid.Padding(xaml::Thickness{ 28, 4, 0, 0 });
        grid.ColumnSpacing(12);

        controls::ColumnDefinition textColumn{};
        textColumn.Width(xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star));
        grid.ColumnDefinitions().Append(textColumn);

        controls::ColumnDefinition rightColumn{};
        rightColumn.Width(xaml::GridLengthHelper::Auto());
        grid.ColumnDefinitions().Append(rightColumn);

        grid.Children().Append(MakeText(text, L"CfgItemGroupStyle"));

        if (right != nullptr)
        {
            controls::Grid::SetColumn(right.as<xaml::FrameworkElement>(), 1);
            grid.Children().Append(right);
        }

        return grid;
    }

    _Use_decl_annotations_
    xaml::FrameworkElement ConfigurationsWindow::MakeImportNote(winrt::hstring const& text)
    {
        auto note = MakeText(text, L"CfgCaptionStyle");
        note.Margin(xaml::Thickness{ 28, 4, 0, 4 });

        return note;
    }

    _Use_decl_annotations_
    xaml::UIElement ConfigurationsWindow::MakeShowAllLink(std::wstring const& groupKey, size_t const count)
    {
        controls::HyperlinkButton link{};
        link.Content(winrt::box_value(res::FormatString(L"CfgShowAllFormat", count)));
        link.FontSize(12);
        link.Margin(xaml::Thickness{ 20, 0, 0, 0 });

        link.Click([weak = get_weak(), groupKey](auto&&, auto&&)
            {
                if (auto strong = weak.get())
                {
                    strong->m_showAllImportGroups.push_back(groupKey);

                    // The link goes away, so focus goes to the part's open and close button.
                    strong->ReRenderImportPartsLater(groupKey.substr(0, groupKey.find(L':')) + L":toggle");
                }
            });

        return link;
    }

    _Use_decl_annotations_
    controls::ComboBox ConfigurationsWindow::MakePartCombo(
        std::wstring const& focusKey,
        std::vector<winrt::hstring> const& options,
        int32_t const selected,
        winrt::hstring const& accessibleName,
        std::function<void(int32_t)> onChanged)
    {
        controls::ComboBox combo{};
        combo.Width(ChoiceWidth);
        combo.VerticalAlignment(xaml::VerticalAlignment::Center);

        for (auto const& option : options)
        {
            combo.Items().Append(winrt::box_value(option));
        }

        combo.SelectedIndex(selected);
        automation::AutomationProperties::SetName(combo, accessibleName);

        // attached after the selection is set, so drawing it doesn't count as a choice
        combo.SelectionChanged([weak = get_weak(), focusKey, onChanged](auto&& sender, auto&&)
            {
                if (auto strong = weak.get())
                {
                    auto const index = sender.template as<controls::ComboBox>().SelectedIndex();

                    if (index >= 0 && onChanged)
                    {
                        onChanged(index);
                        strong->ReRenderImportPartsLater(focusKey);
                    }
                }
            });

        RememberFocusTarget(focusKey, combo);

        return combo;
    }

    _Use_decl_annotations_
    controls::CheckBox ConfigurationsWindow::MakeItemCheck(
        std::wstring const& focusKey,
        winrt::hstring const& label,
        bool const checked,
        std::function<void(bool)> onChanged)
    {
        controls::CheckBox check{};
        check.Content(winrt::box_value(label));
        check.IsChecked(checked);
        check.FontSize(13);
        check.MinWidth(ChoiceWidth);

        check.Click([weak = get_weak(), focusKey, onChanged](auto&& sender, auto&&)
            {
                if (auto strong = weak.get())
                {
                    auto const value = sender.template as<controls::CheckBox>().IsChecked();

                    if (onChanged)
                    {
                        onChanged(value != nullptr && value.GetBoolean());
                    }

                    strong->ReRenderImportPartsLater(focusKey);
                }
            });

        RememberFocusTarget(focusKey, check);

        return check;
    }

    _Use_decl_annotations_
    void ConfigurationsWindow::RememberFocusTarget(std::wstring const& key, controls::Control const& control)
    {
        m_importFocusTargets.emplace_back(key, control);

        // The redraw replaced the control that had focus, so its replacement gets it once it's in
        // the tree.
        if (!key.empty() && key == m_importFocusKey)
        {
            control.Loaded([](auto&& sender, auto&&)
                {
                    try
                    {
                        sender.template as<controls::Control>().Focus(xaml::FocusState::Programmatic);
                    }
                    catch (...)
                    {
                    }
                });
        }
    }

    _Use_decl_annotations_
    xaml::UIElement ConfigurationsWindow::MakeImportPicture(std::wstring const& packagePicture, std::wstring const& herePicture)
    {
        controls::Border frame{};

        // A row without a picture keeps the space, so the names line up.
        if (packagePicture.empty() && herePicture.empty())
        {
            frame.Width(28);
            frame.Height(28);
            return frame;
        }

        frame.Style(StyleNamed(L"CfgPictureFrameStyle"));

        controls::Image image{};
        image.Stretch(media::Stretch::UniformToFill);
        frame.Child(image);

        if (!packagePicture.empty())
        {
            auto const* bytes = m_importContents.FindPicture(packagePicture);

            if (bytes != nullptr && !bytes->empty())
            {
                LoadPictureBytes(image, *bytes, midiapp::EndpointImageAssets::IsScalableVector(packagePicture));
            }

            return frame;
        }

        if (!midiapp::EndpointImageAssets::Exists(herePicture))
        {
            return frame;
        }

        auto const path = midiapp::EndpointImageAssets::FullPathForFileName(herePicture);

        if (path.empty())
        {
            return frame;
        }

        if (midiapp::EndpointImageAssets::IsScalableVector(herePicture))
        {
            imaging::SvgImageSource source{};
            source.RasterizePixelHeight(PicturePixels);
            source.UriSource(foundation::Uri{ L"file:///" + path });
            image.Source(source);
        }
        else
        {
            imaging::BitmapImage bitmap{};
            bitmap.DecodePixelHeight(static_cast<int32_t>(PicturePixels));
            bitmap.UriSource(foundation::Uri{ L"file:///" + path });
            image.Source(bitmap);
        }

        return frame;
    }

    _Use_decl_annotations_
    bool ConfigurationsWindow::IsPluggedIn(engine::PlanItem const& item) const noexcept
    {
        try
        {
            return std::any_of(m_liveEndpoints.begin(), m_liveEndpoints.end(), [&item](auto const& live)
                {
                    return (!item.EndpointDeviceId.empty() && engine::SameText(item.EndpointDeviceId, live.EndpointDeviceId)) ||
                        (!item.DeviceInstanceId.empty() && engine::SameText(item.DeviceInstanceId, live.DeviceInstanceId)) ||
                        (!item.TransportSuppliedName.empty() && engine::SameText(item.TransportSuppliedName, live.TransportSuppliedName));
                });
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    winrt::hstring ConfigurationsWindow::DeviceDisplayName(engine::PlanItem const& item) const
    {
        // The device's own name, from the endpoint when it's here.
        for (auto const& live : m_liveEndpoints)
        {
            auto const same = (!item.EndpointDeviceId.empty() && engine::SameText(item.EndpointDeviceId, live.EndpointDeviceId)) ||
                (!item.DeviceInstanceId.empty() && engine::SameText(item.DeviceInstanceId, live.DeviceInstanceId));

            if (same && !live.TransportSuppliedName.empty())
            {
                return winrt::hstring{ live.TransportSuppliedName };
            }
        }

        if (!item.TransportSuppliedName.empty())
        {
            return winrt::hstring{ item.TransportSuppliedName };
        }

        if (!item.Name.empty())
        {
            return winrt::hstring{ item.Name };
        }

        if (!item.YourName.empty())
        {
            return winrt::hstring{ item.YourName };
        }

        return res::GetString(L"CfgUnknownDevice");
    }
}
