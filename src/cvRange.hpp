#pragma once

#include <rack.hpp>

using namespace rack;

#define CVRANGE_MAX 12

enum CVRange{
	Bipolar_10,
	Bipolar_5,
	Bipolar_3,
	Bipolar_1,
	Unipolar_10,
	Unipolar_5,
	Unipolar_3,
	Unipolar_1,
	Bipolar_4,
	Bipolar_2,
	Unipolar_4,
	Unipolar_2,
};

extern const std::string CVRange_Lables [CVRANGE_MAX];

extern const int CVRange_Order [CVRANGE_MAX];

float mapCVRange(float in, CVRange range);

float invMapCVRange(float in, CVRange range);

template <typename MT = Module>
struct CVRangeParamQuantity : ParamQuantity  {
	float getDisplayValue() override {
		float v = getValue();
		auto _module = dynamic_cast<MT*>(module);
		return mapCVRange(v,_module->range);
	}
	void setDisplayValue(float v) override {
		auto _module = dynamic_cast<MT*>(module);
		setValue(invMapCVRange(v,_module->range));
	}
};

template <typename MT = Module>
void addRangeSelectMenu(MT * module, Menu * menu){
	menu->addChild(createSubmenuItem("Range", CVRange_Lables[module->range],
		[=](Menu* menu) {
			for(int i = 0; i < CVRANGE_MAX; i++){
				int ri = CVRange_Order[i];
				menu->addChild(createMenuItem(CVRange_Lables[ri], CHECKMARK(module->range == ri), [module,ri]() { 
					module->range = (CVRange)ri;
				}));
			}
		}
	));
}