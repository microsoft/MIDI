// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MainWindow.xaml.h"

#include "StringResources.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>

namespace res = ::midisoundmodule::resources;
namespace datatransfer = ::winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::midisoundmodule::implementation
{
    namespace
    {
        // The same files the Add synth dialog lists.
        bool IsSoundBankFile(_In_ std::wstring const& path) noexcept
        {
            return ::CompareStringOrdinal(::PathFindExtensionW(path.c_str()), -1, L".sf2", -1, TRUE) == CSTR_EQUAL;
        }
    }

    _Use_decl_annotations_
    void MainWindow::OnRootDragOver(foundation::IInspectable const&, xaml::DragEventArgs const& args)
    {
        try
        {
            if (args.DataView() != nullptr &&
                args.DataView().Contains(datatransfer::StandardDataFormats::StorageItems()))
            {
                args.AcceptedOperation(datatransfer::DataPackageOperation::Copy);
                args.DragUIOverride().Caption(res::GetString(L"DropCaption"));
                args.DragUIOverride().IsContentVisible(true);
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to handle the drag.")
    }

    _Use_decl_annotations_
    winrt::fire_and_forget MainWindow::OnRootDrop(foundation::IInspectable, xaml::DragEventArgs args)
    {
        auto strong = get_strong();

        try
        {
            if (args.DataView() == nullptr ||
                !args.DataView().Contains(datatransfer::StandardDataFormats::StorageItems()))
            {
                co_return;
            }

            // Without a deferral the drop ends, and its data goes away, at the first suspend.
            auto const deferral = args.GetDeferral();
            auto completeDeferral = wil::scope_exit([&deferral]() noexcept
                {
                    try
                    {
                        deferral.Complete();
                    }
                    catch (...)
                    {
                    }
                });

            auto const items = co_await args.DataView().GetStorageItemsAsync();

            std::vector<std::wstring> paths{};

            if (items != nullptr)
            {
                for (auto const& item : items)
                {
                    if (auto const file = item.try_as<winrt::Windows::Storage::StorageFile>())
                    {
                        std::wstring path{ file.Path() };

                        if (IsSoundBankFile(path))
                        {
                            paths.push_back(std::move(path));
                        }
                    }
                }
            }

            completeDeferral.reset();

            if (paths.empty())
            {
                ShowNotice(res::GetString(L"DropNoSoundBanks"), controls::InfoBarSeverity::Informational);
                co_return;
            }

            for (auto const& path : paths)
            {
                // False at the synth limit, which has already said so.
                if (!AddSynthFromFile(path))
                {
                    break;
                }
            }
        }
        MIDI_SF2SYNTH_CATCH_AND_LOG(L"Unable to add the dropped files.")
    }
}
