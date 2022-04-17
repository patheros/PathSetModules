#include "plugin.hpp"
#include "util.hpp"

#define LINE_MAX 5

struct Nudge : Module {
	enum ParamId {
		SLEW_PARAM,
		SLEW_AV_PARAM,
		SIZE_PARAM,
		SIZE_AV_PARAM,
		MAX_PARAM,
		MAX_AV_PARAM,
		VEL_PARAM,
		VEL_AV_PARAM,
		OUTPUT_RANGE_PARAM,
		NUDGE_BUTTON_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CV_IN_1_INPUT,
		CV_IN_2_INPUT,
		CV_IN_3_INPUT,
		CV_IN_4_INPUT,
		CV_IN_5_INPUT,
		SLEW_CV_INPUT,
		SIZE_CV_INPUT,
		MAX_CV_INPUT,
		VEL_CV_INPUT,
		NUDGE_TRIGGER_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		CV_OUT_1_OUTPUT,
		CV_OUT_2_OUTPUT,
		CV_OUT_3_OUTPUT,
		CV_OUT_4_OUTPUT,
		CV_OUT_5_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	//Non Persisted Data

	bool nudgeButtonDown;
	bool nudgeTriggerHigh;

	//Persisted Data

	struct Line{
		float amt;
		float vel;
		float delta;

		json_t *dataToJson(){
			json_t *jobj = json_object();

			json_object_set_new(jobj, "amt", json_real(amt));
			json_object_set_new(jobj, "vel", json_real(vel));
			json_object_set_new(jobj, "delta", json_real(delta));

			return jobj;
		}

		void dataFromJson(json_t *jobj) {			
			amt = json_real_value(json_object_get(jobj, "amt"));
			vel = json_real_value(json_object_get(jobj, "vel"));
			delta = json_real_value(json_object_get(jobj, "delta"));
		}
	};

	Line lines [LINE_MAX];

	int nudging;


	Nudge() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(SLEW_PARAM, 0.f, 10.f, 5.f, "Slew", " ms", 0.f, 1000.f, 0.f);
		configParam(SLEW_AV_PARAM, -1.f, 1.f, 0.f, "Slew Attenuverter", "%", 0.f, 100.f, 0.f);
		configInput(SLEW_CV_INPUT, "Slew CV");

		configParam(SIZE_PARAM, 0.f, 5.f, 2.5f, "Step", " V");
		configParam(SIZE_AV_PARAM, -1.f, 1.f, 0.f, "Step Attenuverter", "%", 0.f, 100.f, 0.f);
		configInput(SIZE_CV_INPUT, "Step CV");

		configParam(MAX_PARAM, 0.f, 10.f, 5.f, "Range", " V");
		configParam(MAX_AV_PARAM, -1.f, 1.f, 0.f, "Range Attenuverter", "%", 0.f, 100.f, 0.f);
		configInput(MAX_CV_INPUT, "Range CV");

		configParam(VEL_PARAM, -1.f, 1.f, 0.f, "Velocity");
		configParam(VEL_AV_PARAM, -1.f, 1.f, 0.f, "Velocity Attenuverter", "%", 0.f, 100.f, 0.f);
		configInput(VEL_CV_INPUT, "");

		configSwitch(OUTPUT_RANGE_PARAM, 0.f, 2.f, 1.f, "Range", std::vector<std::string>{"Negative - Unipolar","Bipolar","Positive - Unipolar"});
		configButton(NUDGE_BUTTON_PARAM, "Nudge");

		configInput(CV_IN_1_INPUT, "CV 1");
		configInput(CV_IN_2_INPUT, "CV 2");
		configInput(CV_IN_3_INPUT, "CV 3");
		configInput(CV_IN_4_INPUT, "CV 4");
		configInput(CV_IN_5_INPUT, "CV 5");

		configInput(NUDGE_TRIGGER_INPUT, "Nudge");

		configOutput(CV_OUT_1_OUTPUT, "CV 1");
		configOutput(CV_OUT_2_OUTPUT, "CV 2");
		configOutput(CV_OUT_3_OUTPUT, "CV 3");
		configOutput(CV_OUT_4_OUTPUT, "CV 4");
		configOutput(CV_OUT_5_OUTPUT, "CV 5");

		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	void initalize(){
		nudgeButtonDown = false;
		nudgeTriggerHigh = false;

		for(int li = 0; li < LINE_MAX; li++){
			lines[li] = Line();
		}

		nudging = 0;
	}

	json_t *dataToJson() override{
		json_t *jobj = json_object();

		json_object_set_new(jobj, "version", json_integer(1));
			
		json_t *linesJ = json_array();
		for(int li = 0; li < LINE_MAX; li++){
			json_array_insert_new(linesJ, li, lines[li].dataToJson());
		}
		json_object_set_new(jobj, "lines", linesJ);

		json_object_set_new(jobj, "nudging", json_integer(nudging));

		return jobj;
	}

