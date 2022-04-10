/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include "plugin.hpp"
#include "shifty.hpp"

struct ShiftyExpander : ShiftyExpanderBase {
	enum ParamId {
		CLOCK_RATE_AV_PARAM,
		CLOCK_DIVIDER_AV_PARAM,
		RAMP_AV_PARAM,
		SAH_AV_PARAM,
		ENUMS(ECHO_AV_PARAM, NUM_ROWS),
		ENUMS(MUTE_AV_PARAM, NUM_ROWS),
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_RATE_CV_INPUT,
		CLOCK_DIVIDER_CV_INPUT,
		RAMP_CV_INPUT,
		SAH_CV_INPUT,
		ENUMS(ECHO_CV_INPUT, NUM_ROWS),
		ENUMS(MUTE_CV_INPUT, NUM_ROWS),
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	ShiftyExpander() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(CLOCK_RATE_AV_PARAM, -1.f, 1.f, 0.f, "Clock Rate Attenuverter", "%", 0.f, 100.f, 0.f);
		configParam(CLOCK_DIVIDER_AV_PARAM, -1.f, 1.f, 0.f, "Clock Divider Attenuverter", "%", 0.f, 100.f, 0.f);
		configParam(RAMP_AV_PARAM, -1.f, 1.f, 0.f, "Ramp Delay Attenuverter", "%", 0.f, 100.f, 0.f);
		configParam(SAH_AV_PARAM, -1.f, 1.f, 0.f, "Sample & Hold Attenuverter", "%", 0.f, 100.f, 0.f);

		configInput(CLOCK_RATE_CV_INPUT, "Clock Rate CV");
		configInput(CLOCK_DIVIDER_CV_INPUT, "Clock Divider CV");
		configInput(RAMP_CV_INPUT, "Ramp Delay CV");
		configInput(SAH_CV_INPUT, "Sample & Hold CV");

		for(int row = 0; row < NUM_ROWS; row ++){
			std::string rs = std::to_string(row+1);
			configParam(ECHO_AV_PARAM + row, -1.f, 1.f, 0.f, "Echo " + rs + " Attenuverter", "%", 0.f, 100.f, 0.f);
			configParam(MUTE_AV_PARAM + row, -1.f, 1.f, 0.f, "Mute " + rs + " Attenuverter", "%", 0.f, 100.f, 0.f);

			configInput(ECHO_CV_INPUT + row, "Echo " + rs + " CV");
			configInput(MUTE_CV_INPUT + row, "Mute " + rs + " CV");
		}
	}

	void process(const ProcessArgs& args) override {
		bridge.clockRate = 5000.f * params[CLOCK_RATE_AV_PARAM].getValue() * inputs[CLOCK_RATE_CV_INPUT].getVoltage() / 10.f;
		bridge.clockDivider = std::floor(16 * params[CLOCK_DIVIDER_AV_PARAM].getValue() * inputs[CLOCK_DIVIDER_CV_INPUT].getVoltage() / 10.f);

		bridge.ramp = 2.f * params[RAMP_AV_PARAM].getValue() * inputs[RAMP_CV_INPUT].getVoltage() / 10.f;
		bridge.sample_and_hold = params[SAH_AV_PARAM].getValue() * inputs[SAH_CV_INPUT].getVoltage() / 10.f;

		for(int row = 0; row < NUM_ROWS; row ++){
			bridge.echo[row] = 4.f * params[row + ECHO_AV_PARAM].getValue() * inputs[row + ECHO_CV_INPUT].getVoltage() / 10.f;
			bridge.mute[row] = 4.f * params[row + MUTE_AV_PARAM] .getValue()* inputs[row + MUTE_CV_INPUT].getVoltage() / 10.f;
		}
	}
};


struct ShiftyExpanderWidget : ModuleWidget {
	ShiftyExpanderWidget(ShiftyExpander* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ShiftyExpander.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.681, 11.084)), module, ShiftyExpander::CLOCK_RATE_AV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(25.79, 11.084)), module, ShiftyExpander::CLOCK_DIVIDER_AV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.681, 29.076)), module, ShiftyExpander::RAMP_AV_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(25.79, 29.076)), module, ShiftyExpander::SAH_AV_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.438, 10.902)), module, ShiftyExpander::CLOCK_RATE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.546, 10.902)), module, ShiftyExpander::CLOCK_DIVIDER_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.438, 28.893)), module, ShiftyExpander::RAMP_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.546, 28.893)), module, ShiftyExpander::SAH_CV_INPUT));

		//Rows
		const float ROW_YS [] = {48.385,58.969,69.557,80.017,90.723,101.301,111.862};

		for(int row = 0; row < NUM_ROWS; row ++){
			float y = ROW_YS[row] + 0.784f;
			addParam(createParamCentered<Trimpot>(mm2px(Vec(5.681, y)), module, ShiftyExpander::ECHO_AV_PARAM + row));
			addParam(createParamCentered<Trimpot>(mm2px(Vec(25.79, y)), module, ShiftyExpander::MUTE_AV_PARAM + row));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.438, y)), module, ShiftyExpander::ECHO_CV_INPUT + row));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(34.546, y)), module, ShiftyExpander::MUTE_CV_INPUT + row));
		}
	}
};


Model* modelShiftyExpander = createModel<ShiftyExpander, ShiftyExpanderWidget>("ShiftyExpander");