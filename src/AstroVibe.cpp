/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include "plugin.hpp"
#include "util.hpp"

#define ROW_COUNT 3
#define MAX_CHANNELS 16
#define TWO_PI 6.28318530718f
#define PI     3.14159265358f
#define GRAVITY_VALUE_SIZE 13

const float gravityValue [GRAVITY_VALUE_SIZE][2] = {
	{0,0},

	{5,0},
	{0,5},
	{-5,0},
	{0,-5},

	{5,5},
	{-5,-5},
	{5,-5},
	{-5,5},

	{50,50},
	{-50,-50},
	{50,-50},
	{-50,50},
};

struct AstroVibe : Module {
	enum ParamId {
		TRAVEL_BUTTON_1_PARAM,
		TRAVEL_BUTTON_2_PARAM,
		TRAVEL_BUTTON_3_PARAM,

		FREQ_KNOB_1_PARAM,
		FREQ_KNOB_2_PARAM,
		FREQ_KNOB_3_PARAM,

		TIMBRE_KNOB_1_PARAM,
		TIMBRE_KNOB_2_PARAM,
		TIMBRE_KNOB_3_PARAM,

		ENGINE_SWITCH_1_PARAM,
		ENGINE_SWITCH_2_PARAM,
		ENGINE_SWITCH_3_PARAM,

		FLAVOR_SWITCH_1_PARAM,
		FLAVOR_SWITCH_2_PARAM,
		FLAVOR_SWITCH_3_PARAM,

		SPEED_SWITCH_1_PARAM,
		SPEED_SWITCH_2_PARAM,
		SPEED_SWITCH_3_PARAM,

		LEVEL_1_PARAM,
		LEVEL_2_PARAM,
		LEVEL_3_PARAM,
		
		PARAMS_LEN
	};
	enum InputId {
		TRAVEL_TRIG_1_INPUT,
		TRAVEL_TRIG_2_INPUT,
		TRAVEL_TRIG_3_INPUT,

		CLOCK_1_INPUT,
		CLOCK_2_INPUT,
		CLOCK_3_INPUT,

		FREQ_CV_1_INPUT,
		FREQ_CV_2_INPUT,
		FREQ_CV_3_INPUT,

		TIMBRE_CV_1_INPUT,
		TIMBRE_CV_2_INPUT,
		TIMBRE_CV_3_INPUT,

		SPIN_1_INPUT,		
		SPIN_2_INPUT,
		SPIN_3_INPUT,

		ENGINE_CV_1_INPUT,
		ENGINE_CV_2_INPUT,
		ENGINE_CV_3_INPUT,

		FLAVOR_CV_1_INPUT,
		FLAVOR_CV_2_INPUT,
		FLAVOR_CV_3_INPUT,

		INPUTS_LEN
	};
	enum OutputId {

		LEFT_MASTER_OUTPUT,
		RIGHT_MASTER_OUTPUT,
		
		LEFT_1_OUTPUT,
		LEFT_2_OUTPUT,
		LEFT_3_OUTPUT,
		RIGHT_1_OUTPUT,		
		RIGHT_2_OUTPUT,		
		RIGHT_3_OUTPUT,

		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	enum Mode{
		BlackHole,
		Atomic,		
	};

	enum Flavor{
		Tones, 
		Notes,
	};

	enum Speed{
		LFO,
		Audible,
	};

	struct Row{
		struct Engine{
			bool clockTriggerHigh;

			int stepCnt;
			unsigned int stepIndex;

			float outputValue [2];
			float internalState [2];
			float outputHistory [2];

			float frameDrop;

			float gv [2];

			float modeCycle;
			bool engineFlip;
			bool flavorFlip;

