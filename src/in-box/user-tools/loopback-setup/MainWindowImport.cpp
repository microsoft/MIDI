// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Importing loopbacks from another provider.
//
// Third-party loopback drivers were how this was done before Windows MIDI Services shipped one,
// and a customer who has built a setup around them should not have to retype it. The list is
// everything behind a driver that did not come with Windows, so it is not only loopbacks; the
// dialog says so, and the choice stays with the customer.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "ImportCandidates.h"
#include "StringResources.h"
#include "MidiDefs.h"

namespace native = ::midiloopbacksetup;
namespace res = ::midiloopbacksetup::resources;

namespace winrt::midiloopbacksetup::implementation
{
    namespace
    {
        std::wstring LoweredCopy(_In_ std::wstring_view const value) noexcept
        {
            std::wstring result{ value };

            std::transform(result.begin(), result.end(), result.begin(),
                [](wchar_t const c) { return static_cast<wchar_t>(::towlower(c)); });

            return result;
        }

        // The unique identifier ends up inside a device instance path, so anything which is not
        // a letter or a digit is dropped rather than escaped.
        winrt::hstring GenerateImportUniqueId() noexcept
        {
            try
            {
                std::wstring result{};

                for (auto const ch : winrt::to_hstring(foundation::GuidHelper::CreateNewGuid()))
                {
                    if ((ch >= L'0' && ch <= L'9') ||
                        (ch >= L'A' && ch <= L'Z') ||
                        (ch >= L'a' && ch <= L'z'))
                    {
                        result += ch;

                        if (result.size() >= MIDI_MAX_UMP_ENDPOINT_UNIQUE_ID_CHARACTER_COUNT)
                        {
                            break;
                        }
                    }
                }

                return winrt::hstring{ result };
            }
            catch (...)
            {
                return {};
            }
        }
    }


