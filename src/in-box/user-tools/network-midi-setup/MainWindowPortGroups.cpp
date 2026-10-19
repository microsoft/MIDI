// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// The MIDI 1.0 ports a Network MIDI 2.0 connection gets when the device at the other end doesn't
// describe its own groups with function blocks: which groups have an input, an output or both, and
// what MIDI 1.0 apps call them. One dialog changes them everywhere they can be set. Only one dialog
// can be open at a time, so Connect, Customize and Create host step aside for it, and the code
// which opened them brings them back when it closes.

#include "pch.h"
#include "MainWindow.xaml.h"

#include <map>

#include "StringResources.h"

namespace res = ::midinetworksetup::resources;

namespace winrt::midinetworksetup::implementation
{
    namespace
    {
        std::wstring_view TrimmedView(_In_ std::wstring_view const value) noexcept
        {
            auto const first = value.find_first_not_of(L" \t\r\n");

            return first == std::wstring_view::npos ?
                std::wstring_view{} :
                value.substr(first, value.find_last_not_of(L" \t\r\n") - first + 1);
        }

        bool IsSameTextIgnoringCase(_In_ std::wstring_view const left, _In_ std::wstring_view const right) noexcept
        {
            return left.size() == right.size() &&
                (left.empty() || ::CompareStringOrdinal(
                    left.data(), static_cast<int>(left.size()),
                    right.data(), static_cast<int>(right.size()),
                    TRUE) == CSTR_EQUAL);
        }

        // What the port will actually be called. The SDK cleans a name the way the transport does,
        // so asking it means two names which end up the same are caught here, rather than turning
        // into two ports with one name.
        std::wstring CleanedPortName(_In_ std::wstring_view const name) noexcept
        {
            try
            {
                midi2net::MidiNetworkMidi1PortGroup group{};
                group.Name(winrt::hstring{ name });

                return std::wstring{ group.Name() };
            }
            catch (...)
            {
            }

            try
            {
                return std::wstring{ TrimmedView(name) };
            }
            catch (...)
            {
                return std::wstring{};
            }
        }

