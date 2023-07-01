#include "pch.h"

#include "catch_amalgamated.hpp"

using namespace winrt;
using namespace Microsoft::Devices::Midi2;

namespace DummyTests
{

	TEST_CASE("Create test class and set a property")
	{
		DummyTestClass obj;

		const uint32_t val = 200;

		obj.SomeProperty(val);

		REQUIRE((bool)(obj.SomeProperty() == val));
	}

	TEST_CASE("Load up UMPs")
	{
		const uint32_t countUmp = 10;

		DummyTestClass obj{};

		obj.LoadDummyUmps(countUmp);

		REQUIRE((bool)(obj.Umps().Size() == countUmp));
	}


}
