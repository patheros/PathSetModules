#include "plugin.hpp"

json_t* json_bool(bool value){
	return value ? json_true() : json_false();
}