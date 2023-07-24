#include "pch.h"

//#define CATCH_CONFIG_MAIN

#include "catch_amalgamated.hpp"

using namespace winrt;
using namespace Windows::Foundation;

int main(int argc, char* argv[]) 
{
    init_apartment();


    int result = Catch::Session().run(argc, argv);


    return result;
}