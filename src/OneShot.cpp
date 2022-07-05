#include "plugin.hpp"
#include "util.hpp"
#include "cvRange.hpp"

//Mesured in Samples
#define LATE_WINDOW_LENGTH 100 

struct OneShot : Module {
	enum ParamId {
		STEP1_PARAM,
		STEP2_PARAM,
		STEP3_PARAM,
		STEP4_PARAM,
		LENGTH_PARAM,
		STABLE_PARAM,
		CHANCE_PARAM,
		HEAT_PARAM,
		START_BTN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		START_INPUT,
		CLOCK_INPUT,
		GATE_IN_INPUT,
		CV_IN_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		EOC_OUTPUT,
		ACTIVE_GATE_OUTPUT,
		GATE_OUT_OUTPUT,
		CV_OUT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(STEP1_LIGHT,3),
		ENUMS(STEP2_LIGHT,3),
		ENUMS(STEP3_LIGHT,3),
		ENUMS(STEP4_LIGHT,3),
		LIGHTS_LEN
	};

	//Non Persistant State
	bool startTrigHigh;
	bool startBtnDown;
	bool clockHigh;

	int lateArmWindow;

	//Persistant State
	enum State{
		READY,
		ARMED,
		PLAYING,
		EOC,
	};

	State state;
	unsigned int playStep;
	unsigned int noteStep;

	int noteToPlay;

	//Context Menu State
	CVRange range;

