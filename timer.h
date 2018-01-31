#pragma once

#include <stdlib.h>
#include <stdio.h>

namespace gnilk
{

	class Timer {
	private:
		double base;
		double resolution;
	public:
		Timer();
		double GetTime();
		void SetTime(double nTime);
	};
}