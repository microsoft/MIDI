#pragma once

#include "pch.h"

#include "MessageHeaders.h"

struct ServicePingRequestMessage
{
	RequestMessageHeader Header;
};

struct ServicePingResponseMessage
{
	ResponseMessageHeader Header;
};


