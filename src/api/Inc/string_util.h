#pragma once


inline void InPlaceToLower(_In_ std::wstring &s)
{
    std::transform(s.begin(), s.end(), s.begin(), towlower);
}

