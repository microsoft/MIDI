#pragma once
#include "Placeholder.g.h"

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct Placeholder : PlaceholderT<Placeholder>
    {
        Placeholder() = default;

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct Placeholder : PlaceholderT<Placeholder, implementation::Placeholder>
    {
    };
}
