// ------------------------------------------------------------------------------------------------
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the GitHub project root for license information.
// ------------------------------------------------------------------------------------------------


#include <iostream>
#include <memory>
#include "Windows.h"

#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <debugapi.h>

const std::wstring queueName = L"TestQueueFooBar123456";
const int capacity = 10000;

using namespace boost::interprocess;


#define RIGHT_ALIGN_FIXED std::fixed << std::right << std::setw(16)

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
    std::cout << "Server App. This app WRITES to the queue." << std::endl;

    if (IsDebuggerPresent())
    {
        std::cout << "Currently running under the debugger." << std::endl;
    }


#ifdef _DEBUG
    std::cout << "Compiled in DEBUG mode. Performance will be negatively impacted." << std::endl;
#endif

    std::cout << std::endl;

    std::locale commaLocale(std::locale(), new commaPunctuation());
    std::cout.imbue(commaLocale);

    LARGE_INTEGER startPerfCount;
    LARGE_INTEGER endPerfCount;
    LARGE_INTEGER frequency;

    if (QueryPerformanceFrequency(&frequency))
    {
        std::cout << "Frequency: " << std::fixed << frequency.QuadPart << " counts per second " << std::endl << std::endl;
    }

    
    // remove the file if already here
    message_queue::remove(queueName.c_str());

	// Create the new queue
	std::unique_ptr<message_queue> queue; queue = std::make_unique<message_queue>(
		boost::interprocess::create_only,
		queueName.c_str(),
		capacity,
		sizeof uint32_t);



    if (QueryPerformanceCounter(&startPerfCount))
    {
        uint32_t writeCount = 0;

        for (uint32_t i = 0; i < capacity; i++)
        {
            // write to queue
            queue->send(&i, sizeof(i), 0);

            writeCount++;
        }

        QueryPerformanceCounter(&endPerfCount);


        // TODO: Notify listeners that they can now read

        double totalCount = endPerfCount.QuadPart - startPerfCount.QuadPart;
        double totalSeconds =      totalCount / (double)frequency.QuadPart;
        double totalMilliseconds = totalCount / (double)frequency.QuadPart * 1000.0;
        double totalMicroseconds = totalCount / (double)frequency.QuadPart * 1000000.0;

        double average = ((double)(totalCount)) / writeCount;
        double averageInSeconds = average / (double)frequency.QuadPart;
        double averageInMilliseconds = (average / (double)frequency.QuadPart) * 1000.0;
        double averageInMicroseconds = (average / (double)frequency.QuadPart) * 1000000.0;
        double averageInNanoseconds =  (average / (double)frequency.QuadPart) * 1000000000.0;


        std::cout << "Words added to queue:             " << std::fixed << writeCount << std::endl;
        std::cout << "Start performance counter:        " << std::fixed << startPerfCount.QuadPart << std::endl;
        std::cout << "End performance counter:          " << std::fixed << endPerfCount.QuadPart << std::endl;
        std::cout << "Performance counter ticks/second: " << std::fixed << frequency.QuadPart << std::endl;

        std::cout << std::endl;


        std::cout.precision(0);
        std::cout << "Total write time for " << capacity << " words" << std::endl;
        std::cout << "====================================================" << std::endl;
        std::cout.precision(8);

        std::cout << " - Performance Counts: " << RIGHT_ALIGN_FIXED << totalCount << std::endl;
        std::cout << " - Seconds:            " << RIGHT_ALIGN_FIXED << totalSeconds << std::endl;
        std::cout << " - Milliseconds:       " << RIGHT_ALIGN_FIXED << totalMilliseconds << std::endl;
        std::cout << " - Microseconds:       " << RIGHT_ALIGN_FIXED << totalMicroseconds << std::endl;
        std::cout << std::endl;

        std::cout << "Average write time per 32-bit word" << std::endl;
        std::cout << "====================================================" << std::endl;
        std::cout << " - Performance Counts: " << RIGHT_ALIGN_FIXED << average << std::endl;
        std::cout << " - Seconds:            " << RIGHT_ALIGN_FIXED << averageInSeconds << std::endl;
        std::cout << " - Milliseconds:       " << RIGHT_ALIGN_FIXED << averageInMilliseconds << std::endl;
        std::cout << " - Microseconds:       " << RIGHT_ALIGN_FIXED << averageInMicroseconds << std::endl;
        std::cout << " - Nanoseconds:        " << RIGHT_ALIGN_FIXED << averageInNanoseconds << std::endl;

        std::cout << "Above numbers represent the average time for a single 32-bit word to be written to the queue." << std::endl;
        std::cout << "MIDI messages come in 32/64/96/128 bit packets, with the majority being 32 and 64 bits." << std::endl;
        std::cout << std::endl;
        std::cout << "Press enter to remove the queue and shared memory file (do this after client has finished reading)... " << std::endl;
        std::cin.get();
    }
    else
    {
        std::cout << "Unable to query performance counter." << std::endl;
    }

    // cleanup
    std::cout << "Removing shared memory" << std::endl << std::endl;
    message_queue::remove(queueName.c_str());

}

