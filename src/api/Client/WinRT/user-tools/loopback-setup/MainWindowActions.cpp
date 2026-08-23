// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// Everything the customer can act on. The order is always the same: ask the service to make
// the change, and only write it to the configuration file once the service has agreed. A
// configuration entry for something the service refused would come back on the next restart.

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"
#include "MidiDefs.h"

namespace native = ::midiloopbacksetup;
namespace res = ::midiloopbacksetup::resources;

namespace winrt::midiloopbacksetup::implementation
{
    namespace
    {
        template <typename TItem>
        TItem ItemOf(_In_ foundation::IInspectable const& sender) noexcept
        {
            try
            {
                auto const element = sender.try_as<xaml::FrameworkElement>();

                if (element == nullptr)
                {
                    return nullptr;
                }

                return element.DataContext().try_as<TItem>();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        winrt::hstring TextOf(_In_ controls::TextBox const& box) noexcept
        {
            try
            {
                std::wstring value{ box.Text() };

                auto const first = value.find_first_not_of(L" \t\r\n");

                if (first == std::wstring::npos)
                {
                    return {};
                }

                auto const last = value.find_last_not_of(L" \t\r\n");

                return winrt::hstring{ value.substr(first, last - first + 1) };
            }
            catch (...)
            {
                return {};
            }
        }

        bool IsCheckBoxChecked(_In_ controls::CheckBox const& box) noexcept
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

        // The unique identifier ends up inside a device instance path, so only letters and
        // digits survive. Anything else is dropped rather than escaped, which is what the
        // service would do to it anyway.
        winrt::hstring CleanUniqueId(_In_ winrt::hstring const& source) noexcept
        {
            try
            {
                std::wstring result{};

                for (auto const ch : source)
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

        winrt::hstring GenerateUniqueId() noexcept
        {
            try
            {
                return CleanUniqueId(winrt::to_hstring(foundation::GuidHelper::CreateNewGuid()));
            }
            catch (...)
            {
                return {};
            }
        }

        winrt::hstring Lowered(_In_ winrt::hstring const& value) noexcept
        {
            try
            {
                std::wstring copy{ value };

                std::transform(copy.begin(), copy.end(), copy.begin(),
                    [](wchar_t c) { return static_cast<wchar_t>(::towlower(c)); });

                return winrt::hstring{ copy };
            }
            catch (...)
            {
                return value;
            }
        }

        bool TryParseAssociationId(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
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
    }


    foundation::IAsyncOperation<bool> MainWindow::ConfirmAsync(winrt::hstring const& title, winrt::hstring const& message)
    {
        // only one dialog can be open at a time, and a second ShowAsync throws
        if (m_openDialog != nullptr)
        {
            co_return false;
        }

        try
        {
            ConfirmDialog().Title(winrt::box_value(title));
            ConfirmDialogText().Text(message);
            ConfirmDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = ConfirmDialog();

            auto const result = co_await ConfirmDialog().ShowAsync();

            m_openDialog = nullptr;

            co_return result == controls::ContentDialogResult::Primary;
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return false;
        }
    }


    // ------------------------------------------------------------------------------------
    // creating a MIDI 2.0 style loopback pair
    // ------------------------------------------------------------------------------------

    void MainWindow::UpdateCreateLoopbackButtonState() noexcept
    {
        try
        {
            auto const name = TextOf(LoopbackNameTextBox());
            auto const nameA = TextOf(LoopbackNameATextBox());
            auto const nameB = TextOf(LoopbackNameBTextBox());
            auto const uniqueId = CleanUniqueId(TextOf(LoopbackUniqueIdTextBox()));

            winrt::hstring problem{};

            // Either the one name, from which both sides are derived, or both sides named
            // individually. One side named on its own leaves the other without a name.
            if (name.empty() && (nameA.empty() || nameB.empty()))
            {
                problem = res::GetString(L"ValidationLoopbackNameRequired");
            }
            else if (uniqueId.empty())
            {
                problem = res::GetString(L"ValidationUniqueIdRequired");
            }
            else if (midi2loop::MidiLoopbackManager::DoesLoopbackAExist(uniqueId) ||
                     midi2loop::MidiLoopbackManager::DoesLoopbackBExist(uniqueId))
            {
                problem = res::GetString(L"ValidationUniqueIdInUse");
            }

            CreateLoopbackStatusText().Text(problem);
            CreateLoopbackDialog().IsPrimaryButtonEnabled(problem.empty());
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnCreateLoopbackFieldChanged(
        foundation::IInspectable const&,
        controls::TextChangedEventArgs const&)
    {
        UpdateCreateLoopbackButtonState();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCreateLoopbackClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        if (m_openDialog != nullptr)
        {
            co_return;
        }

        try
        {
            LoopbackNameTextBox().Text(L"");
            LoopbackDescriptionTextBox().Text(L"");
            LoopbackNameATextBox().Text(L"");
            LoopbackNameBTextBox().Text(L"");
            LoopbackDescriptionATextBox().Text(L"");
            LoopbackDescriptionBTextBox().Text(L"");
            LoopbackUniqueIdTextBox().Text(GenerateUniqueId());

            m_pendingLoopbackImage = L"";
            ShowChosenImage(
                LoopbackImagePreview(), LoopbackImageNameText(), LoopbackClearImageButton(), L"");

            UpdateCreateLoopbackButtonState();

            CreateLoopbackDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = CreateLoopbackDialog();

            auto const result = co_await CreateLoopbackDialog().ShowAsync();

            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return;
        }

        midi2loop::MidiLoopbackCreationConfig creationConfig{ nullptr };
        bool persist{ false };

        try
        {
            auto const baseName = TextOf(LoopbackNameTextBox());
            auto const baseDescription = TextOf(LoopbackDescriptionTextBox());

            auto const nameA = TextOf(LoopbackNameATextBox());
            auto const nameB = TextOf(LoopbackNameBTextBox());

            auto const uniqueId = CleanUniqueId(TextOf(LoopbackUniqueIdTextBox()));

            midi2loop::MidiLoopbackEndpointDefinition definitionA{};
            midi2loop::MidiLoopbackEndpointDefinition definitionB{};

            if (!nameA.empty() && !nameB.empty())
            {
                definitionA.Name(nameA);
                definitionB.Name(nameB);

                definitionA.Description(TextOf(LoopbackDescriptionATextBox()));
                definitionB.Description(TextOf(LoopbackDescriptionBTextBox()));
            }
            else
            {
                definitionA.Name(res::FormatString(L"LoopbackSideANameFormat", baseName));
                definitionB.Name(res::FormatString(L"LoopbackSideBNameFormat", baseName));

                definitionA.Description(baseDescription);
                definitionB.Description(baseDescription);
            }

            // Both sides share the identifier. The service prefixes them differently, so the
            // two device instance paths still come out distinct.
            definitionA.UniqueId(uniqueId);
            definitionB.UniqueId(uniqueId);

            // one picture for the pair, on both sides so either endpoint shows it
            definitionA.ImageFileName(m_pendingLoopbackImage);
            definitionB.ImageFileName(m_pendingLoopbackImage);

            creationConfig = midi2loop::MidiLoopbackCreationConfig{ definitionA, definitionB };

            persist = IsCheckBoxChecked(LoopbackPersistCheckBox());
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to prepare the new loopback.")

        if (creationConfig == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        co_await winrt::resume_background();

        bool created{ false };
        bool saved{ false };
        winrt::hstring errorMessage{};

        try
        {
            auto const response = midi2loop::MidiLoopbackManager::CreateTransientLoopback(creationConfig);

            created = response != nullptr && response.Success();

            if (!created)
            {
                errorMessage = response == nullptr ? winrt::hstring{} : response.ErrorMessage();
            }
            else if (persist)
            {
                saved = native::LoopbackConfigFile::Current().MergeSection(creationConfig.ConfigJson());

                if (!saved)
                {
                    errorMessage = native::LoopbackConfigFile::Current().LastErrorMessage();
                }
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to create the loopback.")

        if (queue == nullptr)
        {
            co_return;
        }

        queue.TryEnqueue([weak, created, persist, saved, errorMessage]()
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_closing)
                {
                    return;
                }

                if (!created)
                {
                    strong->SetLoopbackStatus(errorMessage.empty() ?
                        res::GetString(L"CreateLoopbackFailed") :
                        res::FormatString(L"CreateLoopbackFailedFormat", errorMessage));
                }
                else if (persist && !saved)
                {
                    strong->SetLoopbackStatus(res::FormatString(L"LoopbackCreatedNotSavedFormat", errorMessage));
                }
                else
                {
                    strong->SetLoopbackStatus(res::GetString(L"LoopbackCreated"));
                }

                strong->RequestRefreshAsync();
            });
    }


    // ------------------------------------------------------------------------------------
    // creating a basic loopback
    // ------------------------------------------------------------------------------------

    void MainWindow::UpdateCreateBasicLoopbackButtonState() noexcept
    {
        try
        {
            auto const name = TextOf(BasicLoopbackNameTextBox());
            auto const uniqueId = CleanUniqueId(TextOf(BasicLoopbackUniqueIdTextBox()));

            winrt::hstring problem{};

            if (name.empty())
            {
                problem = res::GetString(L"ValidationBasicLoopbackNameRequired");
            }
            else if (uniqueId.empty())
            {
                problem = res::GetString(L"ValidationUniqueIdRequired");
            }
            else if (midi2bloop::MidiBasicLoopbackManager::DoesLoopbackExist(uniqueId))
            {
                problem = res::GetString(L"ValidationUniqueIdInUse");
            }

            CreateBasicLoopbackStatusText().Text(problem);
            CreateBasicLoopbackDialog().IsPrimaryButtonEnabled(problem.empty());
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnCreateBasicLoopbackFieldChanged(
        foundation::IInspectable const&,
        controls::TextChangedEventArgs const&)
    {
        UpdateCreateBasicLoopbackButtonState();
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnCreateBasicLoopbackClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        auto strongThis = get_strong();

        if (m_openDialog != nullptr)
        {
            co_return;
        }

        try
        {
            BasicLoopbackNameTextBox().Text(L"");
            BasicLoopbackDescriptionTextBox().Text(L"");
            BasicLoopbackUniqueIdTextBox().Text(GenerateUniqueId());

            m_pendingBasicLoopbackImage = L"";
            ShowChosenImage(
                BasicLoopbackImagePreview(), BasicLoopbackImageNameText(), BasicLoopbackClearImageButton(), L"");

            UpdateCreateBasicLoopbackButtonState();

            CreateBasicLoopbackDialog().XamlRoot(Content().XamlRoot());

            m_openDialog = CreateBasicLoopbackDialog();

            auto const result = co_await CreateBasicLoopbackDialog().ShowAsync();

            m_openDialog = nullptr;

            if (result != controls::ContentDialogResult::Primary)
            {
                co_return;
            }
        }
        catch (...)
        {
            m_openDialog = nullptr;

            co_return;
        }

        midi2bloop::MidiBasicLoopbackCreationConfig creationConfig{ nullptr };
        bool persist{ false };

        try
        {
            midi2bloop::MidiBasicLoopbackEndpointDefinition definition{};

            definition.Name(TextOf(BasicLoopbackNameTextBox()));
            definition.Description(TextOf(BasicLoopbackDescriptionTextBox()));
            definition.UniqueId(CleanUniqueId(TextOf(BasicLoopbackUniqueIdTextBox())));
            definition.ImageFileName(m_pendingBasicLoopbackImage);

            creationConfig = midi2bloop::MidiBasicLoopbackCreationConfig{ definition };

            persist = IsCheckBoxChecked(BasicLoopbackPersistCheckBox());
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to prepare the new basic loopback.")

        if (creationConfig == nullptr)
        {
            co_return;
        }

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        co_await winrt::resume_background();

        bool created{ false };
        bool saved{ false };
        winrt::hstring errorMessage{};

        try
        {
            auto const response = midi2bloop::MidiBasicLoopbackManager::CreateTransientLoopback(creationConfig);

            created = response != nullptr && response.Success();

            if (!created)
            {
                errorMessage = response == nullptr ? winrt::hstring{} : response.ErrorMessage();
            }
            else if (persist)
            {
                saved = native::LoopbackConfigFile::Current().MergeSection(creationConfig.ConfigJson());

                if (!saved)
                {
                    errorMessage = native::LoopbackConfigFile::Current().LastErrorMessage();
                }
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to create the basic loopback.")

        if (queue == nullptr)
        {
            co_return;
        }

        queue.TryEnqueue([weak, created, persist, saved, errorMessage]()
            {
                auto strong = weak.get();

                if (strong == nullptr || strong->m_closing)
                {
                    return;
                }

                if (!created)
                {
                    strong->SetBasicLoopbackStatus(errorMessage.empty() ?
                        res::GetString(L"CreateLoopbackFailed") :
                        res::FormatString(L"CreateLoopbackFailedFormat", errorMessage));
                }
                else if (persist && !saved)
                {
                    strong->SetBasicLoopbackStatus(res::FormatString(L"LoopbackCreatedNotSavedFormat", errorMessage));
                }
                else
                {
                    strong->SetBasicLoopbackStatus(res::GetString(L"LoopbackCreated"));
                }

                strong->RequestRefreshAsync();
            });
    }


    // ------------------------------------------------------------------------------------
    // muting
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnToggleMuteClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const&)
    {
        auto const item = ItemOf<midiloopbacksetup::LoopbackItem>(sender);

        if (item != nullptr)
        {
            SetMutedAsync(item, native::LoopbackKind::Loopback, !item.IsMuted());
        }

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnToggleBasicMuteClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const&)
    {
        auto const item = ItemOf<midiloopbacksetup::LoopbackItem>(sender);

        if (item != nullptr)
        {
            SetMutedAsync(item, native::LoopbackKind::BasicLoopback, !item.IsMuted());
        }

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::SetMutedAsync(
        midiloopbacksetup::LoopbackItem const item,
        native::LoopbackKind const kind,
        bool const mute)
    {
        auto strongThis = get_strong();

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        winrt::guid associationId{};

        if (!TryParseAssociationId(item.AssociationId(), associationId))
        {
            co_return;
        }

        auto const isPersisted = item.IsPersisted();
        auto const associationKey = item.AssociationId();

        item.IsBusy(true);

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        co_await winrt::resume_background();

        bool applied{ false };
        bool saved{ true };
        winrt::hstring errorMessage{};

        try
        {
            if (kind == native::LoopbackKind::BasicLoopback)
            {
                auto const response = mute ?
                    midi2bloop::MidiBasicLoopbackManager::MuteLoopback(associationId) :
                    midi2bloop::MidiBasicLoopbackManager::UnmuteLoopback(associationId);

                applied = response != nullptr && response.Success();
                errorMessage = response == nullptr ? winrt::hstring{} : response.ErrorMessage();
            }
            else
            {
                auto const response = mute ?
                    midi2loop::MidiLoopbackManager::MuteLoopback(associationId) :
                    midi2loop::MidiLoopbackManager::UnmuteLoopback(associationId);

                applied = response != nullptr && response.Success();
                errorMessage = response == nullptr ? winrt::hstring{} : response.ErrorMessage();
            }

            // Only a loopback the file knows about has somewhere to record this. A transient
            // one is muted live and that is all it can be.
            if (applied && isPersisted)
            {
                saved = native::LoopbackConfigFile::Current().SetMuted(kind, associationKey, mute);

                if (!saved)
                {
                    errorMessage = native::LoopbackConfigFile::Current().LastErrorMessage();
                }
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to change the muted state.")

        if (queue == nullptr)
        {
            co_return;
        }

        queue.TryEnqueue([weak, item, kind, mute, applied, saved, errorMessage]()
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                if (item != nullptr)
                {
                    item.IsBusy(false);
                }

                if (strong->m_closing)
                {
                    return;
                }

                winrt::hstring status{};

                if (!applied)
                {
                    status = errorMessage.empty() ?
                        res::GetString(L"MuteFailed") :
                        res::FormatString(L"MuteFailedFormat", errorMessage);
                }
                else if (!saved)
                {
                    status = res::FormatString(L"MuteNotSavedFormat", errorMessage);
                }
                else
                {
                    status = res::GetString(mute ? L"LoopbackMuted" : L"LoopbackUnmuted");
                }

                if (kind == native::LoopbackKind::BasicLoopback)
                {
                    strong->SetBasicLoopbackStatus(status);
                }
                else
                {
                    strong->SetLoopbackStatus(status);
                }

                strong->RequestRefreshAsync();
            });
    }


    // ------------------------------------------------------------------------------------
    // deleting
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDeleteLoopbackClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const&)
    {
        auto const item = ItemOf<midiloopbacksetup::LoopbackItem>(sender);

        if (item != nullptr)
        {
            DeleteAsync(item, native::LoopbackKind::Loopback);
        }

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnDeleteBasicLoopbackClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const&)
    {
        auto const item = ItemOf<midiloopbacksetup::LoopbackItem>(sender);

        if (item != nullptr)
        {
            DeleteAsync(item, native::LoopbackKind::BasicLoopback);
        }

        co_return;
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::DeleteAsync(
        midiloopbacksetup::LoopbackItem const item,
        native::LoopbackKind const kind)
    {
        auto strongThis = get_strong();

        if (item == nullptr || item.IsBusy())
        {
            co_return;
        }

        winrt::guid associationId{};

        if (!TryParseAssociationId(item.AssociationId(), associationId))
        {
            co_return;
        }

        auto const confirmed = co_await ConfirmAsync(
            res::GetString(L"ConfirmDeleteTitle"),
            res::FormatString(L"ConfirmDeleteMessageFormat", item.DisplayName()));

        if (!confirmed)
        {
            co_return;
        }

        auto const associationKey = item.AssociationId();

        item.IsBusy(true);

        auto weak = get_weak();
        auto queue = DispatcherQueue();

        co_await winrt::resume_background();

        bool removed{ false };
        bool saved{ true };
        winrt::hstring errorMessage{};

        try
        {
            if (kind == native::LoopbackKind::BasicLoopback)
            {
                midi2bloop::MidiBasicLoopbackRemovalConfig removalConfig{ associationId };

                auto const response = midi2bloop::MidiBasicLoopbackManager::RemoveTransientLoopback(removalConfig);

                removed = response != nullptr && response.Success();
                errorMessage = response == nullptr ? winrt::hstring{} : response.ErrorMessage();
            }
            else
            {
                midi2loop::MidiLoopbackRemovalConfig removalConfig{ associationId };

                auto const response = midi2loop::MidiLoopbackManager::RemoveTransientLoopback(removalConfig);

                removed = response != nullptr && response.Success();
                errorMessage = response == nullptr ? winrt::hstring{} : response.ErrorMessage();
            }

            if (removed)
            {
                saved = native::LoopbackConfigFile::Current().RemoveEntry(kind, associationKey);

                if (!saved)
                {
                    errorMessage = native::LoopbackConfigFile::Current().LastErrorMessage();
                }
            }
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to remove the loopback.")

        if (queue == nullptr)
        {
            co_return;
        }

        queue.TryEnqueue([weak, item, kind, removed, saved, errorMessage]()
            {
                auto strong = weak.get();

                if (strong == nullptr)
                {
                    return;
                }

                if (item != nullptr)
                {
                    item.IsBusy(false);
                }

                if (strong->m_closing)
                {
                    return;
                }

                winrt::hstring status{};

                if (!removed)
                {
                    status = errorMessage.empty() ?
                        res::GetString(L"DeleteFailed") :
                        res::FormatString(L"DeleteFailedFormat", errorMessage);
                }
                else if (!saved)
                {
                    // The loopback is gone from the running service but the file still asks
                    // for it, so it would come back. Saying so beats letting it reappear.
                    status = res::FormatString(L"DeleteNotSavedFormat", errorMessage);
                }
                else
                {
                    status = res::GetString(L"LoopbackDeleted");
                }

                if (kind == native::LoopbackKind::BasicLoopback)
                {
                    strong->SetBasicLoopbackStatus(status);
                }
                else
                {
                    strong->SetLoopbackStatus(status);
                }

                strong->RequestRefreshAsync();
            });
    }


    // ------------------------------------------------------------------------------------
    // endpoint pictures
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::ShowChosenImage(
        controls::Image const& preview,
        controls::TextBlock const& caption,
        controls::Button const& clearButton,
        winrt::hstring const& fileName) noexcept
    {
        try
        {
            if (fileName.empty())
            {
                preview.Source(nullptr);
                caption.Text(res::GetString(L"NoImageChosenText"));
                clearButton.IsEnabled(false);

                return;
            }

            auto const path = midiapp::EndpointImageAssets::FullPathForFileName(std::wstring{ fileName });

            if (!path.empty())
            {
                foundation::Uri const uri{ winrt::hstring{ path } };

                if (midiapp::EndpointImageAssets::IsScalableVector(std::wstring{ fileName }))
                {
                    preview.Source(xaml::Media::Imaging::SvgImageSource{ uri });
                }
                else
                {
                    preview.Source(xaml::Media::Imaging::BitmapImage{ uri });
                }
            }

            caption.Text(fileName);
            clearButton.IsEnabled(true);
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to show the chosen picture.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::ChooseImageAsync(bool const forBasicLoopback)
    {
        auto strongThis = get_strong();

        try
        {
            // an unpackaged app has no implicit window to parent the picker to
            HWND windowHandle{ nullptr };

            if (auto const nativeWindow = try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(nativeWindow->get_WindowHandle(&windowHandle));
            }

            if (windowHandle == nullptr)
            {
                co_return;
            }

            auto weak = get_weak();
            auto queue = DispatcherQueue();

            if (queue == nullptr)
            {
                co_return;
            }

            // Runs its own modal loop and returns the answer directly. See ShowPicker for why
            // this is not the WinRT picker.
            auto const sourcePath = midiapp::EndpointImageAssets::ShowPicker(windowHandle);

            if (sourcePath.empty())
            {
                co_return;
            }

            // the copy touches the disk, so it does not belong on the UI thread
            co_await winrt::resume_background();

            std::wstring errorMessage{};

            auto const storedName = midiapp::EndpointImageAssets::CopyIntoFolder(sourcePath, errorMessage);

            queue.TryEnqueue([weak, forBasicLoopback, storedName, errorMessage]()
                {
                    auto strong = weak.get();

                    if (strong == nullptr || strong->m_closing)
                    {
                        return;
                    }

                    if (storedName.empty())
                    {
                        auto const message = res::GetString(
                            errorMessage == L"folder" ? L"ImageFolderError" :
                            errorMessage == L"name" ? L"ImageNameError" : L"ImageCopyError");

                        if (forBasicLoopback)
                        {
                            strong->CreateBasicLoopbackStatusText().Text(message);
                        }
                        else
                        {
                            strong->CreateLoopbackStatusText().Text(message);
                        }

                        return;
                    }

                    if (forBasicLoopback)
                    {
                        strong->m_pendingBasicLoopbackImage = winrt::hstring{ storedName };

                        strong->ShowChosenImage(
                            strong->BasicLoopbackImagePreview(),
                            strong->BasicLoopbackImageNameText(),
                            strong->BasicLoopbackClearImageButton(),
                            strong->m_pendingBasicLoopbackImage);
                    }
                    else
                    {
                        strong->m_pendingLoopbackImage = winrt::hstring{ storedName };

                        strong->ShowChosenImage(
                            strong->LoopbackImagePreview(),
                            strong->LoopbackImageNameText(),
                            strong->LoopbackClearImageButton(),
                            strong->m_pendingLoopbackImage);
                    }
                });
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to choose a picture.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnChooseLoopbackImageClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        ChooseImageAsync(false);

        co_return;
    }

    _Use_decl_annotations_
    void MainWindow::OnClearLoopbackImageClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        m_pendingLoopbackImage = L"";

        ShowChosenImage(
            LoopbackImagePreview(), LoopbackImageNameText(), LoopbackClearImageButton(), L"");
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnChooseBasicLoopbackImageClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        ChooseImageAsync(true);

        co_return;
    }

    _Use_decl_annotations_
    void MainWindow::OnClearBasicLoopbackImageClick(
        foundation::IInspectable const&,
        xaml::RoutedEventArgs const&)
    {
        m_pendingBasicLoopbackImage = L"";

        ShowChosenImage(
            BasicLoopbackImagePreview(), BasicLoopbackImageNameText(), BasicLoopbackClearImageButton(), L"");
    }


    // ------------------------------------------------------------------------------------
    // reordering
    // ------------------------------------------------------------------------------------

    _Use_decl_annotations_
    void MainWindow::PersistDisplayOrder(
        native::LoopbackKind const kind,
        collections::IObservableVector<midiloopbacksetup::LoopbackItem> const& rows) noexcept
    {
        try
        {
            std::vector<winrt::hstring> ordered{};

            auto& sessionOrder = kind == native::LoopbackKind::BasicLoopback ?
                m_basicLoopbackOrder : m_loopbackOrder;

            sessionOrder.clear();

            int32_t position{ 0 };

            for (auto const& row : rows)
            {
                if (row == nullptr)
                {
                    continue;
                }

                row.DisplayOrder(position);

                sessionOrder.insert_or_assign(std::wstring{ row.AssociationId() }, position);
                ordered.push_back(row.AssociationId());

                position++;
            }

            // The file write is best effort: a loopback which was never saved has no entry to
            // record a position in, and the session order still holds until the tool is closed.
            native::LoopbackConfigFile::Current().SetDisplayOrder(kind, ordered);
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to save the new order.")
    }

    _Use_decl_annotations_
    void MainWindow::OnLoopbacksDragStarting(
        foundation::IInspectable const&,
        controls::DragItemsStartingEventArgs const&)
    {
        // a refresh landing mid drag rearranges the collection under the customer's hand
        m_reordering = true;
    }

    _Use_decl_annotations_
    void MainWindow::OnBasicLoopbacksDragStarting(
        foundation::IInspectable const&,
        controls::DragItemsStartingEventArgs const&)
    {
        m_reordering = true;
    }

    _Use_decl_annotations_
    void MainWindow::OnLoopbacksReordered(
        foundation::IInspectable const&,
        controls::DragItemsCompletedEventArgs const&)
    {
        m_reordering = false;

        PersistDisplayOrder(native::LoopbackKind::Loopback, m_loopbacks);
    }

    _Use_decl_annotations_
    void MainWindow::OnBasicLoopbacksReordered(
        foundation::IInspectable const&,
        controls::DragItemsCompletedEventArgs const&)
    {
        m_reordering = false;

        PersistDisplayOrder(native::LoopbackKind::BasicLoopback, m_basicLoopbacks);
    }

    _Use_decl_annotations_
    bool MainWindow::MoveFocusedRow(
        controls::ListView const& list,
        collections::IObservableVector<midiloopbacksetup::LoopbackItem> const& rows,
        native::LoopbackKind const kind,
        int32_t const delta) noexcept
    {
        try
        {
            if (list == nullptr || rows == nullptr)
            {
                return false;
            }

            auto focused = xaml::Input::FocusManager::GetFocusedElement(list.XamlRoot())
                .try_as<xaml::DependencyObject>();

            // focus can be on a button inside the row, so walk out to the container
            controls::ListViewItem container{ nullptr };

            while (focused != nullptr)
            {
                container = focused.try_as<controls::ListViewItem>();

                if (container != nullptr)
                {
                    break;
                }

                focused = xaml::Media::VisualTreeHelper::GetParent(focused);
            }

            if (container == nullptr)
            {
                return false;
            }

            auto const from = list.IndexFromContainer(container);

            if (from < 0)
            {
                return false;
            }

            auto const to = from + delta;

            if (to < 0 || to >= static_cast<int32_t>(rows.Size()))
            {
                return false;
            }

            auto const row = rows.GetAt(static_cast<uint32_t>(from));

            rows.RemoveAt(static_cast<uint32_t>(from));
            rows.InsertAt(static_cast<uint32_t>(to), row);

            PersistDisplayOrder(kind, rows);

            // the container is a new one after the move, so focus has to be put back by index
            if (auto moved = list.ContainerFromIndex(to).try_as<controls::ListViewItem>())
            {
                moved.Focus(xaml::FocusState::Keyboard);
            }

            return true;
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to move the loopback.")

        return false;
    }

    _Use_decl_annotations_
    void MainWindow::OnMoveLoopbackUp(
        xaml::Input::KeyboardAccelerator const&,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(MoveFocusedRow(LoopbacksListView(), m_loopbacks, native::LoopbackKind::Loopback, -1));
    }

    _Use_decl_annotations_
    void MainWindow::OnMoveLoopbackDown(
        xaml::Input::KeyboardAccelerator const&,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(MoveFocusedRow(LoopbacksListView(), m_loopbacks, native::LoopbackKind::Loopback, 1));
    }

    _Use_decl_annotations_
    void MainWindow::OnMoveBasicLoopbackUp(
        xaml::Input::KeyboardAccelerator const&,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(MoveFocusedRow(BasicLoopbacksListView(), m_basicLoopbacks, native::LoopbackKind::BasicLoopback, -1));
    }

    _Use_decl_annotations_
    void MainWindow::OnMoveBasicLoopbackDown(
        xaml::Input::KeyboardAccelerator const&,
        xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        args.Handled(MoveFocusedRow(BasicLoopbacksListView(), m_basicLoopbacks, native::LoopbackKind::BasicLoopback, 1));
    }
}
