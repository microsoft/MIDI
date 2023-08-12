#include "pch.h"

//#define CATCH_CONFIG_MAIN

#include "catch_amalgamated.hpp"

using namespace winrt::Windows::Foundation;

int main(int argc, char* argv[]) 
{
    winrt::init_apartment();


    int result = Catch::Session().run(argc, argv);


    return result;
}