/*
This software is available under NYSL(Nirunari Yakunari Sukinishiro License).
*/

//#ifndef    _MOVING_AVERAGE_H_
//#define    _MOVING_AVERAGE_H_

#define BUFFSIZE 8
typedef unsigned short VAL_TYPE;
typedef unsigned short SUM_TYPE;

// moving average class
class MovingAverage
{
public:
	MovingAverage();
	void init();
	VAL_TYPE pop(unsigned short val);
	
private:
	int index;
	VAL_TYPE buff[BUFFSIZE];
	SUM_TYPE sum;
	bool isFirstSample;
};

//#endif
