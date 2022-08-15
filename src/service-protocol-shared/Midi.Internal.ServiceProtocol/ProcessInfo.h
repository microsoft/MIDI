#pragma once

// required for QueryFullProcessImageNameA
//#define _WIN32_WINNT 0x0600

#include "pch.h"

#include <string>
#include <Windows.h>

#include <boost/filesystem/path.hpp>
#include <boost/log/attributes/current_process_id.hpp>
#include <boost/log/attributes/current_process_name.hpp>


std::string GetCurrentExecutingProcessName()
{
	//// TODO API call to get process name
	//const int BUFFER_SIZE = MAX_PATH + 1;
	//char buffer[BUFFER_SIZE];
	//DWORD bufferSize = BUFFER_SIZE;


	//HANDLE hProcess = GetCurrentProcess();



	//if (QueryFullProcessImageNameA(hProcess, 0, buffer, &bufferSize))
	//{
	//	std::string moduleFileName(buffer, bufferSize);

	//	std::filesystem::path filepath{ moduleFileName };

	//	// grab just the name of the app, without the full path
	//	return filepath.filename().string();
	//}
	//else
	//{ 
	//	return std::string{ "Unknown Process" };
	//}


	return boost::log::attributes::current_process_name().get();

}

uint32_t GetCurrentExecutingProcessId()
{
	//return GetCurrentProcessId();
	return (uint32_t)(boost::log::attributes::current_process_id().get().native_id());

}


