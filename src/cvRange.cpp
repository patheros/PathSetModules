#include "cvRange.hpp"

const std::string CVRange_Lables [CVRANGE_MAX] = {
	"+/-10V", //0
	"+/-5V", //1
	"+/-3V", //2
	"+/-1V", //3
	"0V-10V", //4
	"0V-5V", //5
	"0V-3V", //6
	"0V-1V", //7
	"+/-4V", //8
	"+/-2V", //9
	"0V-4V", //10
	"0V-2V", //11
	
};

const int CVRange_Order [CVRANGE_MAX] = {0,1,8,2,9,3,4,5,10,6,11,7};

float mapCVRange(float in, CVRange range){
	switch(range){
		case Bipolar_10:
			return in * 20.f - 10.f;
		case Bipolar_5:
			return in * 10.f - 5.f;
		case Bipolar_3:
			return in * 6.f - 3.f;
		case Bipolar_1:
			return in * 2.f - 1.f;
		case Unipolar_10:
			return in * 10.f;
		case Unipolar_5:
			return in * 5.f;
		case Unipolar_3:
			return in * 3.f;
		case Unipolar_1:
			return in * 1.f;
		case Bipolar_4:
			return in * 8.f - 4.f;
		case Bipolar_2:
			return in * 4.f - 2.f;
		case Unipolar_4:
			return in * 4.f;
		case Unipolar_2:
			return in * 2.f;
	}
	return 0;
}

float invMapCVRange(float in, CVRange range){
	switch(range){
		case Bipolar_10:
			return (in + 10.f) / 20.f;
		case Bipolar_5:
			return (in + 5.f) / 10.f;
		case Bipolar_3:
			return (in + 3.f) / 6.f;
		case Bipolar_1:
			return (in + 1.f) / 2.f;
		case Unipolar_10:
			return in / 10.f;
		case Unipolar_5:
			return in / 5.f;
		case Unipolar_3:
			return in / 3.f;
		case Unipolar_1:
			return in / 1.f;
		case Bipolar_4:
			return (in + 4.f) / 8.f;
		case Bipolar_2:
			return (in + 2.f) / 4.f;
		case Unipolar_4:
			return in / 4.f;
		case Unipolar_2:
			return in / 2.f;
	}
	return 0;
}