			json_t *dataToJson() {
				json_t *engineJ = json_object();
					
				json_object_set_new(engineJ, "clockTriggerHigh", json_bool(clockTriggerHigh));

				json_object_set_new(engineJ, "stepCnt", json_integer(stepCnt));
				json_object_set_new(engineJ, "stepIndex", json_integer(stepIndex));

				json_object_set_new(engineJ, "outputValue.0", json_real(outputValue[0]));
				json_object_set_new(engineJ, "outputValue.1", json_real(outputValue[1]));
				json_object_set_new(engineJ, "internalState.0", json_real(internalState[0]));
				json_object_set_new(engineJ, "internalState.1", json_real(internalState[1]));
				json_object_set_new(engineJ, "outputHistory.0", json_real(outputHistory[0]));
				json_object_set_new(engineJ, "outputHistory.1", json_real(outputHistory[1]));

				json_object_set_new(engineJ, "frameDrop", json_real(frameDrop));

				json_object_set_new(engineJ, "gv.0", json_real(gv[0]));
				json_object_set_new(engineJ, "gv.1", json_real(gv[1]));

				json_object_set_new(engineJ, "modeCycle", json_real(modeCycle));
				json_object_set_new(engineJ, "engineFlip", json_bool(engineFlip));
				json_object_set_new(engineJ, "flavorFlip", json_bool(flavorFlip));
				return engineJ;
			}

			void dataFromJson(json_t *engineJ) {					
				clockTriggerHigh = json_is_true(json_object_get(engineJ, "clockTriggerHigh"));

				stepCnt = json_real_value(json_object_get(engineJ, "stepCnt"));
				stepIndex = json_real_value(json_object_get(engineJ, "stepIndex"));

				outputValue[0] = json_real_value(json_object_get(engineJ, "outputValue.0"));
				outputValue[1] = json_real_value(json_object_get(engineJ, "outputValue.1"));
				internalState[0] = json_real_value(json_object_get(engineJ, "internalState.0"));
				internalState[1] = json_real_value(json_object_get(engineJ, "internalState.1"));
				outputHistory[0] = json_real_value(json_object_get(engineJ, "outputHistory.0"));
				outputHistory[1] = json_real_value(json_object_get(engineJ, "outputHistory.1"));

				frameDrop = json_real_value(json_object_get(engineJ, "frameDrop"));

				gv[0] = json_real_value(json_object_get(engineJ, "gv.0"));
				gv[1] = json_real_value(json_object_get(engineJ, "gv.1"));

				modeCycle = json_real_value(json_object_get(engineJ, "modeCycle"));
				engineFlip = json_is_true(json_object_get(engineJ, "engineFlip"));
				flavorFlip = json_is_true(json_object_get(engineJ, "flavorFlip"));
			}
		};
		Engine engines [MAX_CHANNELS];
		
		bool resetTriggerHigh;
		bool resetButtonHigh;
		std::vector<int> sequence;

		json_t *dataToJson() {
			json_t *rowJ = json_object();
				
			json_t *enginesJ = json_array();
			for(int ei = 0; ei < MAX_CHANNELS; ei++){
				json_array_insert_new(enginesJ, ei, engines[ei].dataToJson());
			}
			json_object_set_new(rowJ, "engines", enginesJ);

			json_object_set_new(rowJ, "resetTriggerHigh", json_bool(resetTriggerHigh));
			json_object_set_new(rowJ, "resetButtonHigh", json_bool(resetButtonHigh));

			json_t *sequenceJ = json_array();
			int sequenceLength = sequence.size();
			for(int si = 0; si < sequenceLength; si++){
				json_array_insert_new(sequenceJ, si, json_integer(sequence[si]));
			}
			json_object_set_new(rowJ, "sequence", sequenceJ);

			return rowJ;
		}

