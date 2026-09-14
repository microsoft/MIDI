// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

namespace
{
    namespace notifications = ::winrt::Windows::UI::Notifications;
    namespace xmldom = ::winrt::Windows::Data::Xml::Dom;
}

_Use_decl_annotations_
std::wstring ToastSender::SanitizeForToast(std::wstring const& untrusted) noexcept
{
    std::wstring result{ };
    result.reserve(untrusted.size());

    size_t copied{ 0 };

    for (auto const c : untrusted)
    {
        if (copied >= MaximumUntrustedTextLength)
        {
            result.append(L"\u2026");
            break;
        }

        // Control characters would not render and can be used to disguise the rest of the text.
        if (c < 0x20)
        {
            continue;
        }

        switch (c)
        {
        case L'&':  result.append(L"&amp;");  break;
        case L'<':  result.append(L"&lt;");   break;
        case L'>':  result.append(L"&gt;");   break;
        case L'"':  result.append(L"&quot;"); break;
        case L'\'': result.append(L"&apos;"); break;
        default:    result.push_back(c);      break;
        }

        copied++;
    }

    return result;
}

_Use_decl_annotations_
HRESULT ToastSender::ShowOrReplace(
    std::wstring const& tag,
    std::wstring const& title,
    std::wstring const& body,
    std::wstring const& attribution,
    std::wstring const& buttonText,
    std::wstring const& buttonProtocolUri) noexcept
try
{
    // Everything interpolated below is either a resource string or has already been through
    // SanitizeForToast. Nothing straight off the network reaches this.
    std::wstring xml{ L"<toast scenario=\"reminder\" activationType=\"protocol\" launch=\"" };
    xml += buttonProtocolUri;
    xml += L"\">";
    xml += L"<visual><binding template=\"ToastGeneric\">";
    xml += L"<text>" + title + L"</text>";
    xml += L"<text>" + body + L"</text>";
    xml += L"<text placement=\"attribution\">" + attribution + L"</text>";
    xml += L"</binding></visual>";
    xml += L"<actions>";
    xml += L"<action content=\"" + buttonText + L"\" activationType=\"protocol\" arguments=\"" + buttonProtocolUri + L"\" />";
    xml += L"</actions>";
    xml += L"</toast>";

    xmldom::XmlDocument document{ };
    document.LoadXml(winrt::hstring{ xml });

    notifications::ToastNotification toast{ document };
    toast.Tag(winrt::hstring{ tag });
    toast.Group(winrt::hstring{ ToastGroup });

    auto notifier = notifications::ToastNotificationManager::CreateToastNotifier(
        winrt::hstring{ MIDI_NOTIFICATIONS_AUMID });

    RETURN_HR_IF_NULL(E_UNEXPECTED, notifier);

    // The customer has turned notifications off for this app in Windows, which is a decision to
    // respect rather than work around.
    if (notifier.Setting() != notifications::NotificationSetting::Enabled)
    {
        return S_FALSE;
    }

    notifier.Show(toast);

    return S_OK;
}
CATCH_RETURN()

_Use_decl_annotations_
void ToastSender::Remove(std::wstring const& tag) noexcept
{
    try
    {
        notifications::ToastNotificationManager::History().Remove(
            winrt::hstring{ tag },
            winrt::hstring{ ToastGroup },
            winrt::hstring{ MIDI_NOTIFICATIONS_AUMID });
    }
    catch (...)
    {
        // Nothing to show the customer, and nothing depends on the removal succeeding.
        LOG_CAUGHT_EXCEPTION();
    }
}