	OneShot() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam<CVRangeParamQuantity<OneShot>>(STEP1_PARAM, 0.f, 1.f, 0.5f, "CV 1", "V");
		configParam<CVRangeParamQuantity<OneShot>>(STEP2_PARAM, 0.f, 1.f, 0.5f, "CV 2", "V");
		configParam<CVRangeParamQuantity<OneShot>>(STEP3_PARAM, 0.f, 1.f, 0.5f, "CV 3", "V");
		configParam<CVRangeParamQuantity<OneShot>>(STEP4_PARAM, 0.f, 1.f, 0.5f, "CV 4", "V");
		configParam(LENGTH_PARAM, 1, 16, 4, "Length");
		configSwitch(STABLE_PARAM, 0.f, 1.f, 0.f, "Stable", std::vector<std::string>{"Stable","Unstable"});
		configParam(CHANCE_PARAM, 0.f, 1.f, 0.f, "Random", "%", 0.f, 100.f, 0.f);
		configParam(HEAT_PARAM, 0.f, 1.f, 0.f, "Heat", "%", 0.f, 100.f, 0.f);
		configInput(START_INPUT, "Start");
		configInput(CLOCK_INPUT, "Clock");
		configInput(GATE_IN_INPUT, "Gate");
		configInput(CV_IN_INPUT, "CV");
		configOutput(EOC_OUTPUT, "End of Cycle");
		configOutput(ACTIVE_GATE_OUTPUT, "Active Gate");
		configOutput(GATE_OUT_OUTPUT, "Gate");
		configOutput(CV_OUT_OUTPUT, "CV");
		initalize();
	}

	void onReset(const ResetEvent& e) override{
		Module::onReset(e);
		initalize();
	}

	void initalize(){
		startTrigHigh = false;
		startBtnDown = false;
		clockHigh = false;

		lateArmWindow = 0;

		state = READY;
		playStep = 0;
		noteStep = 0;

		noteToPlay = 0;

		range = Bipolar_1;
	}

	json_t *dataToJson() override{
		json_t *jobj = json_object();

		json_object_set_new(jobj, "state", json_integer(state));
		json_object_set_new(jobj, "playStep", json_integer(playStep));
		json_object_set_new(jobj, "noteStep", json_integer(noteStep));
		json_object_set_new(jobj, "noteToPlay", json_integer(noteToPlay));
		json_object_set_new(jobj, "range", json_integer(range));

		return jobj;
	}

	void dataFromJson(json_t *jobj) override {					
		state = (State)json_integer_value(json_object_get(jobj, "state"));	
		playStep = json_integer_value(json_object_get(jobj, "playStep"));	
		noteStep = json_integer_value(json_object_get(jobj, "noteStep"));	
		noteToPlay = json_integer_value(json_object_get(jobj, "noteToPlay"));	
		range = (CVRange)json_integer_value(json_object_get(jobj, "range"));	
	}

	void process(const ProcessArgs& args) override {
		float clockIn;
		if(inputs[CLOCK_INPUT].isConnected()){
			clockIn = inputs[CLOCK_INPUT].getVoltage();
		}else{
			clockIn = inputs[GATE_IN_INPUT].getVoltage();
		}
		bool clockEvent = schmittTrigger(clockHigh,clockIn);

		bool startEvent = schmittTrigger(startTrigHigh,inputs[START_INPUT].getVoltage());
		if(buttonTrigger(startBtnDown,params[START_BTN_PARAM].getValue())){
			startEvent = true;
		}		

		if(startEvent){
			if(state == READY || state == EOC){
				state = ARMED;
				if(lateArmWindow > 0) clockEvent = true;
			}
		}

		if(lateArmWindow > 0) lateArmWindow--;

		if(clockEvent){

			bool stable = params[STABLE_PARAM].getValue() == 0;
			float chance = params[CHANCE_PARAM].getValue();
			float heat = params[HEAT_PARAM].getValue();
			int seqLength = static_cast<int>(params[LENGTH_PARAM].getValue());

			bool shunt = false;
			bool flip = false;

			bool isHeat = false;

			lateArmWindow = LATE_WINDOW_LENGTH;

			if(state == ARMED){
				state = PLAYING;
				playStep = 0;
				if(stable) noteStep = 0;
			}

			int seqLengthBig = seqLength + (stable ? 0 : 1);
			int stableNote = static_cast<int>(noteStep / (float)seqLengthBig * 4) % 4;
			//DEBUG("playStep:%i seqLength:%i seqLengthBig:%i",playStep,seqLength,seqLengthBig);

			bool canRnd = false;
			if(playStep == 0 && heat <= 0.5f){
				noteToPlay = 0; //First Note
			}else if(seqLength > 1 && playStep == static_cast<unsigned int>(seqLength - 1) && heat <= 0.5f){
				noteToPlay = 3; //Last Note
			}else if(playStep >= static_cast<unsigned int>(seqLength)){
				noteToPlay = -1; //Past Seq, Don't play a note
			}else{
				int maxHeatSpots = seqLengthBig - 2;
				float val = (noteStep % maxHeatSpots) / (float)maxHeatSpots;
				//DEBUG("val:%f heat:%f noteStep:%i",val,heat,noteStep);
				isHeat = val < heat * 2;
				float heat2 = heat - 0.5f;
				shunt = fmod(val * 7649.f, 1.f) < heat2;
				flip = fmod(val * 137.f, 1.f) < heat2;
				if(isHeat){
					noteToPlay = noteStep % 4;
				}else{
					noteToPlay = stableNote;
				}
				canRnd = true;
			}

			if(seqLengthBig <= 2) canRnd = true;

			bool isRnd = false;
			if(canRnd){
				if(rack::random::uniform() < chance - 0.5f){
					shunt = !shunt;
					isRnd = true;
				}
				if(rack::random::uniform() < chance / 2.f){
					flip = !flip;
					isRnd = true;
				}
			}

			if(shunt) noteToPlay = (noteToPlay + 1) % 4;
			if(flip) noteToPlay = 3 - noteToPlay;

			//Don't display red if we ended on the same note anyways
			if(noteToPlay == stableNote) isHeat = false;

			if(state == PLAYING){
				if(playStep >= static_cast<unsigned int>(seqLength)){
					state = EOC;
				}else{
					playStep ++;
					noteStep ++;
				}
			}else if(state == EOC){
				state = READY;
			}else{
				noteToPlay = -1;
			}

			//Update Lights
			for(int i = 0; i < 4; i ++){
				int idx = i * 3;
				bool on = i == noteToPlay;
				lights[STEP1_LIGHT + idx + 0].setBrightness(on && !isRnd && !isHeat);
				lights[STEP1_LIGHT + idx + 1].setBrightness(on && isRnd);
				lights[STEP1_LIGHT + idx + 2].setBrightness(on && !isRnd && isHeat);
			}

			outputs[EOC_OUTPUT].setVoltage(state == EOC ? 10 : 0);
		}

		if(state == PLAYING){
			outputs[CV_OUT_OUTPUT].setVoltage(mapCVRange(params[STEP1_PARAM + noteToPlay].getValue(),range));
			outputs[GATE_OUT_OUTPUT].setVoltage(clockIn);
			outputs[ACTIVE_GATE_OUTPUT].setVoltage(clockIn);
		}else{
			outputs[CV_OUT_OUTPUT].setVoltage(inputs[CV_IN_INPUT].getVoltage());
			outputs[GATE_OUT_OUTPUT].setVoltage(inputs[GATE_IN_INPUT].getVoltage());
			outputs[ACTIVE_GATE_OUTPUT].setVoltage(0);
		}
	}
};

static const NVGcolor GRAY = nvgRGB(0x89, 0x89, 0x89);

static const NVGcolor PS_BLUE = nvgRGB(0x28, 0xb8, 0xf6);
static const NVGcolor PS_ORANGE = nvgRGB(0xe0, 0x9b, 0x77);
static const NVGcolor PS_PINK = nvgRGB(0xe6, 0x74, 0xbf);