		void dataFromJson(json_t *rowJ) {
			json_t *enginesJ = json_object_get(rowJ, "engines");
			for(int ei = 0; ei < MAX_CHANNELS; ei++){
				engines[ei].dataFromJson(json_array_get(enginesJ,ei));
			}

			resetTriggerHigh = json_is_true(json_object_get(rowJ, "resetTriggerHigh"));
			resetButtonHigh = json_is_true(json_object_get(rowJ, "resetButtonHigh"));

			sequence.clear();
			json_t *sequenceJ = json_object_get(rowJ, "sequence");
			int sequenceLength = json_array_size(sequenceJ);
			for(int si = 0; si < sequenceLength; si++){
				sequence.push_back(json_integer_value(json_array_get(sequenceJ, si)));
			}
		}
	};

	Row rows [ROW_COUNT];

	bool internalRoutingEnabled = true;

	AstroVibe() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configOutput(LEFT_MASTER_OUTPUT, "Left Master");
		configOutput(RIGHT_MASTER_OUTPUT, "Right Master");

		for(int i = 0; i < ROW_COUNT; i++){
			std::string is = std::to_string(i+1);
			configParam(TRAVEL_BUTTON_1_PARAM + i, 0.f, 1.f, 0.f, "New Planet " + is);
			configParam(FREQ_KNOB_1_PARAM + i, 0.0001f, 4.f, 1.f, "Frequency " + is);
			configParam(TIMBRE_KNOB_1_PARAM + i, 0.f, 1.f, 0.5f, "Warp " + is);
			configParam(LEVEL_1_PARAM + i, -1.f, 1.f, 1.f, "Gain " + is);
			configSwitch(ENGINE_SWITCH_1_PARAM + i, 0.f, 1.f, 0.f, "Engine " + is, std::vector<std::string>{"Black Hole","Atomic"});
			configSwitch(FLAVOR_SWITCH_1_PARAM + i, 0.f, 1.f, 0.f, "Waveform " + is, std::vector<std::string>{"Tones","Notes"});
			configSwitch(SPEED_SWITCH_1_PARAM + i, 0.f, 1.f, 1.f, "Speed " + is, std::vector<std::string>{"LFO","Audible"});
			configInput(ENGINE_CV_1_INPUT + i, "Engine Flip Gate " + is);
			configInput(FLAVOR_CV_1_INPUT + i, "Waveform Flip Gate " + is);
			configInput(TRAVEL_TRIG_1_INPUT + i, "New Planet Trigger " + is);
			configInput(CLOCK_1_INPUT + i, "Clock " + is);
			configInput(FREQ_CV_1_INPUT + i, "V/oct " + is);
			configInput(TIMBRE_CV_1_INPUT + i, "Size CV " + is); 
			configInput(SPIN_1_INPUT + i, "Spin CV " + is);
			configOutput(LEFT_1_OUTPUT + i, "Left Output " + is);
			configOutput(RIGHT_1_OUTPUT + i, "Right Output " + is);
		}
		resetState();
	}

	void resetState(){		
		for(int ri = 0; ri < ROW_COUNT; ri++){
			for(int ei = 0; ei < MAX_CHANNELS; ei++){
				rows[ri].engines[ei].clockTriggerHigh = false;
				rows[ri].engines[ei].stepCnt = 0;
				rows[ri].engines[ei].stepIndex = 0;
				
				rows[ri].engines[ei].outputValue[0] = 0;
				rows[ri].engines[ei].outputValue[1] = 0;
				rows[ri].engines[ei].internalState[0] = 0;
				rows[ri].engines[ei].internalState[1] = 0;
				rows[ri].engines[ei].outputHistory[0] = 0;
				rows[ri].engines[ei].outputHistory[1] = 0;

				rows[ri].engines[ei].frameDrop = 0;

				rows[ri].engines[ei].gv[0] = 0;
				rows[ri].engines[ei].gv[1] = 0;

				rows[ri].engines[ei].modeCycle = 0;
				rows[ri].engines[ei].engineFlip = false;
				rows[ri].engines[ei].flavorFlip = false;
			}			
			rows[ri].resetTriggerHigh = false;
			rows[ri].resetButtonHigh = false;
			pickNewSequence(ri);
		}
		internalRoutingEnabled = true;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);