    size_t MainWindow::BuildImportCandidates() noexcept
    {
        size_t offered{ 0 };

        try
        {
            m_importDevices.Clear();

            auto const candidates = native::FindImportCandidates();

            // A name identifies an endpoint to an application, so the same one twice would be
            // worse than useless. Two ports offered under one name block each other, and a name
            // an existing basic loopback already has is taken as well.
            std::map<std::wstring, int32_t> nameCounts{};

            for (auto const& device : candidates)
            {
                for (auto const& port : device.Ports)
                {
                    nameCounts[LoweredCopy(port.SourceName)]++;
                }
            }

            std::set<std::wstring> existingNames{};

            for (auto const& row : m_basicLoopbacks)
            {
                if (row != nullptr)
                {
                    existingNames.insert(LoweredCopy(std::wstring{ row.NameA() }));
                }
            }

            for (auto const& device : candidates)
            {
                auto deviceItem = winrt::make_self<ImportDeviceItem>();

                deviceItem->InternalInitialize(
                    winrt::hstring{ device.DeviceName },
                    res::FormatString(L"ImportDeviceServiceFormat", winrt::hstring{ device.ServiceName }));

                for (auto const& port : device.Ports)
                {
                    auto const lowered = LoweredCopy(port.SourceName);

                    winrt::hstring conflict{};

                    if (port.SourceName.empty())
                    {
                        conflict = res::GetString(L"ImportConflictNoName");
                    }
                    else if (existingNames.find(lowered) != existingNames.end())
                    {
                        conflict = res::GetString(L"ImportConflictAlreadyExists");
                    }
                    else if (nameCounts[lowered] > 1)
                    {
                        conflict = res::GetString(L"ImportConflictDuplicateName");
                    }

                    // A pair whose two sides are named differently is very unlikely to be a
                    // loopback, so the other name is shown rather than quietly discarded.
                    auto const detail = port.NamesMatch() ?
                        res::FormatString(L"ImportPortDetailFormat", static_cast<int32_t>(port.GroupNumber)) :
                        res::FormatString(L"ImportPortDetailMismatchFormat",
                            static_cast<int32_t>(port.GroupNumber),
                            winrt::hstring{ port.DestinationName });

                    auto portItem = winrt::make_self<ImportPortItem>();

                    portItem->InternalInitialize(
                        winrt::hstring{ port.SourceName },
                        detail,
                        winrt::hstring{ device.DeviceName },
                        conflict);

                    deviceItem->Ports().Append(*portItem);

                    offered++;
                }

                m_importDevices.Append(*deviceItem);
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to look for loopbacks from other providers.")

        return offered;
    }


    void MainWindow::UpdateImportButtonState() noexcept
    {
        try
        {
            bool anySelected{ false };

            for (auto const& device : m_importDevices)
            {
                if (device == nullptr)
                {
                    continue;
                }

                for (auto const& port : device.Ports())
                {
                    if (port != nullptr && port.IsSelected())
                    {
                        anySelected = true;
                        break;
                    }
                }

                if (anySelected)
                {
                    break;
                }
            }

            ImportLoopbacksDialog().IsPrimaryButtonEnabled(anySelected);
        }
        catch (...)
        {
        }
    }


    _Use_decl_annotations_
    void MainWindow::OnImportSelectionChanged(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        UpdateImportButtonState();
    }


    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnImportLoopbacksClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        if (m_openDialog != nullptr)
        {
            co_return;
        }

        bool persist{ false };

        try
        {
            auto const offered = BuildImportCandidates();

            NoImportCandidatesText().Visibility(
                offered == 0 ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            UpdateImportButtonState();

            ImportLoopbacksDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ImportLoopbacksDialog();

            auto const result = co_await ImportLoopbacksDialog().ShowAsync();

            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            auto const persistValue = ImportPersistCheckBox().IsChecked();

            persist = persistValue != nullptr && persistValue.Value();
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return;
        }

        ImportSelectedAsync(persist);
    }


    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ImportSelectedAsync(bool const persist)
    {
        auto strongThis = get_strong();

        // Gathered on the UI thread, because the collections are bound to the dialog.
        std::vector<std::pair<winrt::hstring, winrt::hstring>> selected{};

        try
        {
            for (auto const& device : m_importDevices)
            {
                if (device == nullptr)
                {
                    continue;
                }

                for (auto const& port : device.Ports())
                {
                    if (port != nullptr && port.IsSelected())
                    {
                        selected.push_back({ port.PortName(), port.SourceDeviceName() });
                    }
                }
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to read the import selection.")

        if (selected.empty())
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        co_await winrt::resume_background();

        int32_t created{ 0 };
        int32_t failed{ 0 };
        int32_t notSaved{ 0 };

        for (auto const& [portName, deviceName] : selected)
        {
            try
            {
                midi2bloop::MidiBasicLoopbackEndpointDefinition definition{};

                definition.Name(portName);
                definition.Description(res::FormatString(L"ImportedLoopbackDescriptionFormat", deviceName));
                definition.UniqueId(GenerateImportUniqueId());

                midi2bloop::MidiBasicLoopbackCreationConfig creationConfig{ definition };

                auto const response = midi2bloop::MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);

                if (response == nullptr || !response.Success())
                {
                    failed++;
                    continue;
                }

                created++;

                if (persist && !native::LoopbackConfigFile::Current().MergeSection(
                    native::LoopbackKind::BasicLoopback, creationConfig.ConfigJson()))
                {
                    notSaved++;
                }
            }
            MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to import a loopback.")
        }

        if (queue == nullptr)
        {
            co_return;
        }

        queue.TryEnqueue([weak, created, failed, notSaved]()
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_closing)
                {
                    return;
                }

                strong->RequestRefreshAsync();

                if (failed > 0)
                {
                    strong->SetBasicLoopbackStatus(
                        res::FormatString(L"ImportPartlyFailedFormat", created, failed));
                }
                else if (notSaved > 0)
                {
                    strong->SetBasicLoopbackStatus(
                        res::FormatString(L"ImportNotSavedFormat", notSaved));
                }

                if (created > 0)
                {
                    strong->ShowImportCompleteAsync(created);
                }
            });
    }


    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ShowImportCompleteAsync(int32_t const created)
    {
        auto strongThis = get_strong();

        if (m_openDialog != nullptr)
        {
            co_return;
        }

        try
        {
            ImportCompleteText().Text(res::FormatString(L"ImportCompleteFormat", created));
            ImportCompleteDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ImportCompleteDialog();

            co_await ImportCompleteDialog().ShowAsync();

            m_openDialog = nullptr;
        }
        catch (...)
        {
            m_openDialog = nullptr;
        }
    }
}
