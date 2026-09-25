// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Full screen. There is no bar: everything the chrome offered lives behind one 34 px button in
// whichever corner the layout names, fading back to a quarter after a few seconds so it can be
// found without being in the way.
//
// Panic is always the last item in that flyout, always the same color, and never moves. The
// moment it is needed is the moment nobody wants to go looking for it.

#include "pch.h"
#include "RuntimeWindow.xaml.h"

#include "StringResources.h"

namespace resources = ::midiglass::resources;

namespace winrt::midiglass::implementation
{
    namespace
    {
        // Long enough to read, short enough that it is gone before anybody plays a note.
        constexpr int32_t ToastVisibleMilliseconds = 2500;

        // How long the corner button stays bright before it settles back.
        constexpr int32_t CornerFadeDelayMilliseconds = 4000;

        constexpr double CornerRestOpacity = 0.25;
    }

    void RuntimeWindow::ApplyCornerButtonPlacement()
    {
        try
        {
            auto const corner = m_document.FullScreenButtonCorner;

            CornerButton().HorizontalAlignment(
                corner == glass::ScreenCorner::TopLeft || corner == glass::ScreenCorner::BottomLeft
                    ? xaml::HorizontalAlignment::Left
                    : xaml::HorizontalAlignment::Right);

            CornerButton().VerticalAlignment(
                corner == glass::ScreenCorner::TopLeft || corner == glass::ScreenCorner::TopRight
                    ? xaml::VerticalAlignment::Top
                    : xaml::VerticalAlignment::Bottom);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to place the corner button.")
    }

    void RuntimeWindow::ShowFullScreenChrome()
    {
        try
        {
            CornerButton().Visibility(m_fullScreen ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);

            if (!m_fullScreen)
            {
                if (m_cornerFadeTimer != nullptr)
                {
                    m_cornerFadeTimer.Stop();
                }

                if (m_toastTimer != nullptr)
                {
                    m_toastTimer.Stop();
                }

                EscapeToast().Visibility(xaml::Visibility::Collapsed);
                EscapeToast().Opacity(0.0);

                return;
            }

            ApplyCornerButtonPlacement();

            CornerButton().Opacity(1.0);

            if (m_cornerFadeTimer == nullptr)
            {
                m_cornerFadeTimer = xaml::DispatcherTimer{};
                m_cornerFadeTimer.Interval(std::chrono::milliseconds{ CornerFadeDelayMilliseconds });

                auto weak = get_weak();

                m_cornerFadeTimer.Tick([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_cornerFadeTimer.Stop();

                            if (strong->m_fullScreen && !strong->m_cornerHovered)
                            {
                                strong->CornerButton().Opacity(CornerRestOpacity);
                            }
                        }
                    });
            }

            m_cornerFadeTimer.Start();

            ShowEscapeToast();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the full screen chrome.")
    }

    void RuntimeWindow::ShowEscapeToast()
    {
        try
        {
            EscapeToastText().Text(resources::GetString(L"FullScreenToast"));

            EscapeToast().Visibility(xaml::Visibility::Visible);
            EscapeToast().Opacity(1.0);

            if (m_toastTimer == nullptr)
            {
                m_toastTimer = xaml::DispatcherTimer{};
                m_toastTimer.Interval(std::chrono::milliseconds{ ToastVisibleMilliseconds });

                auto weak = get_weak();

                m_toastTimer.Tick([weak](auto&&, auto&&)
                    {
                        if (auto strong = weak.get())
                        {
                            strong->m_toastTimer.Stop();

                            // Hidden rather than only faded: an invisible element over a control
                            // surface is still an element a pointer has to be routed around.
                            strong->EscapeToast().Opacity(0.0);
                            strong->EscapeToast().Visibility(xaml::Visibility::Collapsed);
                        }
                    });
            }

            m_toastTimer.Start();
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to show the full screen note.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnCornerButtonPointerEntered(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_cornerHovered = true;

        try
        {
            CornerButton().Opacity(1.0);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to light the corner button.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnCornerButtonPointerExited(
        foundation::IInspectable const& sender,
        xaml::Input::PointerRoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        m_cornerHovered = false;

        try
        {
            if (m_fullScreen && m_cornerFadeTimer != nullptr)
            {
                m_cornerFadeTimer.Start();
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to settle the corner button.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnCornerFlyoutOpening(
        foundation::IInspectable const& sender,
        foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        try
        {
            CornerButton().Opacity(1.0);

            // One item per page, rebuilt each time, because a page can be added while the
            // layout is running in another window.
            CornerPagesItem().Items().Clear();

            for (size_t index = 0; index < m_document.Pages.size(); ++index)
            {
                controls::MenuFlyoutItem item{};

                item.Text(winrt::hstring{ m_document.Pages[index].Name });
                item.Tag(box_value(winrt::hstring{ L"page" + std::to_wstring(index) }));

                auto weak = get_weak();

                item.Click([weak](auto&& clicked, auto&&)
                    {
                        auto strong = weak.get();

                        if (strong == nullptr)
                        {
                            return;
                        }

                        strong->OnCornerMenuClick(clicked, xaml::RoutedEventArgs{});
                    });

                xaml::Automation::AutomationProperties::SetName(item, item.Text());

                CornerPagesItem().Items().Append(item);
            }

            CornerPagesItem().IsEnabled(m_document.Pages.size() > 1);
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to fill the corner menu.")
    }

    _Use_decl_annotations_
    void RuntimeWindow::OnCornerMenuClick(
        foundation::IInspectable const& sender,
        xaml::RoutedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            auto const element = sender.try_as<xaml::FrameworkElement>();

            if (element == nullptr)
            {
                return;
            }

            auto const tag = std::wstring{ winrt::unbox_value_or<winrt::hstring>(element.Tag(), L"") };

            if (tag == L"panic")
            {
                Panic();
                return;
            }

            if (tag == L"leave")
            {
                SetFullScreen(false);
                return;
            }

            if (tag == L"fit" || tag == L"actual")
            {
                m_document.Scale = tag == L"fit" ? glass::ScaleMode::FitToScreen : glass::ScaleMode::ActualSize;

                auto const previous = m_updatingChrome;
                m_updatingChrome = true;

                ScaleSelector().SelectedIndex(static_cast<int32_t>(m_document.Scale));

                m_updatingChrome = previous;

                ApplyScale();
                return;
            }

            if (tag.rfind(L"corner", 0) == 0 && tag.size() > 6)
            {
                auto const index = tag[6] - L'0';

                if (index >= 0 && index <= 3)
                {
                    m_document.FullScreenButtonCorner = static_cast<glass::ScreenCorner>(index);

                    ApplyCornerButtonPlacement();
                }

                return;
            }

            if (tag.rfind(L"page", 0) == 0 && tag.size() > 4)
            {
                ShowPage(static_cast<size_t>(std::stoul(tag.substr(4))));
            }
        }
        MIDI_GLASS_CATCH_AND_LOG(L"Unable to act on the corner menu.")
    }

    // While a layout is running full screen the machine must not blank the screen. A performer's
    // surface going dark mid set is the worst thing this app could do to them.
    _Use_decl_annotations_
    void RuntimeWindow::HoldDisplayAwake(bool hold)
    {
        try
        {
            if (hold == m_displayHeld)
            {
                return;
            }

            if (m_displayRequest == nullptr)
            {
                m_displayRequest = winrt::Windows::System::Display::DisplayRequest{};
            }

            if (hold)
            {
                m_displayRequest.RequestActive();
            }
            else
            {
                m_displayRequest.RequestRelease();
            }

            m_displayHeld = hold;
        }
        catch (...)
        {
            // A machine that refuses the request is not a reason to stop the layout.
            m_displayHeld = false;
        }
    }
}
