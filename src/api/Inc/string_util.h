#pragma once


inline void InPlaceToLower(_Inout_ std::wstring &s)
{
    std::transform(s.begin(), s.end(), s.begin(), towlower);
}

