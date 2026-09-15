// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "BackgroundWork.h"
#include "CustomizationHelpers.h"
#include "StringResources.h"

namespace native = ::midisettings;
namespace res = ::midisettings::resources;
namespace custom = ::midiapp::customizations;

namespace winrt::midisettings::implementation
{
    namespace
    {
        // Ranking exists to put the likely answer first, not to decide. Every reason shown is
        // something the customer can check for themselves, and nothing is ever applied on the
        // strength of a score.
        struct CandidateScore
        {
            int32_t Score{ 0 };
            winrt::hstring Reason{};
        };

        std::wstring NormalizedForCompare(winrt::hstring const& value) noexcept
        {
            std::wstring result{ value };

            std::transform(result.begin(), result.end(), result.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });

            // Windows appends a disambiguating suffix to the second device of the same name
            auto const suffix = result.find_last_of(L'(');

            if (suffix != std::wstring::npos && suffix > 0)
            {
                result.erase(suffix);
            }

            while (!result.empty() && result.back() == L' ')
            {
                result.pop_back();
            }

            return result;
        }

        CandidateScore ScoreCandidate(
            _In_ midi2config::MidiServiceEndpointCustomization const& orphan,
            _In_ midi2enum::MidiEndpointDeviceInformation const& candidate,
            _In_ uint32_t const sameVendorProductCount) noexcept
        {
            CandidateScore result{};

            try
            {
                auto const provenance = orphan.Provenance();
                auto const transportInfo = candidate.GetTransportSuppliedInfo();

                if (provenance == nullptr || transportInfo == nullptr)
                {
                    return result;
                }

                if (provenance.UsbVendorId() != 0 &&
                    provenance.UsbVendorId() == transportInfo.VendorId() &&
                    provenance.UsbProductId() == transportInfo.ProductId())
                {
                    // Only decisive when nothing else on the machine is the same model.
                    if (sameVendorProductCount == 1)
                    {
                        result.Score += 100;
                        result.Reason = res::GetString(L"RelinkReasonSameModelOnly");
                    }
                    else
                    {
                        result.Score += 40;
                        result.Reason = res::GetString(L"RelinkReasonSameModel");
                    }
                }

                if (!provenance.UsbSerialNumber().empty() &&
                    provenance.UsbSerialNumber() == transportInfo.SerialNumber())
                {
                    result.Score += 30;
                }

                if (!provenance.TransportSuppliedName().empty() &&
                    NormalizedForCompare(provenance.TransportSuppliedName()) ==
                    NormalizedForCompare(transportInfo.Name()))
                {
                    result.Score += 50;

                    if (result.Reason.empty())
                    {
                        result.Reason = res::GetString(L"RelinkReasonSameName");
                    }
                }
                else if (!provenance.CreatedFor().empty() &&
                    NormalizedForCompare(provenance.CreatedFor()) ==
                    NormalizedForCompare(transportInfo.Name()))
                {
                    result.Score += 45;

                    if (result.Reason.empty())
                    {
                        result.Reason = res::GetString(L"RelinkReasonSameName");
                    }
                }

                // A device with the same number of declared groups is a far better fit for a
                // customization that names ports than one with a different layout.
                auto const declared = midiapp::DeclaredGroups(candidate);

                auto const declaredCount = static_cast<uint32_t>(
                    std::count(declared.begin(), declared.end(), true));

                auto const storedPorts =
                    orphan.Midi1SourcePortCustomNames().Size() +
                    orphan.Midi1DestinationPortCustomNames().Size();

                if (storedPorts > 0 && declaredCount > 0 && declaredCount * 2 >= storedPorts)
                {
                    result.Score += 10;
                }
            }
            CATCH_LOG()

            return result;
        }