		resetState();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "version", json_string("2.1.0"));

		json_t *rowsJ = json_array();
		for(int ri = 0; ri < ROW_COUNT; ri++){
			json_array_insert_new(rowsJ, ri, rows[ri].dataToJson());
		}
		json_object_set_new(rootJ, "rows", rowsJ);

		json_object_set_new(rootJ, "internalRoutingEnabled", json_bool(internalRoutingEnabled));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *rowsJ = json_object_get(rootJ, "rows");
		for(int ri = 0; ri < ROW_COUNT; ri++){
			rows[ri].dataFromJson(json_array_get(rowsJ, ri));
		}

		internalRoutingEnabled = json_is_true(json_object_get(rootJ, "internalRoutingEnabled"));
	}

	void process(const ProcessArgs& args) override {
		Speed rowSpeed [ROW_COUNT];

		float masterOut [MAX_CHANNELS][2] = {0,0};

		unsigned int maxChannelsUsed = 1;

		for(int ri = 0; ri < ROW_COUNT; ri++){

			Speed speed = params[SPEED_SWITCH_1_PARAM + ri].getValue() > 0 ? Audible : LFO;

			rowSpeed[ri] = speed;

			bool doTravel = false;
			{
				float is = params[TRAVEL_BUTTON_1_PARAM + ri].getValue();
				bool was = rows[ri].resetButtonHigh;
				if(is > 0 && !was){
					rows[ri].resetButtonHigh = true;
					doTravel = true;
				}else if(is <= 0 && was){
					rows[ri].resetButtonHigh = false;
				}
			}
			{
				float is = inputs[TRAVEL_TRIG_1_INPUT + ri].getVoltageSum();
				bool was = rows[ri].resetTriggerHigh;
				if(is > 2.0f && !was){
					rows[ri].resetTriggerHigh = true;
					doTravel = true;
				}else if(is <= 0.1f && was){
					rows[ri].resetTriggerHigh = false;
				}
			}


			if(doTravel) pickNewSequence(ri);


			float knobTones = params[FREQ_KNOB_1_PARAM + ri].getValue();
			float knobColor = params[TIMBRE_KNOB_1_PARAM + ri].getValue();

			int channels = 1;
			for(int ri2 = ri; ri2 >= 0; ri2--){
				if(inputs[CLOCK_1_INPUT + ri2].isConnected()){
					channels = std::max(channels,inputs[CLOCK_1_INPUT + ri2].getChannels());
					break;
				}	
				if(!internalRoutingEnabled) break;	
			}
			if(internalRoutingEnabled){
				for(int ri2 = ri; ri2 >= 0; ri2--){
					if(inputs[FREQ_CV_1_INPUT + ri2].isConnected()){
						channels = std::max(channels,inputs[FREQ_CV_1_INPUT + ri2].getChannels());
						break;
					}		
				}
			}
			unsigned int engineCount = (unsigned int)channels;

			maxChannelsUsed = std::max(maxChannelsUsed,engineCount);

			for(unsigned int ei = 0; ei < engineCount; ei++){

				AstroVibe::Row::Engine* e = &rows[ri].engines[ei];

				Mode mode = ((params[ENGINE_SWITCH_1_PARAM + ri].getValue() > 0) != e->engineFlip) ? Atomic : BlackHole;
				Flavor flavor = ((params[FLAVOR_SWITCH_1_PARAM + ri].getValue() > 0) != e->flavorFlip) ? Notes : Tones;

				float freqCV = 0;
				for(int ri2 = ri; ri2 >= 0; ri2--){
					if(inputs[FREQ_CV_1_INPUT + ri2].isConnected()){
						freqCV = inputs[FREQ_CV_1_INPUT + ri2].getPolyVoltage(ei);
						break;
					}
					if(!internalRoutingEnabled) break;
				}

				float colorCV = 0;
				for(int ri2 = ri; ri2 >= 0; ){
					if(inputs[TIMBRE_CV_1_INPUT + ri2].isConnected()){
						colorCV = inputs[TIMBRE_CV_1_INPUT + ri2].getPolyVoltage(ei);
						break;
					}
					if(!internalRoutingEnabled) break;
					if(ri2 == 0) break;
					ri2--;
					if(rowSpeed[ri2] == LFO){
						colorCV = outputs[LEFT_1_OUTPUT + ri2].getPolyVoltage(ei);
						break;
					}
				}				

				//if(flavor == Notes || mode == BlackHole){
				if(flavor == Notes){
					float clockValue = 0;
					for(int ri2 = ri; ri2 >= 0; ri2--){

						if(inputs[CLOCK_1_INPUT + ri2].isConnected()){
							clockValue = inputs[CLOCK_1_INPUT + ri2].getPolyVoltage(ei);
							break;
						}
						if(!internalRoutingEnabled) break;
					}

					if(!e->clockTriggerHigh && clockValue > 2.0f){
						e->clockTriggerHigh = true;
						
						e->stepCnt = 0;
						if(flavor == Notes){
							//In smoth mode just go to the next planet with each clock
							e->stepIndex ++;
						}else{
							//In cruncy mode reset to first planet on clock
							e->stepIndex = 0;
						}
					}else if(e->clockTriggerHigh && clockValue < 0.1f){
						e->clockTriggerHigh = false;
					}
				}

				//if(speed == LFO) frameLength *= 1000;

				float tone = knobTones * pow(2,freqCV);
				float color = clamp(knobColor + colorCV / 10.f,0.f,1.f);

				float frameLength = 10.f / tone;
				if(speed == LFO) frameLength *= 1000;

				int advanceSim = 0;
				e->frameDrop -= 44100.f / args.sampleRate;
				while(e->frameDrop < 0){
					e->frameDrop += frameLength;
					advanceSim++;
				}

				float engineShift = inputs[ENGINE_CV_1_INPUT + ri].getPolyVoltage(ei);
				float flavorShift = inputs[FLAVOR_CV_1_INPUT + ri].getPolyVoltage(ei);

				while(advanceSim > 0){
					advanceSim--;

					e->modeCycle += frameLength;
					while(e->modeCycle > 200) e->modeCycle -= 200;
					float mc = e->modeCycle;
					if(mc > 100) mc = 200 - 100;
					mc = mc / 100.f * 5.f;

					e->engineFlip = mc < engineShift;
					e->flavorFlip = mc < flavorShift;

					mode = ((params[ENGINE_SWITCH_1_PARAM + ri].getValue() > 0) != e->engineFlip) ? Atomic : BlackHole;
					flavor = ((params[FLAVOR_SWITCH_1_PARAM + ri].getValue() > 0) != e->flavorFlip) ? Notes : Tones;

					if(flavor == Tones){
						//int paternRate = args.sampleRate / 2000 * (1 + (1-color) * 99);
						int paternRate = args.sampleRate / (10 + 4000 * color);
						//paternRate /= frameLength;
						paternRate /= 10;
						e->stepCnt++;
						if(e->stepCnt > paternRate){
							e->stepCnt -= paternRate;
							e->stepIndex ++;
						}
					}

					// if(flavor == Tones && e->stepIndex < rows[ri].sequence.size()){
					// 	int paternRate = args.sampleRate / 200 * (10 + color * 90);
					// 	paternRate /= frameLength;
					// 	e->stepCnt++;
					// 	if(e->stepCnt > paternRate){
					// 		e->stepCnt -= paternRate;
					// 		e->stepIndex ++;
					// 	}
					// }


					if(e->stepIndex >= rows[ri].sequence.size()) e->stepIndex = 0;
					int gvIndex = rows[ri].sequence[e->stepIndex];
					float gvNew [2];
					{
						float angleCV = 0;
						for(int ri2 = ri; ri2 >= 0; ){
							if(inputs[SPIN_1_INPUT + ri2].isConnected()){
								angleCV = inputs[SPIN_1_INPUT + ri2].getPolyVoltage(ei);
								break;
							}
							if(!internalRoutingEnabled) break;
							if(ri2 == 0) break;
							ri2--;
							if(rowSpeed[ri2] == LFO){
								angleCV = outputs[RIGHT_1_OUTPUT + ri2].getPolyVoltage(ei);
								break;
							}
						}
						float angle = ((angleCV / 5.0f) + knobColor) * TWO_PI;
						float x = gravityValue[gvIndex][0];
						float y = gravityValue[gvIndex][1];
						float _cos = cos(angle);
						float _sin = sin(angle);
						gvNew[0] = x * _cos - y * _sin;
						gvNew[1] = x * _sin + y * _cos;
					}
					float gvScalar = 0.2f + color * (1.0f - 0.2f);
					if(mode == BlackHole) gvScalar *= 0.01f;
					for (int d = 0; d < 2; d++){
						e->gv[d] = e->gv[d] * (1-gvScalar) + gvNew[d] * gvScalar;
						if(!std::isfinite(e->internalState[d])) e->internalState[d] = 0;
						if(!std::isfinite(e->outputValue[d])) e->outputValue[d] = 0;
						if(!std::isfinite(e->outputHistory[d])) e->outputHistory[d] = 0;
					}

					float g_param_a = clamp(e->gv[0] / 10.f + 0.5f,0.f,1.f);
					float g_param_b = clamp(e->gv[1] / 10.f + 0.5f,0.f,1.f);

					if(mode == Atomic){
						float unkown_a = 0.4f + 0.2f * g_param_a;
						float gravity = 0.09f + (unkown_a * g_param_b) * (3.52f - 0.09f);
						float velocity = 0.01f + (unkown_a * (1.f-g_param_b)) * (1.52f - 0.01f);
						float decay = 0.999f - g_param_b * 0.001f;
						for (int d = 0; d < 2; d++){
							float dist = e->outputValue[d] - e->gv[d];
							if(abs(dist) > 0.01f){
								float internalState = clamp(e->internalState[d],-1000.f,1000.f);
								internalState -= dist * gravity;
								internalState *= decay;
								e->internalState[d] = internalState;
							}
							e->outputValue[d] += e->internalState[d] * velocity;
						}
					}else{ //mode == BlackHole
						float unkown_a = 0.2f * g_param_a;
						float velocity = 1.f + g_param_b * (3.3f - 1.f);
						float turnSpeed = 0.1f + unkown_a * (0.7f - 0.1f);
						float angle = modAngle(e->internalState[0]);
						float flip = flavor == Notes ? 1 : -1;
						float x = e->outputValue[0];
						float y = e->outputValue[1];
						x += cos(angle) * velocity;
						y += sin(angle) * velocity;
						x = clamp(x,-100.f,100.f);
						y = clamp(y,-100.f,100.f);
						float dx = e->gv[0] - x;
						float dy = e->gv[1] - y;
						float dSqrd = dx * dx + dy * dy;
						float angleToG = modAngleDelta(atan2(dy,dx) - angle);
						float target = 0;
						if(angleToG > 0) target = PI/2.f * flip;
						else target = -PI/2.f * flip;
						if(dSqrd > 25.f) target *= clamp(2 - dSqrd/25.f,0.f,1.f);
						if(dSqrd < 25.f) target *= clamp(1 + log(dSqrd/25.f),1.f,1.5f);
						float delta = modAngleDelta(angleToG - target);

						if(delta > turnSpeed) angle += turnSpeed;
						else if(delta < -turnSpeed) angle -= turnSpeed;
						else angle += delta;

						e->outputValue[0] = x;
						e->outputValue[1] = y;
						e->internalState[0] = angle;
					}
				}

				for (int d = 0; d < 2; d++){
					float output = e->outputValue[d];

					//Attempt to remove DC offset
					output -= e->gv[d];

					float level = params[LEVEL_1_PARAM + ri].getValue();

					if(speed == Audible){
						output /= 2.f;
						output = clamp(output,-5.f,5.f);
						output *= level * level; //Make level exponential on audio signals to better map to decibells
						e->outputHistory[d] = 0.99f * e->outputHistory[d] + 0.01f * output;
						output = e->outputHistory[d];
					}else{
						output = clamp(output,-10.f,10.f) / 2.f + 5.0f;
						output *= level;
						if(flavor == Notes){
							float speed = 0.000033f * tone;
							e->outputHistory[d] = (1.f-speed) * e->outputHistory[d] + speed * output;
							output = e->outputHistory[d];
						}
					}

					if(d == 0) outputs[LEFT_1_OUTPUT + ri].setVoltage(output, ei);
					else outputs[RIGHT_1_OUTPUT + ri].setVoltage(output, ei);

					if(speed == Audible){
						masterOut[ei][d] += output;
					}
				}

				outputs[LEFT_1_OUTPUT + ri].setChannels(channels);
				outputs[RIGHT_1_OUTPUT + ri].setChannels(channels);

			}
			
		}

		outputs[LEFT_MASTER_OUTPUT].setChannels(maxChannelsUsed);
		outputs[RIGHT_MASTER_OUTPUT].setChannels(maxChannelsUsed);

		for(unsigned int ei = 0; ei < maxChannelsUsed; ei++){
			outputs[LEFT_MASTER_OUTPUT].setVoltage(masterOut[ei][0],ei);
			outputs[RIGHT_MASTER_OUTPUT].setVoltage(masterOut[ei][1],ei);
		}
	}

	inline float modAngleDelta(float angle){
		while(angle > PI) angle -= TWO_PI;
		while(angle < -PI) angle += TWO_PI;
		return angle;
	}

	inline float modAngle(float angle){
		while(angle > TWO_PI) angle -= TWO_PI;
		while(angle < 0) angle += TWO_PI;
		return angle;
	}

	void pickNewSequence(int ri){
		int length = 2 + std::ceil(std::pow(rack::random::uniform(),5) * 20);

		rows[ri].sequence.clear();

		for(int i = 0; i < length; i++){
			int gi = floor(rack::random::uniform() * GRAVITY_VALUE_SIZE);
			rows[ri].sequence.push_back(gi);
		}
	}
};


