// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "WindowChrome.h"

#include <robuffer.h>

namespace backdrops = ::winrt::Microsoft::UI::Composition::SystemBackdrops;
namespace wux = ::winrt::Microsoft::UI::Xaml;
namespace wuw = ::winrt::Microsoft::UI::Windowing;

// the linker supplies this; it is the HINSTANCE of the module holding the icon resource
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace midiapp
{
    namespace
    {
        winrt::Windows::UI::Color ColorFromArgb(uint32_t argb) noexcept
        {
            winrt::Windows::UI::Color color{};

            color.A = static_cast<uint8_t>((argb >> 24) & 0xFF);
            color.R = static_cast<uint8_t>((argb >> 16) & 0xFF);
            color.G = static_cast<uint8_t>((argb >> 8) & 0xFF);
            color.B = static_cast<uint8_t>(argb & 0xFF);

            return color;
        }
    }

    wux::Media::Imaging::WriteableBitmap WindowChrome::LoadIconImageSource(
        uint16_t const resourceId,
        int32_t const sizePixels) noexcept
    {
        try
        {
            if (sizePixels <= 0)
            {
                return nullptr;
            }

            auto const instance = reinterpret_cast<HINSTANCE>(&__ImageBase);

            wil::unique_hicon icon{ static_cast<HICON>(::LoadImageW(
                instance, MAKEINTRESOURCEW(resourceId), IMAGE_ICON, sizePixels, sizePixels, LR_DEFAULTCOLOR)) };

            if (!icon)
            {
                return nullptr;
            }

            ICONINFO info{};

            if (!::GetIconInfo(icon.get(), &info))
            {
                return nullptr;
            }

            wil::unique_hbitmap colorBitmap{ info.hbmColor };
            wil::unique_hbitmap maskBitmap{ info.hbmMask };

            if (!colorBitmap)
            {
                return nullptr;
            }

            BITMAPINFO bitmapInfo{};
            bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth = sizePixels;
            bitmapInfo.bmiHeader.biHeight = -sizePixels;      // negative for a top down DIB
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;

            std::vector<uint8_t> pixels(static_cast<size_t>(sizePixels) * sizePixels * 4);

            // CreateCompatibleDC pairs with DeleteDC, which is what wil::unique_hdc does. A DC
            // from GetDC would need ReleaseDC instead.
            wil::unique_hdc memoryDC{ ::CreateCompatibleDC(nullptr) };

            if (!memoryDC)
            {
                return nullptr;
            }

            if (::GetDIBits(memoryDC.get(), colorBitmap.get(), 0, sizePixels,
                    pixels.data(), &bitmapInfo, DIB_RGB_COLORS) == 0)
            {
                return nullptr;
            }

            // WriteableBitmap wants premultiplied BGRA; an icon's color bitmap carries straight
            // alpha, so without this every semi transparent edge pixel reads too bright.
            for (size_t i = 0; i < pixels.size(); i += 4)
            {
                auto const alpha = pixels[i + 3];

                pixels[i + 0] = static_cast<uint8_t>((pixels[i + 0] * alpha) / 255);
                pixels[i + 1] = static_cast<uint8_t>((pixels[i + 1] * alpha) / 255);
                pixels[i + 2] = static_cast<uint8_t>((pixels[i + 2] * alpha) / 255);
            }

            wux::Media::Imaging::WriteableBitmap bitmap{ sizePixels, sizePixels };

            auto buffer = bitmap.PixelBuffer();

            auto byteAccess = buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();

            uint8_t* destination{ nullptr };

            if (FAILED(byteAccess->Buffer(&destination)) ||
                destination == nullptr ||
                buffer.Capacity() < pixels.size())
            {
                return nullptr;
            }

            memcpy(destination, pixels.data(), pixels.size());

            bitmap.Invalidate();

            return bitmap;
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            return nullptr;
        }
    }

    wuw::AppWindow WindowChrome::GetAppWindow() const noexcept
    {
        try
        {
            if (m_elements.Window == nullptr)
            {
                return nullptr;
            }

            return m_elements.Window.AppWindow();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            return nullptr;
        }
    }

    void WindowChrome::Initialize(WindowChromeElements const& elements, MidiAppSettings& settings) noexcept
    {
        try
        {
            m_elements = elements;
            m_settings = &settings;

            m_elements.Window.ExtendsContentIntoTitleBar(true);

            if (m_elements.TitleBar != nullptr)
            {
                m_elements.Window.SetTitleBar(m_elements.TitleBar);
            }

            ApplyTitleBarColors();
            UpdateTitleBarInsets();
            ApplyTheme();
            ApplyAlwaysOnTop();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::ApplyTheme() noexcept
    {
        try
        {
            if (m_settings == nullptr || m_elements.Root == nullptr)
            {
                return;
            }

            auto const theme = m_settings->Theme();

            auto const requested =
                theme == AppTheme::Light ? wux::ElementTheme::Light :
                theme == AppTheme::Dark ? wux::ElementTheme::Dark :
                wux::ElementTheme::Default;

            m_elements.Root.RequestedTheme(requested);

            ApplyTitleBarColors();
            ApplyBackdrop();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::ApplyBackdrop() noexcept
    {
        try
        {
            if (m_settings == nullptr)
            {
                return;
            }

            auto const backdrop = m_settings->Backdrop();

            // Rebuilding the material tears it down and flickers. Only do it when it changes.
            if (!m_backdropApplied || m_appliedBackdrop != backdrop)
            {
                ReleaseBackdropControllers();

                auto const target =
                    m_elements.Window.try_as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>();

                if (target != nullptr)
                {
                    if (m_backdropConfiguration == nullptr)
                    {
                        m_backdropConfiguration = backdrops::SystemBackdropConfiguration{};
                        m_backdropConfiguration.IsInputActive(true);

                        // the material dims when the window is not the active one
                        m_activatedToken = m_elements.Window.Activated(
                            [this](auto&&, wux::WindowActivatedEventArgs const& args)
                            {
                                if (m_backdropConfiguration != nullptr)
                                {
                                    m_backdropConfiguration.IsInputActive(
                                        args.WindowActivationState() != wux::WindowActivationState::Deactivated);
                                }
                            });
                    }

                    switch (backdrop)
                    {
                    case WindowBackdrop::Mica:
                        if (backdrops::MicaController::IsSupported())
                        {
                            m_micaController = backdrops::MicaController{};
                            m_micaController.SetSystemBackdropConfiguration(m_backdropConfiguration);
                            m_micaController.AddSystemBackdropTarget(target);
                        }
                        break;

                    case WindowBackdrop::Acrylic:
                        if (backdrops::DesktopAcrylicController::IsSupported())
                        {
                            m_acrylicController = backdrops::DesktopAcrylicController{};
                            m_acrylicController.SetSystemBackdropConfiguration(m_backdropConfiguration);
                            m_acrylicController.AddSystemBackdropTarget(target);
                        }
                        break;

                    default:
                        break;
                    }
                }

                m_appliedBackdrop = backdrop;
                m_backdropApplied = true;
            }

            UpdateBackdropConfiguration();

            // a system material only shows if the window is not painting over it
            if (m_elements.Fill != nullptr)
            {
                m_elements.Fill.Visibility(backdrop == WindowBackdrop::Solid
                    ? wux::Visibility::Visible
                    : wux::Visibility::Collapsed);
            }

            ApplyBackgroundColor();
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::ApplyBackgroundColor() noexcept
    {
        try
        {
            if (m_settings == nullptr)
            {
                return;
            }

            auto const useCustom = m_settings->UseCustomBackgroundColor();
            auto const color = ColorFromArgb(m_settings->BackgroundColorArgb());

            // Solid has no material to tint, so the color goes on a layer of its own. Mica and
            // Acrylic take it as the material's tint instead, which keeps their texture.
            auto const showTint = useCustom && m_settings->Backdrop() == WindowBackdrop::Solid;

            if (m_elements.Tint != nullptr)
            {
                if (showTint)
                {
                    m_elements.Tint.Background(wux::Media::SolidColorBrush{ color });
                }

                m_elements.Tint.Visibility(showTint ? wux::Visibility::Visible : wux::Visibility::Collapsed);
            }

            if (m_micaController != nullptr)
            {
                if (useCustom)
                {
                    m_micaController.TintColor(color);
                    m_micaController.FallbackColor(color);
                }
                else
                {
                    m_micaController.ResetProperties();
                }
            }

            if (m_acrylicController != nullptr)
            {
                if (useCustom)
                {
                    m_acrylicController.TintColor(color);
                    m_acrylicController.FallbackColor(color);
                }
                else
                {
                    m_acrylicController.ResetProperties();
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::UpdateBackdropConfiguration() noexcept
    {
        try
        {
            if (m_backdropConfiguration == nullptr || m_elements.Root == nullptr)
            {
                return;
            }

            m_backdropConfiguration.Theme(m_elements.Root.ActualTheme() == wux::ElementTheme::Dark
                ? backdrops::SystemBackdropTheme::Dark
                : backdrops::SystemBackdropTheme::Light);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::ReleaseBackdropControllers() noexcept
    {
        try
        {
            if (m_micaController != nullptr)
            {
                m_micaController.Close();
                m_micaController = nullptr;
            }

            if (m_acrylicController != nullptr)
            {
                m_acrylicController.Close();
                m_acrylicController = nullptr;
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::Shutdown() noexcept
    {
        ReleaseBackdropControllers();
    }

    void WindowChrome::ApplyTitleBarColors() noexcept
    {
        try
        {
            auto appWindow = GetAppWindow();

            if (appWindow == nullptr || m_elements.Root == nullptr)
            {
                return;
            }

            auto titleBar = appWindow.TitleBar();

            auto const transparent = winrt::Windows::UI::Colors::Transparent();

            titleBar.ButtonBackgroundColor(transparent);
            titleBar.ButtonInactiveBackgroundColor(transparent);

            auto const dark = (m_elements.Root.ActualTheme() == wux::ElementTheme::Dark);

            auto const foreground = dark ? winrt::Windows::UI::Colors::White() : winrt::Windows::UI::Colors::Black();

            titleBar.ButtonForegroundColor(foreground);
            titleBar.ButtonHoverForegroundColor(foreground);
            titleBar.ButtonPressedForegroundColor(foreground);
            titleBar.ButtonInactiveForegroundColor(
                dark ? winrt::Windows::UI::Colors::Gray() : winrt::Windows::UI::Colors::DimGray());
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::UpdateTitleBarInsets() noexcept
    {
        try
        {
            auto appWindow = GetAppWindow();

            if (appWindow == nullptr || m_elements.LeftInset == nullptr || m_elements.RightInset == nullptr)
            {
                return;
            }

            auto const titleBar = appWindow.TitleBar();

            auto scale = 1.0;

            if (m_elements.Root != nullptr)
            {
                if (auto const xamlRoot = m_elements.Root.XamlRoot())
                {
                    scale = xamlRoot.RasterizationScale();
                }
            }

            if (scale <= 0.0)
            {
                scale = 1.0;
            }

            // insets are physical pixels; XAML columns are in DIPs
            m_elements.LeftInset.Width(wux::GridLength{ titleBar.LeftInset() / scale, wux::GridUnitType::Pixel });
            m_elements.RightInset.Width(wux::GridLength{ titleBar.RightInset() / scale, wux::GridUnitType::Pixel });
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::ApplyAlwaysOnTop() noexcept
    {
        try
        {
            auto appWindow = GetAppWindow();

            if (appWindow == nullptr || m_settings == nullptr)
            {
                return;
            }

            if (auto presenter = appWindow.Presenter().try_as<wuw::OverlappedPresenter>())
            {
                presenter.IsAlwaysOnTop(m_settings->AlwaysOnTop());
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::SetWindowIconFromResource(uint16_t resourceId) noexcept
    {
        try
        {
            if (m_elements.Window == nullptr)
            {
                return;
            }

            HWND handle{ nullptr };

            if (auto const native = m_elements.Window.try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            if (handle == nullptr)
            {
                return;
            }

            auto const instance = reinterpret_cast<HINSTANCE>(&__ImageBase);

            // load each size separately: letting Windows stretch one bitmap is what makes small
            // icons look muddy
            m_largeIcon = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(resourceId),
                IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));

            m_smallIcon = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(resourceId),
                IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

            if (m_largeIcon != nullptr)
            {
                ::SendMessageW(handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(m_largeIcon));
            }

            if (m_smallIcon != nullptr)
            {
                ::SendMessageW(handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(m_smallIcon));
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::RestorePlacement(
        wux::Window const& window,
        MidiAppSettings const& settings,
        int32_t defaultWidth,
        int32_t defaultHeight) noexcept
    {
        try
        {
            if (window == nullptr)
            {
                return;
            }

            auto appWindow = window.AppWindow();

            if (appWindow == nullptr)
            {
                return;
            }

            auto const& saved = settings.WindowPlacement();

            if (!saved.Valid)
            {
                appWindow.Resize(winrt::Windows::Graphics::SizeInt32{ defaultWidth, defaultHeight });
                return;
            }

            winrt::Windows::Graphics::RectInt32 bounds{ saved.X, saved.Y, saved.Width, saved.Height };

            // The saved monitor may be gone or smaller now, so pull the window back onto a
            // display that actually exists before showing it.
            auto const display = wuw::DisplayArea::GetFromRect(bounds, wuw::DisplayAreaFallback::Nearest);

            if (display != nullptr)
            {
                auto const work = display.WorkArea();

                bounds.Width = std::min(bounds.Width, work.Width);
                bounds.Height = std::min(bounds.Height, work.Height);
                bounds.X = std::clamp(bounds.X, work.X, work.X + work.Width - bounds.Width);
                bounds.Y = std::clamp(bounds.Y, work.Y, work.Y + work.Height - bounds.Height);
            }

            appWindow.MoveAndResize(bounds);

            if (saved.Maximized)
            {
                if (auto presenter = appWindow.Presenter().try_as<wuw::OverlappedPresenter>())
                {
                    presenter.Maximize();
                }
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }

    void WindowChrome::SavePlacement() noexcept
    {
        try
        {
            if (m_settings == nullptr || m_elements.Window == nullptr)
            {
                return;
            }

            HWND handle{ nullptr };

            if (auto const native = m_elements.Window.try_as<::IWindowNative>())
            {
                LOG_IF_FAILED(native->get_WindowHandle(&handle));
            }

            if (handle == nullptr)
            {
                return;
            }

            WINDOWPLACEMENT placement{};
            placement.length = sizeof(placement);

            if (!::GetWindowPlacement(handle, &placement))
            {
                return;
            }

            WindowPlacementInfo info{};

            // restore bounds, so a maximized window still reopens at its previous size
            info.X = placement.rcNormalPosition.left;
            info.Y = placement.rcNormalPosition.top;
            info.Width = placement.rcNormalPosition.right - placement.rcNormalPosition.left;
            info.Height = placement.rcNormalPosition.bottom - placement.rcNormalPosition.top;
            info.Maximized = placement.showCmd == SW_SHOWMAXIMIZED;

            m_settings->WindowPlacement(info);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
        }
    }
}
