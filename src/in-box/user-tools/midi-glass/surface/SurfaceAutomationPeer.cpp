// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "SurfaceAutomationPeer.h"
#include "SurfaceAutomationPeer.g.cpp"
#include "GlassControl.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml::Automation::Peers;

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    SurfaceAutomationPeer::SurfaceAutomationPeer(midiglass::GlassControl const& owner)
        : SurfaceAutomationPeerT<SurfaceAutomationPeer>(owner)
        , m_owner(owner)
    {
    }

    midiglass::GlassControl SurfaceAutomationPeer::Owner() const
    {
        return m_owner.get();
    }

    hstring SurfaceAutomationPeer::GetClassNameCore() const
    {
        return L"GlassControl";
    }

    hstring SurfaceAutomationPeer::GetNameCore() const
    {
        if (auto const owner = Owner())
        {
            auto const name = owner.SurfaceName();

            if (!name.empty())
            {
                return name;
            }
        }

        return ::midiglass::resources::GetString(L"SurfaceControlUnnamed");
    }

    AutomationControlType SurfaceAutomationPeer::GetAutomationControlTypeCore() const
    {
        auto const owner = Owner();

        if (owner == nullptr)
        {
            return AutomationControlType::Slider;
        }

        switch (owner.SurfaceRole())
        {
        case midiglass::SurfaceControlRole::Button:
            return AutomationControlType::Button;

        case midiglass::SurfaceControlRole::Toggle:
            return AutomationControlType::CheckBox;

        case midiglass::SurfaceControlRole::Text:
            return AutomationControlType::Text;

        case midiglass::SurfaceControlRole::Slider:
        default:
            return AutomationControlType::Slider;
        }
    }

    _Use_decl_annotations_
    foundation::IInspectable SurfaceAutomationPeer::GetPatternCore(PatternInterface pattern)
    {
        auto const owner = Owner();
        auto const role = owner != nullptr ? owner.SurfaceRole() : midiglass::SurfaceControlRole::Slider;

        switch (role)
        {
        case midiglass::SurfaceControlRole::Button:
            if (pattern == PatternInterface::Invoke)
            {
                return get_strong().as<foundation::IInspectable>();
            }
            break;

        case midiglass::SurfaceControlRole::Toggle:
            if (pattern == PatternInterface::Toggle)
            {
                return get_strong().as<foundation::IInspectable>();
            }
            break;

        case midiglass::SurfaceControlRole::Text:
            break;

        case midiglass::SurfaceControlRole::Slider:
        default:
            if (pattern == PatternInterface::RangeValue)
            {
                return get_strong().as<foundation::IInspectable>();
            }
            break;
        }

        return nullptr;
    }

    bool SurfaceAutomationPeer::IsReadOnly() const noexcept
    {
        auto const owner = m_owner.get();

        return owner != nullptr && owner.SurfaceRole() == midiglass::SurfaceControlRole::Text;
    }

    double SurfaceAutomationPeer::Value() const
    {
        auto const owner = Owner();

        return owner != nullptr ? owner.SurfaceValue() : 0.0;
    }

    _Use_decl_annotations_
    void SurfaceAutomationPeer::SetValue(double value)
    {
        if (auto const owner = Owner())
        {
            owner.SurfaceValue(std::clamp(value, 0.0, 1.0));
        }
    }

    xaml::Automation::ToggleState SurfaceAutomationPeer::ToggleState() const
    {
        auto const owner = Owner();

        return owner != nullptr && owner.SurfaceValue() >= 0.5
            ? xaml::Automation::ToggleState::On
            : xaml::Automation::ToggleState::Off;
    }

    void SurfaceAutomationPeer::Toggle()
    {
        if (auto const owner = Owner())
        {
            owner.SurfaceValue(owner.SurfaceValue() >= 0.5 ? 0.0 : 1.0);
        }
    }

    void SurfaceAutomationPeer::Invoke()
    {
        if (auto const owner = Owner())
        {
            winrt::get_self<implementation::GlassControl>(owner)->RaiseInvoke();
        }
    }
}
