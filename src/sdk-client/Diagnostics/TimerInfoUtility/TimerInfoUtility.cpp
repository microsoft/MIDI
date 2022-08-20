
#include <iostream>
#include <iomanip>
#include "Windows.h"

// QPC and Timer info
// https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps#hardware-timer-info

class commaPunctuation : public std::numpunct<char>
{
protected:
    virtual char do_thousands_sep() const
    {
        return ',';
    }

    virtual std::string do_grouping() const
    {
        return "\03";
    }
};

int main()
{
    std::locale commaLocale(std::locale(), new commaPunctuation());
    std::cout.imbue(commaLocale);

    std::cout << "This is for the Microsoft Windows MIDI Services project." << std::endl;
    std::cout << "Not guaranteed to work on systems older than recent Windows 10." << std::endl << std::endl;
    std::cout << "QueryPerformanceCounter timer info for this PC:" << std::endl << std::endl;

    LARGE_INTEGER perfCount;
    LARGE_INTEGER frequency;

    if (QueryPerformanceFrequency(&frequency))
    {
        std::cout << "Frequency: " << std::fixed << frequency.QuadPart << " counts per second " << std::endl << std::endl;
    }

    for (int i = 0; i < 10; i++)
    {
        if (QueryPerformanceCounter(&perfCount))
        {
            std::cout << "Current performance counter: " << std::fixed << perfCount.QuadPart << std::endl;

        }
        else
        {
            std::cout << "Unable to query performance counter on this PC." << std::endl;
        }
    }

    std::cout << std::endl << "Deltas in above times include console output time." << std::endl << std::endl;


}
