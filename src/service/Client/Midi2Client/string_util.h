#pragma once

#include <pch.h>

#include <algorithm>
#include <string>
#include <cwctype>


namespace Windows::Devices::Midi2::Internal
{
	inline winrt::hstring TrimmedHStringCopy(_In_ winrt::hstring s)
	{
		std::wstring ws{ s };

		size_t index = ws.find_first_not_of(' ');
		if (index != std::wstring::npos)
		{
			ws.erase(0, index);
		}

		index = ws.find_last_not_of(' ');
		ws.resize(index + 1);

		return hstring{ ws };
	}

	inline winrt::hstring ToUpperHStringCopy(_In_ winrt::hstring s)
	{
		std::wstring ws{ s };
		std::transform(ws.begin(), ws.end(), ws.begin(), std::towupper); 

		return hstring{ ws };
	}


	inline winrt::hstring ToUpperTrimmedHStringCopy(_In_ winrt::hstring s)
	{
		return ToUpperHStringCopy(TrimmedHStringCopy(s));
	}


}
