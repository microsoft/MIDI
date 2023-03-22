#pragma once

#include "pch.h"

#include <ctime>
#include <iostream>
#include <chrono>
#include <iomanip>

//using date::operator<<;

class TimeHelpers
{
public:
	static std::chrono::system_clock::time_point GetCurrentTime()
	{
		auto now = std::chrono::system_clock::now();

		return now;
	}

	// just for debugging and console use. Sends to cout

	static void PrintCurrentTime()
	{
		std::cout << std::dec << std::setw(15) << std::left << std::setfill(' ')
			<< std::chrono::duration_cast<std::chrono::milliseconds>(GetCurrentTime().time_since_epoch()).count();
	
	}

};