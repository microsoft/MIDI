// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Not every tool's pch pulls in the automation peers, and this header uses them directly.
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

namespace midiapp
{
    // ========================================================================================
    // Live regions
    //
    // A status strip tells a sighted customer how a scan, a repair or a send went. A screen
    // reader user never sees it, because focus does not move there. A live region is what makes
    // the change spoken anyway.
    //
    // Setting AutomationProperties.LiveSetting alone is not enough. Assistive technology is only
    // told about a live region when the element raises LiveRegionChanged, and XAML does not
    // raise it for a plain Text assignment. Registering for the property change here means the
    // hundreds of existing StatusText().Text(...) call sites across the tools keep working
    // unchanged - each window marks its status elements once, during setup.
    //
    // Polite waits for a pause in what the screen reader is already saying; Assertive interrupts.
    // Use Assertive only where not hearing it would leave the customer stuck.
    // ========================================================================================

    namespace details
    {
        inline void RaiseLiveRegionChanged(
            _In_ winrt::Microsoft::UI::Xaml::UIElement const& element) noexcept
        {
            try
            {
                namespace peers = winrt::Microsoft::UI::Xaml::Automation::Peers;

                // FromElement only returns a peer that already exists, and one is created lazily
                // the first time assistive technology asks about the element. CreatePeerForElement
                // makes it, which is what the event has to be raised on.
                auto peer = peers::FrameworkElementAutomationPeer::FromElement(element);

                if (peer == nullptr)
                {
                    peer = peers::FrameworkElementAutomationPeer::CreatePeerForElement(element);
                }

                if (peer != nullptr)
                {
                    peer.RaiseAutomationEvent(peers::AutomationEvents::LiveRegionChanged);
                }
            }
            catch (...)
            {
            }
        }
    }

    // Marks a status TextBlock as a live region and announces it whenever its text changes.
    inline void MakeLiveStatusRegion(
        _In_ winrt::Microsoft::UI::Xaml::Controls::TextBlock const& element,
        _In_ bool const assertive = false) noexcept
    {
        namespace xaml = winrt::Microsoft::UI::Xaml;

        if (element == nullptr)
        {
            return;
        }

        try
        {
            xaml::Automation::AutomationProperties::SetLiveSetting(
                element,
                assertive ?
                    xaml::Automation::Peers::AutomationLiveSetting::Assertive :
                    xaml::Automation::Peers::AutomationLiveSetting::Polite);

            element.RegisterPropertyChangedCallback(
                xaml::Controls::TextBlock::TextProperty(),
                [](xaml::DependencyObject const& sender, xaml::DependencyProperty const&)
                {
                    auto const changed = sender.try_as<xaml::Controls::TextBlock>();

                    // Clearing a status is not news, and announcing it would talk over whatever
                    // the customer was reading.
                    if (changed == nullptr || changed.Text().empty())
                    {
                        return;
                    }

                    details::RaiseLiveRegionChanged(changed);
                });
        }
        catch (...)
        {
        }
    }

    // Marks an InfoBar as a live region. The message is announced when the bar opens, and again
    // if its text changes while it is open.
    inline void MakeLiveStatusRegion(
        _In_ winrt::Microsoft::UI::Xaml::Controls::InfoBar const& element,
        _In_ bool const assertive = true) noexcept
    {
        namespace xaml = winrt::Microsoft::UI::Xaml;

        if (element == nullptr)
        {
            return;
        }

        try
        {
            xaml::Automation::AutomationProperties::SetLiveSetting(
                element,
                assertive ?
                    xaml::Automation::Peers::AutomationLiveSetting::Assertive :
                    xaml::Automation::Peers::AutomationLiveSetting::Polite);

            auto const announce =
                [](xaml::DependencyObject const& sender, xaml::DependencyProperty const&)
                {
                    auto const changed = sender.try_as<xaml::Controls::InfoBar>();

                    if (changed == nullptr || !changed.IsOpen())
                    {
                        return;
                    }

                    details::RaiseLiveRegionChanged(changed);
                };

            element.RegisterPropertyChangedCallback(xaml::Controls::InfoBar::IsOpenProperty(), announce);
            element.RegisterPropertyChangedCallback(xaml::Controls::InfoBar::MessageProperty(), announce);
        }
        catch (...)
        {
        }
    }
}