        bool TryParseEntryId(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
        {
            value = winrt::guid{};

            if (text.empty())
            {
                return false;
            }

            try
            {
                value = winrt::guid{ std::wstring_view{ text } };
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        template <typename TItem>
        TItem ItemOf(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                return element == nullptr ? nullptr : element.DataContext().try_as<TItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // A button handed to another control, like an InfoBar's action button, may not inherit the
        // row as its data context, so those buttons also carry the row in their Tag
        template <typename TItem>
        TItem ItemOrTagOf(_In_ foundation::IInspectable const& sender) noexcept
        {
            if (auto item = ItemOf<TItem>(sender); item != nullptr)
            {
                return item;
            }

            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                return element == nullptr ? nullptr : element.Tag().try_as<TItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        bool IsChecked(_In_ controls::CheckBox const& box) noexcept
        {
            try
            {
                auto const value = box.IsChecked();

                return value != nullptr && value.Value();
            }
            catch (...)
            {
                return false;
            }
        }

        midi2net::MidiNetworkSavedClient FindSavedNetworkClient(_In_ winrt::guid const& clientId) noexcept
        {
            try
            {
                for (auto const& saved : midi2net::MidiNetworkTransportManager::GetSavedClients())
                {
                    if (saved != nullptr && saved.ClientId() == clientId)
                    {
                        return saved;
                    }
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }

        // Makes a change in the running service and, once the service has taken it, saves it.
        // Returns what to tell the customer: doneMessage, or why it didn't happen.
        winrt::hstring SendAndSaveMidi1PortsChange(
            _In_ midi2svc::IMidiServiceTransportPluginConfig const& config,
            _In_ winrt::hstring const& doneMessage) noexcept
        {
            try
            {
                auto const response = midi2svc::MidiServiceTransportPluginConfigManager::SendUpdate(config);

                if (response == nullptr || response.Status() != midi2svc::MidiServiceConfigResponseStatus::Success)
                {
                    return (response == nullptr || response.ServiceErrorMessage().empty()) ?
                        res::GetString(L"Midi1PortsChangeFailedGeneral") :
                        res::FormatString(L"Midi1PortsChangeFailedFormat", response.ServiceErrorMessage());
                }

                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                return (saved != nullptr && saved.Success()) ?
                    doneMessage :
                    res::FormatString(L"ChangeNotSavedFormat", saved == nullptr ? winrt::hstring{} : saved.ErrorMessage());
            }
            catch (...)
            {
                return res::GetString(L"Midi1PortsChangeFailedGeneral");
            }
        }

        winrt::hstring PortCountText(
            _In_ uint32_t const count,
            _In_ std::wstring_view const noneKey,
            _In_ std::wstring_view const oneKey,
            _In_ std::wstring_view const manyKey) noexcept
        {
            if (count == 0)
            {
                return res::GetString(noneKey);
            }

            return count == 1 ? res::GetString(oneKey) : res::FormatString(manyKey, count);
        }
    }


    // ------------------------------------------------------------------------------------
    // layouts
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    Midi1PortLayout NormalizedMidi1PortLayout(Midi1PortLayout layout) noexcept
    {
        try
        {
            std::map<uint8_t, Midi1PortGroupChoice> byGroup{};

            for (auto& group : layout.Groups)
            {
                if (group.GroupIndex >= MIDI_NETWORK_MIDI1_PORT_GROUP_COUNT)
                {
                    continue;
                }

                // a later entry for a group replaces an earlier one, as it does in the transport
                if (!group.HasInput && !group.HasOutput)
                {
                    byGroup.erase(group.GroupIndex);
                    continue;
                }

                group.Name = CleanedPortName(group.Name);

                byGroup[group.GroupIndex] = std::move(group);
            }

            Midi1PortLayout result{};

            result.Count = std::clamp<uint8_t>(
                layout.Count,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);

            for (auto& entry : byGroup)
            {
                result.Groups.push_back(std::move(entry.second));
            }

            // groups 1 to N with both ports and no names are exactly what the count creates
            bool isCount = !result.Groups.empty();

            for (size_t i = 0; isCount && i < result.Groups.size(); i++)
            {
                auto const& group = result.Groups[i];

                isCount = group.GroupIndex == i && group.HasInput && group.HasOutput && group.Name.empty();
            }

            if (isCount)
            {
                result.Count = static_cast<uint8_t>(result.Groups.size());
                result.Groups.clear();
            }

            return result;
        }
        catch (...)
        {
            return Midi1PortLayout{};
        }
    }

    _Use_decl_annotations_
    Midi1PortLayout Midi1PortLayoutFromApi(
        uint8_t const count,
        collections::IIterable<midi2net::MidiNetworkMidi1PortGroup> const& groups) noexcept
    {
        Midi1PortLayout layout{};
        layout.Count = count;

        try
        {
            if (groups != nullptr)
            {
                for (auto const& group : groups)
                {
                    midi2::MidiGroup midiGroup{ nullptr };

                    if (group != nullptr)
                    {
                        midiGroup = group.Group();
                    }

                    if (midiGroup == nullptr)
                    {
                        continue;
                    }

                    layout.Groups.push_back({
                        midiGroup.Index(),
                        std::wstring{ group.Name() },
                        group.HasSourcePort(),
                        group.HasDestinationPort() });
                }
            }
        }
        catch (...)
        {
            layout.Groups.clear();
        }

        return NormalizedMidi1PortLayout(std::move(layout));
    }

    _Use_decl_annotations_
    uint8_t Midi1PortCountFor(Midi1PortLayout const& layout) noexcept
    {
        if (layout.Groups.empty())
        {
            return std::clamp<uint8_t>(
                layout.Count,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MINIMUM,
                MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM);
        }

        uint8_t highest{ 0 };

        for (auto const& group : layout.Groups)
        {
            highest = (std::max)(highest, group.GroupIndex);
        }

        return static_cast<uint8_t>((std::min)(
            static_cast<uint32_t>(highest) + 1u,
            static_cast<uint32_t>(MIDI_NETWORK_MIDI_FALLBACK_MIDI1_PORT_COUNT_MAXIMUM)));
    }

    _Use_decl_annotations_
    void WriteMidi1PortGroups(
        Midi1PortLayout const& layout,
        collections::IVector<midi2net::MidiNetworkMidi1PortGroup> const& target)
    {
        target.Clear();

        for (auto const& group : layout.Groups)
        {
            target.Append(midi2net::MidiNetworkMidi1PortGroup{
                midi2::MidiGroup{ group.GroupIndex },
                winrt::hstring{ group.Name },
                group.HasInput,
                group.HasOutput });
        }
    }

    _Use_decl_annotations_
    void WriteMidi1PortGroupsAsList(
        Midi1PortLayout const& layout,
        collections::IVector<midi2net::MidiNetworkMidi1PortGroup> const& target)
    {
        if (!layout.Groups.empty())
        {
            WriteMidi1PortGroups(layout, target);
            return;
        }

        target.Clear();

        auto const count = Midi1PortCountFor(layout);

        for (uint8_t index = 0; index < count; index++)
        {
            target.Append(midi2net::MidiNetworkMidi1PortGroup{
                midi2::MidiGroup{ index },
                winrt::hstring{},
                true,
                true });
        }
    }

    _Use_decl_annotations_
    winrt::hstring Midi1PortLayoutSummary(Midi1PortLayout const& layout) noexcept
    {
        try
        {
            uint32_t inputs{ 0 };
            uint32_t outputs{ 0 };
            size_t named{ 0 };

            if (layout.Groups.empty())
            {
                inputs = Midi1PortCountFor(layout);
                outputs = inputs;
            }
            else
            {
                for (auto const& group : layout.Groups)
                {
                    if (group.HasInput) inputs++;
                    if (group.HasOutput) outputs++;
                    if (!group.Name.empty()) named++;
                }
            }

            auto const inputText = PortCountText(inputs, L"Midi1PortsNoInputs", L"Midi1PortsOneInput", L"Midi1PortsInputsFormat");
            auto const outputText = PortCountText(outputs, L"Midi1PortsNoOutputs", L"Midi1PortsOneOutput", L"Midi1PortsOutputsFormat");

            auto const format =
                named == 0 ? L"Midi1PortsSummaryWindowsNamesFormat" :
                named == layout.Groups.size() ? L"Midi1PortsSummaryYourNamesFormat" :
                L"Midi1PortsSummarySomeNamesFormat";

            return res::FormatString(format, inputText, outputText);
        }
        catch (...)
        {
            return winrt::hstring{};
        }
    }

    _Use_decl_annotations_
    winrt::hstring Midi1PortsLineText(Midi1PortLayout const& layout) noexcept
    {
        return res::FormatString(L"Midi1PortsLineFormat", Midi1PortLayoutSummary(layout));
    }

    _Use_decl_annotations_
    bool IsSameRemoteClientText(winrt::hstring const& left, winrt::hstring const& right) noexcept
    {
        return IsSameTextIgnoringCase(TrimmedView(left), TrimmedView(right));
    }

    _Use_decl_annotations_
    midi2net::MidiNetworkRemoteClientSettings FindRemoteClientSettings(
        collections::IIterable<midi2net::MidiNetworkRemoteClientSettings> const& settings,
        winrt::hstring const& remoteName,
        winrt::hstring const& remoteProductInstanceId) noexcept
    {
        try
        {
            if (settings != nullptr)
            {
                for (auto const& entry : settings)
                {
                    if (entry != nullptr &&
                        IsSameRemoteClientText(entry.RemoteClientName(), remoteName) &&
                        IsSameRemoteClientText(entry.RemoteClientProductInstanceId(), remoteProductInstanceId))
                    {
                        return entry;
                    }
                }
            }
        }
        catch (...)
        {
        }

        return nullptr;
    }


    // ------------------------------------------------------------------------------------
    // the dialog
    // ------------------------------------------------------------------------------------

    // Every group gets a row, so choosing a group is ticking a box rather than adding a line
    _Use_decl_annotations_
    void MainWindow::FillMidi1PortGroupRows(Midi1PortLayout const& layout, bool const offerNames) noexcept
    {
        try
        {
            m_midi1PortGroupRows.Clear();

            auto const count = Midi1PortCountFor(layout);

            for (uint8_t index = 0; index < MIDI_NETWORK_MIDI1_PORT_GROUP_COUNT; index++)
            {
                Midi1PortGroupChoice choice{ index };

                if (layout.Groups.empty())
                {
                    choice.HasInput = index < count;
                    choice.HasOutput = choice.HasInput;
                }
                else
                {
                    for (auto const& group : layout.Groups)
                    {
                        if (group.GroupIndex == index)
                        {
                            choice = group;
                            break;
                        }
                    }
                }

                auto const number = static_cast<uint32_t>(index) + 1;

                auto row = winrt::make_self<Midi1PortGroupItem>();

                // A name a host already has is kept even where names aren't offered, so changing
                // its groups doesn't quietly lose one set some other way
                row->InternalInitialize(
                    choice,
                    offerNames,
                    res::FormatString(L"Midi1PortGroupFormat", number),
                    res::FormatString(L"Midi1PortGroupNameAutomationFormat", number),
                    res::FormatString(L"Midi1PortGroupInputAutomationFormat", number),
                    res::FormatString(L"Midi1PortGroupOutputAutomationFormat", number));

                m_midi1PortGroupRows.Append(*row);
            }
        }
        catch (...)
        {
        }
    }

    Midi1PortLayout MainWindow::ReadMidi1PortGroupRows() const noexcept
    {
        Midi1PortLayout layout{};

        try
        {
            for (auto const& row : m_midi1PortGroupRows)
            {
                if (row == nullptr || (!row.HasInput() && !row.HasOutput()))
                {
                    continue;
                }

                layout.Groups.push_back({ row.GroupIndex(), std::wstring{ row.Name() }, row.HasInput(), row.HasOutput() });
            }
        }
        catch (...)
        {
        }

        return NormalizedMidi1PortLayout(std::move(layout));
    }

    winrt::hstring MainWindow::Midi1PortGroupRowsProblem() noexcept
    {
        try
        {
            // the rows mean nothing while the device stays on the host's ports
            if (Midi1PortGroupsUseHostCheckBox().Visibility() == xaml::Visibility::Visible &&
                IsChecked(Midi1PortGroupsUseHostCheckBox()))
            {
                return winrt::hstring{};
            }

            bool anyPort{ false };

            std::vector<std::wstring> inputNames{};
            std::vector<std::wstring> outputNames{};

            auto const findName = [](std::vector<std::wstring> const& names, std::wstring const& name)
                {
                    return std::any_of(names.begin(), names.end(),
                        [&name](std::wstring const& other) { return IsSameTextIgnoringCase(other, name); });
                };

            for (auto const& row : m_midi1PortGroupRows)
            {
                if (row == nullptr)
                {
                    continue;
                }

                auto const name = CleanedPortName(row.Name());
                auto const number = static_cast<uint32_t>(row.GroupIndex()) + 1;

                if (!row.HasInput() && !row.HasOutput())
                {
                    // Dropping it without a word would lose what the customer typed
                    if (!name.empty() && row.NameVisibility() == xaml::Visibility::Visible)
                    {
                        return res::FormatString(L"Midi1PortsNameWithoutPortFormat", number);
                    }

                    continue;
                }

                anyPort = true;

                // a host's names aren't shown, so there's nothing the customer could fix
                if (name.empty() || row.NameVisibility() != xaml::Visibility::Visible)
                {
                    continue;
                }

                // Two ports with one name can't be told apart in a MIDI 1.0 app. An input and an
                // output can share one, because apps list them separately.
                if (row.HasInput())
                {
                    if (findName(inputNames, name))
                    {
                        return res::FormatString(L"Midi1PortsDuplicateInputNameFormat", name);
                    }

                    inputNames.push_back(name);
                }

                if (row.HasOutput())
                {
                    if (findName(outputNames, name))
                    {
                        return res::FormatString(L"Midi1PortsDuplicateOutputNameFormat", name);
                    }

                    outputNames.push_back(name);
                }
            }

            if (!anyPort)
            {
                return res::GetString(L"Midi1PortsNeedOnePort");
            }
        }
        catch (...)
        {
        }

        return winrt::hstring{};
    }

    _Use_decl_annotations_
    foundation::IAsyncOperation<bool> MainWindow::ShowMidi1PortGroupsDialogAsync(
        winrt::hstring const title,
        Midi1PortsScope const scope,
        winrt::hstring const primaryButtonText,
        winrt::hstring const hostPortsText,
        std::shared_ptr<Midi1PortLayout> layout,
        std::shared_ptr<bool> useHostPorts)
    {
        auto strongThis = get_strong();

        // only one dialog can be open at a time, and a second ShowAsync throws
        if (m_openDialog != nullptr || m_closing || layout == nullptr)
        {
            co_return false;
        }

        auto const offerNames = scope == Midi1PortsScope::Device;
        auto const offerHostPorts = !hostPortsText.empty() && useHostPorts != nullptr;
        auto const startsOnHostPorts = offerHostPorts && *useHostPorts;

        try
        {
            FillMidi1PortGroupRows(*layout, offerNames);

            Midi1PortGroupsDialog().Title(winrt::box_value(title));
            Midi1PortGroupsDialog().PrimaryButtonText(primaryButtonText);

            Midi1PortGroupsIntroText().Text(res::GetString(
                scope == Midi1PortsScope::Host ? L"Midi1PortGroupsHostIntro" :
                offerHostPorts ? L"Midi1PortGroupsDeviceOnHostIntro" :
                L"Midi1PortGroupsDeviceIntro"));

            Midi1PortGroupsHintText().Text(res::GetString(offerNames ? L"Midi1PortGroupsNamesHint" : L"Midi1PortGroupsNoNamesHint"));
            Midi1PortGroupsNameHeader().Visibility(offerNames ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            // Without the name boxes, the full width would leave the rows in the left half
            Midi1PortGroupsContent().Width(offerNames ? 640.0 : 460.0);

            // A row with a name box is about 41 tall and one without about 40. Each height shows
            // seven rows and cuts the eighth one's label through its middle, which reads as a row
            // going on below rather than as stray marks.
            Midi1PortGroupsListScroller().MaxHeight(offerNames ? 306.0 : 300.0);

            Midi1PortGroupsUseHostCheckBox().Visibility(offerHostPorts ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
            Midi1PortGroupsUseHostCheckBox().Content(winrt::box_value(res::FormatString(L"Midi1PortGroupsUseHostFormat", hostPortsText)));
            Midi1PortGroupsUseHostCheckBox().IsChecked(startsOnHostPorts);

            // Set here as well as by the check box, which raises nothing when it's already in the
            // state asked for
            Midi1PortGroupsList().IsEnabled(!startsOnHostPorts);

            ShowMidi1PortGroupsProblem({});

            Midi1PortGroupsDialog().XamlRoot(Content().XamlRoot());
        }
        catch (...)
        {
            co_return false;
        }

        auto result = controls::ContentDialogResult::None;

        m_openDialog = Midi1PortGroupsDialog();

        try
        {
            result = co_await Midi1PortGroupsDialog().ShowAsync();
        }
        catch (...)
        {
            result = controls::ContentDialogResult::None;
        }

        m_openDialog = nullptr;

        if (result != controls::ContentDialogResult::Primary)
        {
            co_return false;
        }

        if (offerHostPorts)
        {
            *useHostPorts = IsChecked(Midi1PortGroupsUseHostCheckBox());
        }

        // what the device had is kept while it stays on the host's ports
        if (!offerHostPorts || !*useHostPorts)
        {
            *layout = ReadMidi1PortGroupRows();
        }

        co_return true;
    }

    _Use_decl_annotations_
    void MainWindow::OnMidi1PortGroupsUseHostChanged(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            // the rows describe ports of the device's own
            Midi1PortGroupsList().IsEnabled(!IsChecked(Midi1PortGroupsUseHostCheckBox()));
            ShowMidi1PortGroupsProblem({});
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::ShowMidi1PortGroupsProblem(winrt::hstring const& problem) noexcept
    {
        try
        {
            auto const hasProblem = !problem.empty();

            if (hasProblem)
            {
                Midi1PortGroupsErrorBar().Message(problem);
            }

            Midi1PortGroupsErrorBar().IsOpen(hasProblem);
            Midi1PortGroupsHintText().Visibility(hasProblem ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        }
        catch (...)
        {
        }
    }

    // Checked here rather than as the customer types, so a half finished choice isn't flagged
    _Use_decl_annotations_
    void MainWindow::OnMidi1PortGroupsPrimaryButtonClick(
        controls::ContentDialog const&,
        controls::ContentDialogButtonClickEventArgs const& args)
    {
        try
        {
            auto const problem = Midi1PortGroupRowsProblem();

            if (!problem.empty())
            {
                // stays open, with everything as the customer left it
                args.Cancel(true);

                ShowMidi1PortGroupsProblem(problem);

                return;
            }

            ShowMidi1PortGroupsProblem({});
        }
        catch (...)
        {
        }
    }

    // The button in the Connect, Customize and Create host dialogs. Hiding the dialog completes
    // the ShowAsync its opener is waiting on, and the flag tells the opener it wasn't canceled.
    _Use_decl_annotations_
    void MainWindow::OnStepAsideForMidi1PortsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        try
        {
            if (m_openDialog == nullptr)
            {
                return;
            }

            m_midi1PortGroupsRequested = true;

            m_openDialog.Hide();
        }
        catch (...)
        {
            m_midi1PortGroupsRequested = false;
        }
    }


    // ------------------------------------------------------------------------------------
    // changing them
    // ------------------------------------------------------------------------------------

    // Connect by address is on the page rather than in a dialog, so nothing has to step aside
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnManualConnectMidi1PortsClick(foundation::IInspectable const&, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        try
        {
            auto layout = std::make_shared<Midi1PortLayout>(m_manualConnectMidi1Ports);

            std::wstring const address{ TrimmedView(ManualAddressTextBox().Text()) };

            auto const title = address.empty() ?
                res::GetString(L"Midi1PortGroupsTitle") :
                res::FormatString(L"Midi1PortGroupsTitleFormat", address);

            if (!co_await ShowMidi1PortGroupsDialogAsync(
                title,
                Midi1PortsScope::Device,
                res::GetString(L"Midi1PortGroupsOkButton"),
                winrt::hstring{},
                layout,
                nullptr))
            {
                co_return;
            }

            m_manualConnectMidi1Ports = *layout;

            ManualConnectMidi1PortsText().Text(Midi1PortsLineText(*layout));
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to choose the MIDI 1.0 ports for a connection by address.")
    }

    // Applies to the connection straight away, without disconnecting it
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnChangeRemoteHostMidi1PortsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto item = ItemOf<midinetworksetup::RemoteHostItem>(sender);

        winrt::guid clientId{};

        if (item == nullptr || !TryParseEntryId(item.ClientId(), clientId))
        {
            co_return;
        }

        // A change also says whether the entry creates MIDI 1.0 ports at all, and only the saved
        // entry records that. Changing an entry which isn't saved would quietly reset it.
        auto const savedClient = FindSavedNetworkClient(clientId);

        if (savedClient == nullptr)
        {
            SetRemoteStatus(res::FormatString(L"Midi1PortsNeedsSavedEntryFormat", item.DisplayName()));

            co_return;
        }

        Midi1PortLayout current{};
        bool createMidi1Ports{ true };

        try
        {
            current = Midi1PortLayoutFromApi(savedClient.FallbackMidi1PortCount(), savedClient.FallbackMidi1PortGroups());
            createMidi1Ports = !savedClient.CreateOnlyUmpEndpoints();
        }
        catch (...)
        {
            SetRemoteStatus(res::GetString(L"Midi1PortsChangeFailedGeneral"));

            co_return;
        }

        auto const displayName = item.DisplayName();
        auto layout = std::make_shared<Midi1PortLayout>(current);

        if (!co_await ShowMidi1PortGroupsDialogAsync(
                res::FormatString(L"Midi1PortGroupsTitleFormat", displayName),
                Midi1PortsScope::Device,
                res::GetString(L"Midi1PortGroupsSaveButton"),
                winrt::hstring{},
                layout,
                nullptr) ||
            *layout == current)
        {
            co_return;
        }

        midi2net::MidiNetworkClientUpdateConfig update{ nullptr };

        try
        {
            update = midi2net::MidiNetworkClientUpdateConfig{};

            update.ClientId(clientId);

            // an update always carries these two
            update.CreateMidi1Ports(createMidi1Ports);
            update.FallbackMidi1PortCount(Midi1PortCountFor(*layout));

            WriteMidi1PortGroups(*layout, update.FallbackMidi1PortGroups());
        }
        catch (...)
        {
            SetRemoteStatus(res::GetString(L"Midi1PortsChangeFailedGeneral"));

            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const doneMessage = res::FormatString(L"Midi1PortsChangedFormat", displayName);

        item.IsBusy(true);

        co_await winrt::resume_background();

        auto const message = SendAndSaveMidi1PortsChange(update, doneMessage);

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetRemoteStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }

    // What every device which connects to the host gets, unless it has ports of its own. Applies
    // to the devices already connected too, without disconnecting them.
    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnChangeHostMidi1PortsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        auto item = ItemOrTagOf<midinetworksetup::LocalHostItem>(sender);

        winrt::guid hostId{};

        if (item == nullptr || !TryParseEntryId(item.HostId(), hostId))
        {
            co_return;
        }

        auto const current = winrt::get_self<LocalHostItem>(item)->Midi1Ports();
        auto const displayName = item.DisplayName();
        auto layout = std::make_shared<Midi1PortLayout>(current);

        if (!co_await ShowMidi1PortGroupsDialogAsync(
                res::FormatString(L"Midi1PortGroupsTitleFormat", displayName),
                Midi1PortsScope::Host,
                res::GetString(L"Midi1PortGroupsSaveButton"),
                winrt::hstring{},
                layout,
                nullptr) ||
            *layout == current)
        {
            co_return;
        }

        midi2net::MidiNetworkHostUpdateConfig config{ nullptr };

        try
        {
            config = midi2net::MidiNetworkHostUpdateConfig{ hostId };

            // only these, so nothing else the host was set up with is touched
            config.FallbackMidi1PortCount(Midi1PortCountFor(*layout));

            WriteMidi1PortGroups(*layout, config.FallbackMidi1PortGroups());
        }
        catch (...)
        {
            SetLocalStatus(res::GetString(L"Midi1PortsChangeFailedGeneral"));

            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        item.IsBusy(true);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            auto const response = co_await midi2net::MidiNetworkTransportManager::UpdateNetworkHostAsync(config);

            if (response != nullptr && response.Success())
            {
                auto const saved = midi2svc::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                message = saved != nullptr && saved.Success() ?
                    res::FormatString(L"Midi1PortsChangedFormat", displayName) :
                    res::FormatString(L"ChangeNotSavedFormat", saved == nullptr ? winrt::hstring{} : saved.ErrorMessage());
            }
            else
            {
                message = (response == nullptr || response.ErrorMessage().empty()) ?
                    res::GetString(L"Midi1PortsChangeFailedGeneral") :
                    res::FormatString(L"Midi1PortsChangeFailedFormat", response.ErrorMessage());
            }
        }
        catch (...)
        {
            message = res::GetString(L"Midi1PortsChangeFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, item, message]()
                {
                    item.IsBusy(false);

                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }


    // ------------------------------------------------------------------------------------
    // one device on a host on this PC
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::OnChangeRemoteClientMidi1PortsClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = ItemOf<midinetworksetup::HostConnectionItem>(sender);

            if (item == nullptr)
            {
                return;
            }

            auto const self = winrt::get_self<HostConnectionItem>(item);

            EditRemoteClientMidi1PortsAsync(
                item.HostId(),
                self->RemoteName(),
                item.ProductInstanceId(),
                self->HasOwnMidi1Ports(),
                self->OwnMidi1Ports());
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to change the MIDI 1.0 ports for a connected device.")
    }

    _Use_decl_annotations_
    void MainWindow::OnChangeRemoteClientMidi1PortsEntryClick(foundation::IInspectable const& sender, xaml::RoutedEventArgs const&)
    {
        try
        {
            auto const item = ItemOf<midinetworksetup::RemoteClientSpeedItem>(sender);

            if (item == nullptr)
            {
                return;
            }

            auto const self = winrt::get_self<RemoteClientSpeedItem>(item);

            EditRemoteClientMidi1PortsAsync(
                item.HostId(),
                self->RemoteName(),
                item.ProductInstanceId(),
                self->HasOwnMidi1Ports(),
                self->OwnMidi1Ports());
        }
        MIDI_NETSETUP_CATCH_AND_LOG(L"Unable to change a device's own MIDI 1.0 ports.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::EditRemoteClientMidi1PortsAsync(
        winrt::hstring const hostKey,
        winrt::hstring const remoteName,
        winrt::hstring const remoteProductInstanceId,
        bool const hasOwnPorts,
        Midi1PortLayout const ownPorts)
    {
        auto strongThis = get_strong();

        // The host's ports are offered to go back to, and are where a device without ports of its
        // own starts from
        Midi1PortLayout hostPorts{};
        bool foundHost{ false };

        try
        {
            for (auto const& candidate : m_localHosts)
            {
                if (candidate != nullptr && candidate.HostId() == hostKey)
                {
                    hostPorts = winrt::get_self<LocalHostItem>(candidate)->Midi1Ports();
                    foundHost = true;
                    break;
                }
            }
        }
        catch (...)
        {
            foundHost = false;
        }

        if (!foundHost)
        {
            co_return;
        }

        auto layout = std::make_shared<Midi1PortLayout>(hasOwnPorts ? ownPorts : hostPorts);
        auto useHostPorts = std::make_shared<bool>(!hasOwnPorts);

        if (!co_await ShowMidi1PortGroupsDialogAsync(
            res::FormatString(L"Midi1PortGroupsTitleFormat", remoteName.empty() ? res::GetString(L"UnnamedDevice") : remoteName),
            Midi1PortsScope::Device,
            res::GetString(L"Midi1PortGroupsSaveButton"),
            Midi1PortLayoutSummary(hostPorts),
            layout,
            useHostPorts))
        {
            co_return;
        }

        // nothing to send when nothing changed
        if (*useHostPorts == !hasOwnPorts && (*useHostPorts || *layout == ownPorts))
        {
            co_return;
        }

        ChangeRemoteClientMidi1PortsAsync(hostKey, remoteName, remoteProductInstanceId, *useHostPorts, *layout);
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ChangeRemoteClientMidi1PortsAsync(
        winrt::hstring const hostKey,
        winrt::hstring const remoteName,
        winrt::hstring const remoteProductInstanceId,
        bool const useHostPorts,
        Midi1PortLayout const ports)
    {
        winrt::guid hostId{};

        if (!TryParseEntryId(hostKey, hostId))
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        auto const displayName = remoteName.empty() ? res::GetString(L"UnnamedDevice") : remoteName;

        auto const doneMessage = useHostPorts ?
            res::FormatString(L"RemoteClientMidi1PortsResetFormat", displayName) :
            res::FormatString(L"RemoteClientMidi1PortsChangedFormat", displayName);

        co_await winrt::resume_background();

        winrt::hstring message{};

        try
        {
            // The list replaces the host's whole list, so it starts from what the service holds
            // now. A host which couldn't be read must not be written as one with no device
            // settings at all.
            midi2net::MidiNetworkHostRemoteClientSettingsConfig config{ hostId };

            midi2net::MidiNetworkRemoteClientSettings existing{ nullptr };
            bool found{ false };

            if (auto const hosts = midi2net::MidiNetworkTransportManager::GetConfiguredHosts())
            {
                for (auto const& host : hosts)
                {
                    if (host == nullptr || host.HostId() != hostId)
                    {
                        continue;
                    }

                    found = true;

                    if (auto const settings = host.RemoteClientSettings())
                    {
                        for (auto const& entry : settings)
                        {
                            if (entry == nullptr)
                            {
                                continue;
                            }

                            if (IsSameRemoteClientText(entry.RemoteClientName(), remoteName) &&
                                IsSameRemoteClientText(entry.RemoteClientProductInstanceId(), remoteProductInstanceId))
                            {
                                existing = entry;
                            }
                            else
                            {
                                config.RemoteClientSettings().Append(entry);
                            }
                        }
                    }

                    break;
                }
            }

            if (found)
            {
                midi2net::MidiNetworkRemoteClientSettings settings{ remoteName, remoteProductInstanceId };

                // The speed stays as it was: the device's own, or the host's for a device which had
                // nothing of its own or only ports
                if (existing != nullptr)
                {
                    settings.SendSpeedLimit(existing.SendSpeedLimit());
                    settings.ReduceSendSpeedAutomatically(existing.ReduceSendSpeedAutomatically());
                    settings.SkipInBetweenValuesToCatchUp(existing.SkipInBetweenValuesToCatchUp());
                    settings.UseHostSendSpeed(existing.UseHostSendSpeed());
                }
                else
                {
                    settings.UseHostSendSpeed(true);
                }

                if (!useHostPorts)
                {
                    WriteMidi1PortGroupsAsList(ports, settings.FallbackMidi1PortGroups());
                }

                // an entry on the host's speed with no ports of its own would say nothing
                if (!settings.UseHostSendSpeed() || settings.FallbackMidi1PortGroups().Size() > 0)
                {
                    config.RemoteClientSettings().Append(settings);
                }
            }

            message = found ?
                SendAndSaveMidi1PortsChange(config, doneMessage) :
                res::GetString(L"Midi1PortsChangeFailedGeneral");
        }
        catch (...)
        {
            message = res::GetString(L"Midi1PortsChangeFailedGeneral");
        }

        if (queue != nullptr)
        {
            queue.TryEnqueue([weak, message]()
                {
                    if (auto strong = weak.get())
                    {
                        strong->SetLocalStatus(message);
                        strong->RequestRefreshAsync();
                    }
                });
        }
    }
}
