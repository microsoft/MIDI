#include "pch.h"

//#define CATCH_CONFIG_MAIN
//#define CATCH_CONFIG_RUNNER


#define CATCH_AMALGAMATED_CUSTOM_MAIN


#include "catch_amalgamated.hpp"

#include <iostream>

using namespace winrt::Windows::Foundation;

int __cdecl main(int argc, char* argv[]) 
{
    std::cout << "Custom main for Catch2" << std::endl;

    winrt::init_apartment();

    int result = Catch::Session().run(argc, argv);

    return result;
}