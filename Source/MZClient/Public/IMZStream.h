#pragma once
#include "RHI.h"


class IMZStream {
public:
	enum Type : uint32_t
	{
		Input = 0,
		Output = 1,
	};
};
