/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include <rack.hpp>

json_t* json_bool(bool value);
#define json_bool_value(X) json_is_true(X)

inline void schmittTrigger(bool & state, float input, bool & highEvent, bool & lowEvent){
	if(!state && input >= 2.0f){
		state = true;
		highEvent = true;
	}else if(state && input <= 0.1f){
		state = false;
		lowEvent = true;
	}
}

inline bool schmittTrigger(bool & state, float input){
	if(!state && input >= 2.0f){
		state = true;
		return true;
	}else if(state && input <= 0.1f){
		state = false;
		return false;
	}
	return false;
}

inline bool buttonTrigger(bool & state, float input){
	if(!state && input >= 1.0f){
		state = true;
		return true;
	}else if(state && input <= 0.f){
		state = false;
		return false;
	}
	return false;
}

inline void countClockLength(int & clockCounter, int & clockLength, bool clockHighEvent){
	if(clockHighEvent){
		clockLength = clockCounter;
		clockCounter = 0;
	}
	clockCounter++;
}
