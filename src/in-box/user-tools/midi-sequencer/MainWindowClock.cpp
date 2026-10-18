// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "DialogParts.h"
#include "MidiTimeCode.h"
#include "StringResources.h"

namespace res = ::midisequencer::resources;

namespace winrt::midisequencer::implementation
{
    using namespace dialogparts;

    namespace
    {
        // The device and group lists, the same widths everywhere in the dialog.
        constexpr double DeviceListWidth{ 220 };
        constexpr double GroupListWidth{ 200 };

        // The notes wrap to this: the dialog scrolls sideways, so nothing else stops them.
        constexpr double ClockPanelWidth{ 620 };

        // midiapp::MidiTimeCodeFrameRate values, in the order the list shows them.
        constexpr std::array<wchar_t const*, 4> RateNames{ L"TimeCodeRate24", L"TimeCodeRate25", L"TimeCodeRate2997", L"TimeCodeRate30" };
    }

    void MainWindow::ShowClockDialog()
    {
        [](winrt::com_ptr<MainWindow> strong) -> winrt::fire_and_forget
        {
            try
            {
                auto& settings = seq::AppSettings::Current();
                auto const weak = strong->get_weak();

                auto const secondary = strong->BrushFor(strong->m_palette.Text2);
                auto const endpointTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"EndpointChoiceTemplate")).as<xaml::DataTemplate>();
                auto const namedTemplate = strong->RootGrid().Resources().Lookup(winrt::box_value(L"NamedChoiceTemplate")).as<xaml::DataTemplate>();

                controls::StackPanel panel{};
                panel.Spacing(8);
                panel.Padding(xaml::Thickness{ 0, 0, 16, 0 });
                panel.MaxWidth(ClockPanelWidth);

                panel.Children().Append(Note(res::GetString(L"ClockDialogIntro"), secondary));

                // ---- where the tempo comes from ----

                panel.Children().Append(Heading(res::GetString(L"ClockTempoFromHeading")));

                auto const firstTempo = strong->m_doc.Tempo.empty() ? 120.0 : strong->m_doc.Tempo.front().BeatsPerMinute;

                auto const makeRadio = [&secondary](winrt::hstring const& title, winrt::hstring const& detail)
                {
                    controls::StackPanel content{};
                    content.Spacing(2);

                    controls::TextBlock titleText{};
                    titleText.Text(title);
                    content.Children().Append(titleText);
                    content.Children().Append(Note(detail, secondary));

                    controls::RadioButton radio{};
                    radio.GroupName(L"ClockTempoSource");
                    radio.Content(content);
                    automation::AutomationProperties::SetName(radio, title);
                    return radio;
                };

                auto const follow = settings.FollowClock();
                seq::EndpointRef const followEndpoint{ follow.EndpointName, follow.EndpointId };

                auto const fromSequence = makeRadio(res::GetString(L"ClockTempoThisSequence"),
                    res::FormatString(L"ClockTempoThisSequenceFormat", std::format(L"{:g}", std::round(firstTempo * 100.0) / 100.0)));
                fromSequence.IsChecked(!follow.Enabled);
                panel.Children().Append(fromSequence);

                auto const fromDevice = makeRadio(res::GetString(L"ClockTempoAnotherDevice"), res::GetString(L"ClockTempoAnotherDeviceHelp"));
                fromDevice.IsChecked(follow.Enabled);
                panel.Children().Append(fromDevice);

                // The device to follow, under its choice.
                controls::StackPanel followRow{};
                followRow.Orientation(controls::Orientation::Horizontal);
                followRow.Spacing(8);
                followRow.Margin(xaml::Thickness{ 28, 0, 0, 0 });

                controls::ComboBox followDevice{};
                followDevice.Width(DeviceListWidth);
                followDevice.ItemTemplate(endpointTemplate);
                strong->FillEndpointCombo(followDevice, true, followEndpoint);
                automation::AutomationProperties::SetName(followDevice, res::GetString(L"FollowDeviceName"));
                followRow.Children().Append(followDevice);

                controls::ComboBox followGroup{};
                followGroup.Width(GroupListWidth);
                followGroup.ItemTemplate(namedTemplate);
                strong->FillGroupCombo(followGroup, true, followEndpoint, follow.Group, false);
                automation::AutomationProperties::SetName(followGroup, res::GetString(L"FollowGroupName"));
                followRow.Children().Append(followGroup);

                panel.Children().Append(followRow);

                auto const followError = Note(L"", strong->ThemeBrush(L"SeqWarnTextBrush"));
                followError.Visibility(xaml::Visibility::Collapsed);
                followError.Margin(xaml::Thickness{ 28, 0, 0, 0 });
                panel.Children().Append(followError);

                auto const followChanged = std::make_shared<bool>(false);

                followDevice.SelectionChanged([weak, weakDevice = winrt::make_weak(followDevice), weakGroup = winrt::make_weak(followGroup), followChanged](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const device = weakDevice.get();
                    auto const group = weakGroup.get();

                    if (window && device && group)
                    {
                        *followChanged = true;
                        window->FillGroupCombo(group, true, ChosenEndpoint(device, {}, true), 0, false);
                    }
                });

                // The lists only matter while following is chosen.
                auto const enableFollowRow = [weakDevice = winrt::make_weak(followDevice), weakGroup = winrt::make_weak(followGroup), weakRadio = winrt::make_weak(fromDevice)]()
                {
                    auto const device = weakDevice.get();
                    auto const group = weakGroup.get();
                    auto const radio = weakRadio.get();

                    if (device && group && radio)
                    {
                        auto const on = radio.IsChecked() != nullptr && radio.IsChecked().GetBoolean();
                        device.IsEnabled(on);
                        group.IsEnabled(on);
                    }
                };

                enableFollowRow();
                fromDevice.Checked([enableFollowRow](auto&&, auto&&) { enableFollowRow(); });
                fromDevice.Unchecked([enableFollowRow](auto&&, auto&&) { enableFollowRow(); });

                // ---- devices that get the clock: what each one gets is chosen in Studio setup ----

                panel.Children().Append(Heading(res::GetString(L"ClockSendHeading")));

                std::vector<std::wstring> followers{};

                for (auto const& output : settings.ClockOutputs())
                {
                    seq::EndpointRef const endpoint{ output.EndpointName, output.EndpointId };

                    // A group Studio setup turned off gets nothing.
                    if ((strong->GroupsTurnedOff(endpoint, true) & (1u << (output.Group & 0x0F))) != 0)
                    {
                        continue;
                    }

                    auto const summary = strong->m_directory->Resolve(endpoint);
                    auto name = summary.has_value() ? summary->Live.Name : output.EndpointName;

                    if (name.empty())
                    {
                        name = std::wstring{ res::GetString(L"UnknownEndpoint") };
                    }

                    if (std::find(followers.begin(), followers.end(), name) == followers.end())
                    {
                        followers.push_back(std::move(name));
                    }
                }

                panel.Children().Append(Note(followers.empty() ? res::GetString(L"ClockNoDevices") : res::FormatString(L"ClockSentToFormat", JoinNames(followers)), secondary));
                panel.Children().Append(Note(res::GetString(L"ClockStudioHelp"), secondary));

                controls::Button studio{};
                studio.Style(strong->RootGrid().Resources().Lookup(winrt::box_value(L"SmallButtonStyle")).as<xaml::Style>());
                studio.HorizontalAlignment(xaml::HorizontalAlignment::Left);

                {
                    controls::StackPanel content{};
                    content.Orientation(controls::Orientation::Horizontal);
                    content.Spacing(6);

                    controls::FontIcon glyph{};
                    glyph.Glyph(L"\uE772");
                    glyph.FontSize(12);
                    content.Children().Append(glyph);

                    controls::TextBlock label{};
                    label.Text(res::GetString(L"StudioSetupButton"));
                    content.Children().Append(label);

                    studio.Content(content);
                    automation::AutomationProperties::SetName(studio, res::GetString(L"StudioSetupButton"));
                }

                panel.Children().Append(studio);

                // ---- MIDI Time Code ----

                panel.Children().Append(Heading(res::GetString(L"TimeCodeHeading")));

                auto const timeCode = settings.TimeCode();
                seq::EndpointRef const timeCodeEndpoint{ timeCode.EndpointName, timeCode.EndpointId };

                controls::StackPanel timeCodeRow{};
                timeCodeRow.Orientation(controls::Orientation::Horizontal);
                timeCodeRow.Spacing(8);

                controls::ToggleSwitch timeCodeOn{};
                timeCodeOn.OnContent(winrt::box_value(L""));
                timeCodeOn.OffContent(winrt::box_value(L""));
                timeCodeOn.MinWidth(0);
                timeCodeOn.Width(44);
                timeCodeOn.IsOn(timeCode.Enabled);
                timeCodeOn.VerticalAlignment(xaml::VerticalAlignment::Center);
                automation::AutomationProperties::SetName(timeCodeOn, res::GetString(L"TimeCodeSwitchName"));
                timeCodeRow.Children().Append(timeCodeOn);

                controls::TextBlock sendTo{};
                sendTo.Text(res::GetString(L"TimeCodeSendTo"));
                sendTo.VerticalAlignment(xaml::VerticalAlignment::Center);
                timeCodeRow.Children().Append(sendTo);

                controls::ComboBox timeCodeDevice{};
                timeCodeDevice.Width(DeviceListWidth);
                timeCodeDevice.ItemTemplate(endpointTemplate);
                strong->FillEndpointCombo(timeCodeDevice, false, timeCodeEndpoint);
                automation::AutomationProperties::SetName(timeCodeDevice, res::GetString(L"TimeCodeDeviceName"));
                timeCodeRow.Children().Append(timeCodeDevice);

                controls::ComboBox timeCodeGroup{};
                timeCodeGroup.Width(GroupListWidth);
                timeCodeGroup.ItemTemplate(namedTemplate);
                strong->FillGroupCombo(timeCodeGroup, false, timeCodeEndpoint, timeCode.Group, false);
                automation::AutomationProperties::SetName(timeCodeGroup, res::GetString(L"TimeCodeGroupName"));
                timeCodeRow.Children().Append(timeCodeGroup);

                panel.Children().Append(timeCodeRow);

                controls::StackPanel rateRow{};
                rateRow.Orientation(controls::Orientation::Horizontal);
                rateRow.Spacing(8);
                rateRow.Margin(xaml::Thickness{ 52, 0, 0, 0 });

                controls::ComboBox rate{};
                rate.Width(190);

                for (auto const key : RateNames)
                {
                    rate.Items().Append(winrt::box_value(res::GetString(key)));
                }

                rate.SelectedIndex(std::clamp(timeCode.Rate, 0, 3));
                automation::AutomationProperties::SetName(rate, res::GetString(L"TimeCodeRateName"));
                rateRow.Children().Append(rate);

                controls::TextBlock startingAt{};
                startingAt.Text(res::GetString(L"TimeCodeStartingAt"));
                startingAt.VerticalAlignment(xaml::VerticalAlignment::Center);
                rateRow.Children().Append(startingAt);

                controls::TextBox start{};
                start.Width(120);
                start.Text(winrt::hstring{ timeCode.Start });
                start.FontFamily(media::FontFamily{ L"Cascadia Mono, Consolas" });
                automation::AutomationProperties::SetName(start, res::GetString(L"TimeCodeStartName"));
                rateRow.Children().Append(start);

                panel.Children().Append(rateRow);

                // Where and how it's sent only matter while it's on.
                auto const enableTimeCode = [weakDevice = winrt::make_weak(timeCodeDevice), weakGroup = winrt::make_weak(timeCodeGroup), weakRate = winrt::make_weak(rate), weakStart = winrt::make_weak(start), weakStartingAt = winrt::make_weak(startingAt)](bool on)
                {
                    if (auto const device = weakDevice.get()) { device.IsEnabled(on); }
                    if (auto const group = weakGroup.get()) { group.IsEnabled(on); }
                    if (auto const rateBox = weakRate.get()) { rateBox.IsEnabled(on); }
                    if (auto const startBox = weakStart.get()) { startBox.IsEnabled(on); }
                    if (auto const label = weakStartingAt.get()) { label.Opacity(on ? 1.0 : 0.45); }
                };

                enableTimeCode(timeCode.Enabled);

                timeCodeOn.Toggled([enableTimeCode](foundation::IInspectable const& sender, auto&&)
                {
                    enableTimeCode(sender.as<controls::ToggleSwitch>().IsOn());
                });

                auto const timeCodeError = Note(L"", strong->ThemeBrush(L"SeqWarnTextBrush"));
                timeCodeError.Visibility(xaml::Visibility::Collapsed);
                timeCodeError.Margin(xaml::Thickness{ 52, 0, 0, 0 });
                panel.Children().Append(timeCodeError);

                auto const timeCodeHelp = Note(res::GetString(L"TimeCodeHelp"), secondary);
                timeCodeHelp.Margin(xaml::Thickness{ 52, 0, 0, 0 });
                panel.Children().Append(timeCodeHelp);

                auto const timeCodeChanged = std::make_shared<bool>(false);

                timeCodeDevice.SelectionChanged([weak, weakDevice = winrt::make_weak(timeCodeDevice), weakGroup = winrt::make_weak(timeCodeGroup), timeCodeChanged](auto&&, auto&&)
                {
                    auto const window = weak.get();
                    auto const device = weakDevice.get();
                    auto const group = weakGroup.get();

                    if (window && device && group)
                    {
                        *timeCodeChanged = true;
                        window->FillGroupCombo(group, false, ChosenEndpoint(device, {}, true), 0, false);
                    }
                });

                // ---- timing ----

                panel.Children().Append(Heading(res::GetString(L"TimingTitle")));

                controls::CheckBox offsets{};
                offsets.Content(winrt::box_value(res::GetString(L"ApplyOffsetsCheck")));
                offsets.IsChecked(settings.ApplyDeviceOffsets());
                panel.Children().Append(offsets);
                panel.Children().Append(Note(res::GetString(L"ApplyOffsetsHelp"), secondary));
                panel.Children().Append(Note(res::GetString(L"ApplyOffsetsWhenOff"), secondary));

                controls::ScrollViewer scroller{};
                scroller.Content(panel);
                scroller.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Auto);
                scroller.HorizontalScrollMode(controls::ScrollMode::Auto);
                scroller.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Auto);

                controls::ContentDialog dialog{};
                dialog.XamlRoot(strong->Content().XamlRoot());
                dialog.Resources().Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(ClockPanelWidth + 80.0));
                dialog.Title(winrt::box_value(res::GetString(L"ClockDialogTitle")));
                dialog.Content(scroller);
                dialog.PrimaryButtonText(res::GetString(L"DialogDone"));
                dialog.CloseButtonText(res::GetString(L"DialogCancel"));
                dialog.DefaultButton(controls::ContentDialogButton::Primary);
                dialog.RequestedTheme(strong->RootGrid().ActualTheme());

                // Following needs a device; time code that's on needs a device and a start it can
                // read. The dialog stays open to say so.
                auto const valid = [fromDevice, followDevice, followEndpoint, followChanged, followError, timeCodeOn, timeCodeDevice, rate, start, timeCodeError, timeCodeChanged, timeCodeEndpoint]()
                {
                    if (IsChecked(fromDevice) && ChosenEndpoint(followDevice, followEndpoint, *followChanged).IsEmpty())
                    {
                        followError.Text(res::GetString(L"FollowNoDevice"));
                        followError.Visibility(xaml::Visibility::Visible);
                        return false;
                    }

                    if (!timeCodeOn.IsOn())
                    {
                        return true;
                    }

                    if (ChosenEndpoint(timeCodeDevice, timeCodeEndpoint, *timeCodeChanged).IsEmpty())
                    {
                        timeCodeError.Text(res::GetString(L"TimeCodeNoDevice"));
                        timeCodeError.Visibility(xaml::Visibility::Visible);
                        return false;
                    }

                    midiapp::MidiTimeCodePosition position{};

                    if (!midiapp::TryParsePosition(std::wstring{ start.Text() }, midiapp::FrameRateFromValue(std::clamp(rate.SelectedIndex(), 0, 3)), position))
                    {
                        timeCodeError.Text(res::GetString(L"TimeCodeBadStart"));
                        timeCodeError.Visibility(xaml::Visibility::Visible);
                        return false;
                    }

                    return true;
                };

                dialog.PrimaryButtonClick([valid](auto&&, controls::ContentDialogButtonClickEventArgs const& args)
                {
                    if (!valid())
                    {
                        args.Cancel(true);
                    }
                });

                // Only one dialog can be open at a time, so this one keeps what's chosen and closes
                // before Studio setup opens.
                auto const toStudio = std::make_shared<bool>(false);

                studio.Click([valid, toStudio, weakDialog = winrt::make_weak(dialog)](auto&&, auto&&)
                {
                    if (valid())
                    {
                        *toStudio = true;

                        if (auto const shown = weakDialog.get())
                        {
                            shown.Hide();
                        }
                    }
                });

                auto const result = co_await dialog.ShowAsync();

                if ((result == controls::ContentDialogResult::Primary || *toStudio) && !strong->m_closing)
                {
                    auto updated = timeCode;
                    auto const endpoint = ChosenEndpoint(timeCodeDevice, timeCodeEndpoint, *timeCodeChanged);
                    updated.Enabled = timeCodeOn.IsOn() && !endpoint.IsEmpty();
                    updated.EndpointName = endpoint.Name;
                    updated.EndpointId = endpoint.Id;
                    updated.Group = ChosenGroup(timeCodeGroup, timeCode.Group);
                    updated.Rate = std::clamp(rate.SelectedIndex(), 0, 3);

                    midiapp::MidiTimeCodePosition position{};
                    auto const frameRate = midiapp::FrameRateFromValue(updated.Rate);

                    if (midiapp::TryParsePosition(std::wstring{ start.Text() }, frameRate, position))
                    {
                        updated.Start = midiapp::FormatPosition(midiapp::ClampPosition(position, frameRate), frameRate);
                    }

                    settings.TimeCode(updated);

                    auto followUpdated = follow;
                    auto const followChosen = ChosenEndpoint(followDevice, followEndpoint, *followChanged);
                    followUpdated.Enabled = IsChecked(fromDevice) && !followChosen.IsEmpty();

                    if (!followChosen.IsEmpty())
                    {
                        followUpdated.EndpointName = followChosen.Name;
                        followUpdated.EndpointId = followChosen.Id;
                        followUpdated.Group = ChosenGroup(followGroup, follow.Group);
                    }

                    settings.FollowClock(followUpdated);

                    auto const applyOffsets = IsChecked(offsets);
                    settings.ApplyDeviceOffsets(applyOffsets);
                    strong->m_directory->ApplyOffsets(applyOffsets);

                    strong->ApplyEngineSettings();
                    strong->PrepareConnectionsAsync();
                    strong->UpdateProtocolNote();
                    strong->UpdateStatusBar();
                }

                if (*toStudio && !strong->m_closing)
                {
                    strong->ShowStudioSetup();
                }
            }
            MIDI_SEQUENCER_CATCH_AND_LOG(L"Unable to show the clock settings.")
        }(get_strong());
    }
}