	void dataFromJson(json_t *jobj) override {			
		json_t *linesJ = json_object_get(jobj,"lines");
		for(int li = 0; li < LINE_MAX; li++){
			lines[li].dataFromJson(json_array_get(linesJ,li));
		}
		nudging = json_integer_value(json_object_get(jobj, "nudging"));
	}

	void process(const ProcessArgs& args) override {

		bool nudgeEvent = false;
		if(schmittTrigger(nudgeTriggerHigh,inputs[NUDGE_TRIGGER_INPUT].getVoltage()))
			nudgeEvent = true;
		if(buttonTrigger(nudgeButtonDown,params[NUDGE_TRIGGER_INPUT].getValue()))
			nudgeEvent = true;

		float max_nudge = params[MAX_PARAM].getValue() + params[MAX_AV_PARAM].getValue() * inputs[MAX_CV_INPUT].getVoltage(); 
		if(max_nudge < 0) max_nudge = 0;

		if(nudgeEvent){
			float slew = params[SLEW_PARAM].getValue() + params[SLEW_AV_PARAM].getValue() * inputs[SLEW_CV_INPUT].getVoltage(); 
			float velocity = params[VEL_PARAM].getValue() + params[VEL_AV_PARAM].getValue() * inputs[VEL_CV_INPUT].getVoltage() / 5.f; 
			
			float size = params[SIZE_PARAM].getValue() + params[SIZE_AV_PARAM].getValue() * inputs[SIZE_CV_INPUT].getVoltage(); 
			if(size < 0) size = 0;

			nudging = std::floor(args.sampleRate * slew);
			if(nudging < 1) nudging = 1;

			for(int li = 0; li < LINE_MAX; li++){
				Line & line = lines[li];
				float offset = 0;
				offset = line.amt * velocity;
				float prev_amt = line.amt;
				if(prev_amt > max_nudge) offset = -1;
				if(prev_amt < -max_nudge) offset = 1;
				float fullDelta = size * (rack::random::uniform() * 2 - 1 + offset);
				line.delta = fullDelta / (float)nudging;
				if(fullDelta > 0){
					line.vel = 1;
				}else if(fullDelta < 0){
					line.vel = -1;
				}else{
					line.vel = 0;
				}
			}
		}

		if(nudging > 0){
			nudging --;

			for(int li = 0; li < LINE_MAX; li++){
				lines[li].amt += lines[li].delta;
			}
		}

		int range = static_cast<int>(params[OUTPUT_RANGE_PARAM].getValue());

		for(int li = 0; li < LINE_MAX; li++){
			float value = inputs[CV_IN_1_INPUT + li].getVoltage();
			float nudge = clamp(lines[li].amt,-max_nudge,max_nudge);
			switch(range){
				case 0:
					nudge = -std::abs(nudge);
					break;
				case 1:
					// nudge = nudge;
					break;
				case 2:
					nudge = std::abs(nudge);
					break;
			}
			value += nudge;
			outputs[CV_OUT_1_OUTPUT + li].setVoltage(value);
		}
	}
};


struct NudgeWidget : ModuleWidget {
	NudgeWidget(Nudge* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Nudge.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.239, 78.388)), module, Nudge::SLEW_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.239, 87.744)), module, Nudge::SLEW_AV_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(17.822, 81.978)), module, Nudge::SIZE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(17.822, 91.334)), module, Nudge::SIZE_AV_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(28.406, 85.568)), module, Nudge::MAX_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(28.406, 94.924)), module, Nudge::MAX_AV_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(38.989, 89.158)), module, Nudge::VEL_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(38.989, 98.514)), module, Nudge::VEL_AV_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(42.184, 70.905)), module, Nudge::OUTPUT_RANGE_PARAM));
		{
			CKD6 * button = createParamCentered<CKD6>(mm2px(Vec(12.415, 113.447)), module, Nudge::NUDGE_BUTTON_PARAM);
			button->momentary = true;
			button->latch = true;
			addParam(button);
		}

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.318, 17.194)), module, Nudge::CV_IN_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(11.348, 27.177)), module, Nudge::CV_IN_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(16.056, 37.113)), module, Nudge::CV_IN_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12.534, 47.816)), module, Nudge::CV_IN_4_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.437, 57.638)), module, Nudge::CV_IN_5_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.239, 97.1)), module, Nudge::SLEW_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.822, 100.69)), module, Nudge::SIZE_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.406, 104.28)), module, Nudge::MAX_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(38.989, 107.87)), module, Nudge::VEL_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 114.315)), module, Nudge::NUDGE_TRIGGER_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.051186, 8.5919476)), module, Nudge::CV_OUT_1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(36.729866, 23.027697)), module, Nudge::CV_OUT_2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(40.810059, 37.993649)), module, Nudge::CV_OUT_3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(35.072895, 53.077431)), module, Nudge::CV_OUT_4_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(27.557224, 66.25251)), module, Nudge::CV_OUT_5_OUTPUT));
	}
};


Model* modelNudge = createModel<Nudge, NudgeWidget>("Nudge");