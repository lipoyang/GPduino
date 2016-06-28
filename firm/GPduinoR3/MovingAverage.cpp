/*
This software is available under NYSL(Nirunari Yakunari Sukinishiro License).
*/

#include <string.h>
#include "MovingAverage.h"

MovingAverage::MovingAverage()
{
	init();
}

void MovingAverage::init()
{
	isFirstSample = true;
}

VAL_TYPE MovingAverage::pop(unsigned short val)
{
	VAL_TYPE ret = 0;
	
	if(isFirstSample)
	{
		sum = 0;
		index = 0;
		for(int i=0;i<BUFFSIZE;i++){
			buff[i] = val;
			sum += (SUM_TYPE)val;
		}
		ret = val;
		isFirstSample = false;
	}
	else
	{
		sum -= (SUM_TYPE)buff[index];
		sum += (SUM_TYPE)val;
		buff[index] = val;
		index++;
		if(index >= BUFFSIZE) index = 0;
		ret = (VAL_TYPE)(sum / BUFFSIZE);
	}
	
	return ret;
}