struct PathSetColors : GrayModuleLightWidget {
	PathSetColors() {
		addBaseColor(PS_BLUE);
		addBaseColor(PS_ORANGE);
		addBaseColor(PS_PINK);
	}
};

template <typename TBase>
struct TrimpotRingLight : TBase {
	TrimpotRingLight() {
		this->bgColor = GRAY;
		this->box.size = mm2px(math::Vec(8.0, 8.0));
	}
	void drawBackground(const widget::Widget::DrawArgs& args) override {
		// Derived from LightWidget::drawBackground()

		nvgBeginPath(args.vg);
		float radiusA = std::min(this->box.size.x, this->box.size.y) / 2.0;
		nvgCircle(args.vg, radiusA, radiusA, radiusA * 1.05f);

		// Background
		if (this->bgColor.a > 0.0) {
			nvgFillColor(args.vg, this->bgColor);
			nvgFill(args.vg);
		}

		// Border
		if (this->borderColor.a > 0.0) {
			nvgStrokeWidth(args.vg, 0.5);
			nvgStrokeColor(args.vg, this->borderColor);
			nvgStroke(args.vg);
		}
	}

	void drawLight(const widget::Widget::DrawArgs& args) override {
		// Derived from LightWidget::drawLight()

		// Foreground
		if (this->color.a > 0.0) {
			nvgBeginPath(args.vg);

			float radiusA = std::min(this->box.size.x, this->box.size.y) / 2.0;
			float radiusB = radiusA * 0.75f;
			
			nvgFillColor(args.vg, this->color);

			nvgBeginPath(args.vg);
			nvgArc(args.vg, radiusA, radiusA, radiusA, 3.2f, 0, NVGsolidity::NVG_SOLID);
			nvgArc(args.vg, radiusA, radiusA, radiusB, 0, 3.2f, NVGsolidity::NVG_HOLE);			
			nvgFill(args.vg);

			nvgBeginPath(args.vg);
			nvgArc(args.vg, radiusA, radiusA, radiusA, 6.3f, 3.1f, NVGsolidity::NVG_SOLID);
			nvgArc(args.vg, radiusA, radiusA, radiusB, 3.1f, 6.3f, NVGsolidity::NVG_HOLE);
			nvgFill(args.vg);
		}
	}
};

//


struct OneShotWidget : ModuleWidget {
	OneShotWidget(OneShot* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OneShot.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addChild(createLightCentered<TrimpotRingLight<PathSetColors>>(mm2px(Vec(20.595, 10.668)), module, OneShot::STEP1_LIGHT));
		addChild(createLightCentered<TrimpotRingLight<PathSetColors>>(mm2px(Vec(20.595, 20.546)), module, OneShot::STEP2_LIGHT));
		addChild(createLightCentered<TrimpotRingLight<PathSetColors>>(mm2px(Vec(20.595, 30.424)), module, OneShot::STEP3_LIGHT));
		addChild(createLightCentered<TrimpotRingLight<PathSetColors>>(mm2px(Vec(20.595, 40.302)), module, OneShot::STEP4_LIGHT));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 10.668)), module, OneShot::STEP1_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 20.546)), module, OneShot::STEP2_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 30.424)), module, OneShot::STEP3_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 40.302)), module, OneShot::STEP4_PARAM));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 10.668)), module, OneShot::STEP1_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 20.546)), module, OneShot::STEP2_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 30.424)), module, OneShot::STEP3_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.595, 40.302)), module, OneShot::STEP4_PARAM));
		addParam(createParamCentered<RotarySwitch<Trimpot>>(mm2px(Vec(5.25, 54.273)), module, OneShot::LENGTH_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(20.596, 54.273)), module, OneShot::STABLE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.25, 68.996)), module, OneShot::CHANCE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.596, 68.996)), module, OneShot::HEAT_PARAM));

		{
			auto btn = createParamCentered<TL1105>(mm2px(Vec(9.1110239, 3.7372425)), module, OneShot::START_BTN_PARAM);
			btn->momentary = true;
			btn->latch = false;
			addParam(btn);
		}

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.111, 10.935)), module, OneShot::START_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.25, 86.047)), module, OneShot::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.228, 100.068)), module, OneShot::GATE_IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.228, 109.593)), module, OneShot::CV_IN_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(9.111, 40.302)), module, OneShot::EOC_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.574, 86.047)), module, OneShot::ACTIVE_GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.574, 100.068)), module, OneShot::GATE_OUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.574, 109.593)), module, OneShot::CV_OUT_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		auto module = dynamic_cast<OneShot*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("OneShot"));

		addRangeSelectMenu<OneShot>(module,menu);
	}
};

Model* modelOneShot = createModel<OneShot, OneShotWidget>("OneShot");