        // "ep-linnstrument128.png" reads as "linnstrument128". Not a name the customer typed, but
        // it is the name of the picture they chose, which is what they will recognize.
        winrt::hstring FriendlyNameFromImageFileName(_In_ winrt::hstring const& imageFileName) noexcept
        {
            try
            {
                std::wstring name{ imageFileName };

                if (name.empty())
                {
                    return {};
                }

                auto const extension = name.find_last_of(L'.');

                if (extension != std::wstring::npos)
                {
                    name.erase(extension);
                }

                // the prefix the picture picker adds when it copies a file into the assets folder
                constexpr std::wstring_view prefix{ L"ep-" };

                if (name.size() > prefix.size() &&
                    ::_wcsnicmp(name.c_str(), prefix.data(), prefix.size()) == 0)
                {
                    name.erase(0, prefix.size());
                }

                return winrt::hstring{ name };
            }
            CATCH_LOG()

            return {};
        }
    }


    _Use_decl_annotations_
    void MainWindow::OnCustomizeRelinkClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        // The customize dialog has to close before another can open on the same XamlRoot. The
        // endpoint it was showing becomes the preferred target, which is the whole point of
        // reaching the feature from here.
        m_relinkPreferredEndpointDeviceId = m_customizeEndpointDeviceId;

        CustomizeDialog().Hide();

        ShowRelinkDialogAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnReviewOrphanedCustomizationsClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        m_relinkPreferredEndpointDeviceId = {};

        ShowRelinkDialogAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnOrphanedCustomizationsBarClosed(
        controls::InfoBar const&,
        controls::InfoBarClosedEventArgs const&)
    {
        // Dismissed entries are not raised again. A customer who chooses to live with an orphan
        // should not be asked about it every time the app opens.
        std::wstring stored{ native::AppSettings::Current().DismissedOrphanedCustomizations() };

        for (auto const& storedId : m_currentOrphanStoredIds)
        {
            if (storedId.empty())
            {
                continue;
            }

            if (stored.find(std::wstring{ storedId } + L"|") == std::wstring::npos)
            {
                stored += std::wstring{ storedId } + L"|";
            }
        }

        native::AppSettings::Current().DismissedOrphanedCustomizations(stored);
    }


    foundation::IAsyncAction MainWindow::RefreshOrphanedCustomizationsAsync()
    {
        auto lifetime = get_strong();

        try
        {
            std::vector<winrt::hstring> storedIds{};

            co_await native::RunOnBackgroundAsync(
                [&storedIds]()
                {
                    for (auto const& customization :
                        midi2config::MidiServiceTransportPluginConfigManager::GetEndpointCustomizations())
                    {
                        // An entry holding only default values is not something a customer would
                        // miss, so it is never raised as a prompt.
                        if (!customization.IsOrphaned() || !customization.HasUserContent())
                        {
                            continue;
                        }

                        storedIds.push_back(StoredIdFor(customization));
                    }
                });

            if (m_closing)
            {
                co_return;
            }

            m_currentOrphanStoredIds.clear();

            std::wstring const dismissed{ native::AppSettings::Current().DismissedOrphanedCustomizations() };

            bool hasUndismissed{ false };

            for (auto const& storedId : storedIds)
            {
                m_currentOrphanStoredIds.push_back(storedId);

                if (dismissed.find(std::wstring{ storedId } + L"|") == std::wstring::npos)
                {
                    hasUndismissed = true;
                }
            }

            CustomizeRelinkPanel().Visibility(
                storedIds.empty() ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);

            if (!hasUndismissed)
            {
                co_return;
            }

            OrphanedCustomizationsBar().Message(
                res::FormatString(L"OrphanedCustomizationsMessageFormat",
                    winrt::hstring{ std::to_wstring(storedIds.size()) }));

            OrphanedCustomizationsBar().IsOpen(true);
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to check for saved settings with no matching device.")
    }


    _Use_decl_annotations_
    winrt::hstring MainWindow::StoredIdFor(midi2config::MidiServiceEndpointCustomization const& customization) noexcept
    {
        try
        {
            auto const match = customization.MatchCriteria();

            if (match == nullptr)
            {
                return {};
            }

            return match.DeviceInstanceId().empty() ? match.EndpointDeviceId() : match.DeviceInstanceId();
        }
        CATCH_LOG()

        return {};
    }


    winrt::fire_and_forget MainWindow::ShowRelinkDialogAsync()
    {
        auto lifetime = get_strong();

        try
        {
            m_relinkOrphans.Clear();
            m_relinkCandidates.Clear();
            m_relinkCustomizations.clear();

            RelinkOrphanList().ItemsSource(m_relinkOrphans);
            RelinkCandidateList().ItemsSource(m_relinkCandidates);
            RelinkStatusText().Text({});

            std::vector<midi2config::MidiServiceEndpointCustomization> orphans{};

            co_await native::RunOnBackgroundAsync(
                [&orphans]()
                {
                    for (auto const& customization :
                        midi2config::MidiServiceTransportPluginConfigManager::GetEndpointCustomizations())
                    {
                        if (customization.IsOrphaned() && customization.HasUserContent())
                        {
                            orphans.push_back(customization);
                        }
                    }
                });

            if (m_closing)
            {
                co_return;
            }

            for (auto const& customization : orphans)
            {
                m_relinkCustomizations.push_back(customization);

                auto item = winrt::make<implementation::OrphanedCustomizationItem>();
                auto const self = item.as<implementation::OrphanedCustomizationItem>();

                self->InternalSetCustomization(customization);

                winrt::hstring displayName{ customization.Name() };
                winrt::hstring provenanceText{};

                if (auto const provenance = customization.Provenance())
                {
                    if (displayName.empty())
                    {
                        displayName = provenance.CreatedFor();
                    }

                    if (!provenance.Created().empty())
                    {
                        provenanceText = res::FormatString(L"RelinkSavedOnFormat", provenance.Created());
                    }
                }

                if (displayName.empty())
                {
                    // Entries written before provenance existed have no name at all. The picture
                    // the customer chose is usually the one thing that identifies it, so its file
                    // name is a better label than "Unnamed settings" repeated down the list.
                    displayName = FriendlyNameFromImageFileName(customization.ImageFileName());
                }

                if (displayName.empty())
                {
                    displayName = res::GetString(L"RelinkUnnamedCustomization");
                }

                self->Update(
                    displayName,
                    customization.Description(),
                    custom::DescribeContent(customization),
                    provenanceText,
                    StoredIdFor(customization),
                    {},
                    winrt::hstring{ midiapp::EndpointImageAssets::FullPathForFileName(
                        std::wstring{ customization.ImageFileName() }) });

                m_relinkOrphans.Append(item);
            }

            RelinkStatusText().Text(orphans.empty()
                ? res::GetString(L"RelinkNoOrphans")
                : res::GetString(L"RelinkChooseOrphan"));

            RelinkDialog().IsPrimaryButtonEnabled(false);
            RelinkDialog().IsSecondaryButtonEnabled(false);

            if (!m_relinkOrphans.Size())
            {
                RelinkDialog().XamlRoot(Content().XamlRoot());

                co_await RelinkDialog().ShowAsync();

                co_return;
            }

            RelinkOrphanList().SelectedIndex(0);

            RelinkDialog().XamlRoot(Content().XamlRoot());

            auto const result = co_await RelinkDialog().ShowAsync();

            if (m_closing)
            {
                co_return;
            }

            if (result == controls::ContentDialogResult::Primary)
            {
                co_await ApplyRelinkAsync();
            }
            else if (result == controls::ContentDialogResult::Secondary)
            {
                co_await ForgetSelectedCustomizationAsync();
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to show the saved settings.")
    }


    _Use_decl_annotations_
    void MainWindow::OnRelinkOrphanSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        RefreshRelinkCandidatesAsync();
    }

    _Use_decl_annotations_
    void MainWindow::OnRelinkCandidateSelectionChanged(
        foundation::IInspectable const&,
        controls::SelectionChangedEventArgs const&)
    {
        RelinkDialog().IsPrimaryButtonEnabled(RelinkCandidateList().SelectedIndex() >= 0);
    }


    winrt::fire_and_forget MainWindow::RefreshRelinkCandidatesAsync()
    {
        auto lifetime = get_strong();

        try
        {
            m_relinkCandidates.Clear();

            RelinkDialog().IsPrimaryButtonEnabled(false);

            auto const index = RelinkOrphanList().SelectedIndex();

            RelinkDialog().IsSecondaryButtonEnabled(index >= 0);

            if (index < 0 || static_cast<uint32_t>(index) >= m_relinkCustomizations.size())
            {
                co_return;
            }

            auto const orphan = m_relinkCustomizations[static_cast<size_t>(index)];

            struct Candidate
            {
                winrt::hstring Name;
                winrt::hstring EndpointDeviceId;
                winrt::hstring Reason;
                int32_t Score{ 0 };
                bool AlreadyCustomized{ false };
            };

            std::vector<Candidate> candidates{};

            auto const preferred = m_relinkPreferredEndpointDeviceId;

            co_await native::RunOnBackgroundAsync(
                [&candidates, orphan, preferred]()
                {
                    auto const endpoints = midi2enum::MidiEndpointDeviceInformation::FindAll();

                    // How many endpoints share the stored model. One means a VID and PID match is
                    // decisive; more than one means it is only a hint.
                    uint32_t sameVendorProductCount{ 0 };

                    if (orphan.Provenance() != nullptr && orphan.Provenance().UsbVendorId() != 0)
                    {
                        for (auto const& endpoint : endpoints)
                        {
                            auto const info = endpoint.GetTransportSuppliedInfo();

                            if (info != nullptr &&
                                info.VendorId() == orphan.Provenance().UsbVendorId() &&
                                info.ProductId() == orphan.Provenance().UsbProductId())
                            {
                                sameVendorProductCount++;
                            }
                        }
                    }

                    for (auto const& endpoint : endpoints)
                    {
                        auto const info = endpoint.GetTransportSuppliedInfo();

                        // Only the transport that owns the customization can accept it.
                        if (info == nullptr || info.TransportId() != orphan.TransportId())
                        {
                            continue;
                        }

                        Candidate candidate{};

                        candidate.Name = endpoint.Name();
                        candidate.EndpointDeviceId = endpoint.EndpointDeviceId();

                        auto const existing = custom::FindCustomizationForEndpoint(
                            orphan.TransportId(), endpoint.EndpointDeviceId());

                        candidate.AlreadyCustomized = existing != nullptr && existing.HasUserContent();

                        auto const scored = ScoreCandidate(orphan, endpoint, sameVendorProductCount);

                        candidate.Score = scored.Score;
                        candidate.Reason = scored.Reason;

                        if (candidate.AlreadyCustomized)
                        {
                            candidate.Score -= 20;
                        }

                        if (!preferred.empty() && candidate.EndpointDeviceId == preferred)
                        {
                            candidate.Score += 15;

                            if (candidate.Reason.empty())
                            {
                                candidate.Reason = res::GetString(L"RelinkReasonBeingCustomized");
                            }
                        }

                        candidates.push_back(candidate);
                    }

                    std::stable_sort(candidates.begin(), candidates.end(),
                        [](Candidate const& a, Candidate const& b) { return a.Score > b.Score; });
                });

            if (m_closing)
            {
                co_return;
            }

            for (auto const& candidate : candidates)
            {
                auto item = winrt::make<implementation::RelinkCandidateItem>();

                item.as<implementation::RelinkCandidateItem>()->Update(
                    candidate.Name,
                    candidate.EndpointDeviceId,
                    candidate.Reason,
                    candidate.AlreadyCustomized);

                m_relinkCandidates.Append(item);
            }

            // Two equally good answers is not a recommendation. Leaving both unselected is the
            // honest outcome and makes the customer look at the evidence.
            bool const hasClearWinner =
                candidates.size() == 1 ||
                (candidates.size() > 1 && candidates[0].Score > 0 && candidates[0].Score > candidates[1].Score);

            if (hasClearWinner)
            {
                RelinkCandidateList().SelectedIndex(0);
            }

            RelinkStatusText().Text(candidates.empty()
                ? res::GetString(L"RelinkNoCandidates")
                : res::GetString(hasClearWinner ? L"RelinkChooseCandidate" : L"RelinkNoClearMatch"));
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to list the endpoints this customization could move to.")
    }


    foundation::IAsyncAction MainWindow::ApplyRelinkAsync()
    {
        auto lifetime = get_strong();

        try
        {
            auto const orphanIndex = RelinkOrphanList().SelectedIndex();
            auto const candidateIndex = RelinkCandidateList().SelectedIndex();

            if (orphanIndex < 0 || candidateIndex < 0 ||
                static_cast<uint32_t>(orphanIndex) >= m_relinkCustomizations.size())
            {
                co_return;
            }

            auto const orphan = m_relinkCustomizations[static_cast<size_t>(orphanIndex)];

            auto const candidate = m_relinkCandidates.GetAt(static_cast<uint32_t>(candidateIndex))
                .as<implementation::RelinkCandidateItem>();

            auto const targetEndpointDeviceId = candidate->EndpointDeviceId();
            auto const targetName = candidate->Name();

            bool succeeded{ false };
            bool oldEntryRemoved{ false };
            winrt::hstring errorMessage{};

            co_await native::RunOnBackgroundAsync(
                [&succeeded, &oldEntryRemoved, &errorMessage, orphan, targetEndpointDeviceId]()
                {
                    auto const target = midi2enum::MidiEndpointDeviceInformation::CreateFromEndpointDeviceId(
                        targetEndpointDeviceId);

                    if (target == nullptr)
                    {
                        return;
                    }

                    midi2config::MidiServiceEndpointCustomizationConfig config{ orphan.TransportId() };

                    config.MatchCriteria().EndpointDeviceId(targetEndpointDeviceId);
                    config.MatchCriteria().DeviceInstanceId(target.DeviceInstanceId());

                    custom::CopyInto(orphan, config);

                    config.Provenance(custom::BuildProvenance(targetEndpointDeviceId, orphan.Provenance()));

                    auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(config);

                    if (response == nullptr ||
                        response.Status() != midi2config::MidiServiceConfigResponseStatus::Success)
                    {
                        errorMessage = response == nullptr ? winrt::hstring{} : response.ServiceErrorMessage();
                        return;
                    }

                    auto const saveResponse = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(config);

                    if (saveResponse == nullptr || !saveResponse.Success())
                    {
                        errorMessage = saveResponse == nullptr ? winrt::hstring{} : saveResponse.ErrorMessage();
                        return;
                    }

                    succeeded = true;

                    // Only once the new entry is safely stored. The stored match is the entry's
                    // identity, so a changed match appends rather than replacing: removing first
                    // would risk losing the customization, and not removing at all would leave a
                    // duplicate and a fresh orphan.
                    midi2config::MidiServiceEndpointCustomizationRemovalConfig removal{
                        orphan.TransportId(), orphan.MatchCriteria() };

                    auto const removeResponse = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(removal);
                    auto const removeSave = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(removal);

                    oldEntryRemoved =
                        removeResponse != nullptr &&
                        removeResponse.Status() == midi2config::MidiServiceConfigResponseStatus::Success &&
                        removeSave != nullptr && removeSave.Success();
                });

            if (m_closing)
            {
                co_return;
            }

            if (!succeeded)
            {
                StatusText().Text(errorMessage.empty()
                    ? res::GetString(L"RelinkFailed")
                    : res::FormatString(L"RelinkFailedFormat", errorMessage));

                co_return;
            }

            StatusText().Text(oldEntryRemoved
                ? res::FormatString(L"RelinkSucceededFormat", targetName)
                : res::FormatString(L"RelinkSucceededOldEntryRemainsFormat", targetName));

            RefreshEndpointList();

            co_await RefreshOrphanedCustomizationsAsync();
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to move the saved settings onto the endpoint.")
    }


    foundation::IAsyncAction MainWindow::ForgetSelectedCustomizationAsync()
    {
        auto lifetime = get_strong();

        try
        {
            auto const index = RelinkOrphanList().SelectedIndex();

            if (index < 0 || static_cast<uint32_t>(index) >= m_relinkCustomizations.size())
            {
                co_return;
            }

            auto const orphan = m_relinkCustomizations[static_cast<size_t>(index)];

            bool succeeded{ false };
            winrt::hstring errorMessage{};

            co_await native::RunOnBackgroundAsync(
                [&succeeded, &errorMessage, orphan]()
                {
                    midi2config::MidiServiceEndpointCustomizationRemovalConfig removal{
                        orphan.TransportId(), orphan.MatchCriteria() };

                    auto const response = midi2config::MidiServiceTransportPluginConfigManager::SendUpdate(removal);

                    if (response == nullptr ||
                        response.Status() != midi2config::MidiServiceConfigResponseStatus::Success)
                    {
                        errorMessage = response == nullptr ? winrt::hstring{} : response.ServiceErrorMessage();
                        return;
                    }

                    auto const saveResponse = midi2config::MidiServiceTransportPluginConfigManager::SaveUpdate(removal);

                    succeeded = saveResponse != nullptr && saveResponse.Success();

                    if (!succeeded && saveResponse != nullptr)
                    {
                        errorMessage = saveResponse.ErrorMessage();
                    }
                });

            if (m_closing)
            {
                co_return;
            }

            StatusText().Text(succeeded
                ? res::GetString(L"RelinkForgotten")
                : (errorMessage.empty()
                    ? res::GetString(L"RelinkForgetFailed")
                    : res::FormatString(L"RelinkFailedFormat", errorMessage)));

            if (succeeded)
            {
                co_await RefreshOrphanedCustomizationsAsync();
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Unable to delete the saved settings.")
    }
}