struct AstroVibeWidget : ModuleWidget {
	AstroVibeWidget(AstroVibe* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/AstroVibe.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<CKSS>(mm2px(Vec(108.516, 41.332)), module, AstroVibe::SPEED_SWITCH_1_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(53.94, 42.14)), module, AstroVibe::ENGINE_SWITCH_1_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(73.691, 42.054)), module, AstroVibe::FLAVOR_SWITCH_1_PARAM));
		addParam(createParamCentered<PB61303>(mm2px(Vec(9.131, 51.724)), module, AstroVibe::TRAVEL_BUTTON_1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(37.676, 51.779)), module, AstroVibe::FREQ_KNOB_1_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(92.738, 51.779)), module, AstroVibe::TIMBRE_KNOB_1_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(126.885, 54.834)), module, AstroVibe::LEVEL_1_PARAM));

		addParam(createParamCentered<CKSS>(mm2px(Vec(108.516, 73.082)), module, AstroVibe::SPEED_SWITCH_2_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(53.94, 73.89)), module, AstroVibe::ENGINE_SWITCH_2_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(73.691, 73.804)), module, AstroVibe::FLAVOR_SWITCH_2_PARAM));
		addParam(createParamCentered<PB61303>(mm2px(Vec(9.131, 83.474)), module, AstroVibe::TRAVEL_BUTTON_2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(37.676, 83.529)), module, AstroVibe::FREQ_KNOB_2_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(92.738, 83.529)), module, AstroVibe::TIMBRE_KNOB_2_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(126.885, 86.584)), module, AstroVibe::LEVEL_2_PARAM));

		addParam(createParamCentered<CKSS>(mm2px(Vec(108.516, 104.832)), module, AstroVibe::SPEED_SWITCH_3_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(53.94, 105.64)), module, AstroVibe::ENGINE_SWITCH_3_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(73.691, 105.554)), module, AstroVibe::FLAVOR_SWITCH_3_PARAM));
		addParam(createParamCentered<PB61303>(mm2px(Vec(9.131, 115.224)), module, AstroVibe::TRAVEL_BUTTON_3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(37.676, 115.279)), module, AstroVibe::FREQ_KNOB_3_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(92.738, 115.279)), module, AstroVibe::TIMBRE_KNOB_3_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(126.885, 118.168)), module, AstroVibe::LEVEL_3_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.226, 32.394)), module, AstroVibe::TRAVEL_TRIG_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.451, 32.394)), module, AstroVibe::CLOCK_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.676, 32.394)), module, AstroVibe::FREQ_CV_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.336, 32.624)), module, AstroVibe::ENGINE_CV_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(86.504, 32.624)), module, AstroVibe::TIMBRE_CV_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(98.394, 32.624)), module, AstroVibe::SPIN_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.105, 51.779)), module, AstroVibe::FLAVOR_CV_1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.226, 64.144)), module, AstroVibe::TRAVEL_TRIG_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.451, 64.144)), module, AstroVibe::CLOCK_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.676, 64.144)), module, AstroVibe::FREQ_CV_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.336, 64.374)), module, AstroVibe::ENGINE_CV_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(86.504, 64.374)), module, AstroVibe::TIMBRE_CV_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(98.394, 64.374)), module, AstroVibe::SPIN_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.105, 83.529)), module, AstroVibe::FLAVOR_CV_2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.226, 95.894)), module, AstroVibe::TRAVEL_TRIG_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.451, 95.894)), module, AstroVibe::CLOCK_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(37.676, 95.894)), module, AstroVibe::FREQ_CV_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.336, 96.124)), module, AstroVibe::ENGINE_CV_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(86.504, 96.124)), module, AstroVibe::TIMBRE_CV_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(98.394, 96.124)), module, AstroVibe::SPIN_3_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(64.105, 115.279)), module, AstroVibe::FLAVOR_CV_3_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(121.379, 16.262)), module, AstroVibe::LEFT_MASTER_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(131.962, 16.262)), module, AstroVibe::RIGHT_MASTER_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(121.379, 41.332)), module, AstroVibe::LEFT_1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(131.962, 41.332)), module, AstroVibe::RIGHT_1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(121.379, 73.082)), module, AstroVibe::LEFT_2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(131.962, 73.082)), module, AstroVibe::RIGHT_2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(121.379, 104.832)), module, AstroVibe::LEFT_3_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(131.962, 104.832)), module, AstroVibe::RIGHT_3_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		AstroVibe* module = dynamic_cast<AstroVibe*>(this->module);

		menu->addChild(new MenuEntry);
		menu->addChild(createMenuLabel("Internal Routing"));

		struct InternalRoutingMenuItem : MenuItem {
			AstroVibe* module;
			bool value;
			void onAction(const event::Action& e) override {
				module->internalRoutingEnabled = value;
			}
		};

		InternalRoutingMenuItem* mi = createMenuItem<InternalRoutingMenuItem>("On");
		mi->rightText = CHECKMARK(module->internalRoutingEnabled);
		mi->module = module;
		mi->value = true;
		menu->addChild(mi);

		mi = createMenuItem<InternalRoutingMenuItem>("Off");
		mi->rightText = CHECKMARK(!module->internalRoutingEnabled);
		mi->module = module;
		mi->value = false;
		menu->addChild(mi);
	}
};


Model* modelAstroVibe = createModel<AstroVibe, AstroVibeWidget>("AstroVibe");