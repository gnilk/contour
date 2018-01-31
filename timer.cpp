/*-------------------------------------------------------------------------
	File    :	Timer.cpp
	Author  :	Fredrik KLing
	EMail   :	fredrik.kling@home.se
	Orginal :	31.01.2018
				
 

--------------------------------------------------------------------------- 
    Todo [-:undone,+:inprogress,!:done]:
	
Changes: 

-- Date -- | -- Name ------- | -- Did what...

---------------------------------------------------------------------------*/
#include <math.h>
#include <mach/mach_time.h>

#include "timer.h"

using namespace gnilk;



static uint64_t getRawTime(void)
{
    return mach_absolute_time();
}

Timer::Timer() {
  	mach_timebase_info_data_t info;
    mach_timebase_info(&info);

    resolution = (double) info.numer / (info.denom * 1.0e9);
    base = getRawTime();	
}

double Timer::GetTime() {
    return (double) (getRawTime() - base) * resolution;
}

void Timer::SetTime(double nTime) {
    base = getRawTime() -(uint64_t) (nTime / resolution);
}

