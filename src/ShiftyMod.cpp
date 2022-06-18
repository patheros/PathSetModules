/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include "plugin.hpp"
#include "util.hpp"
#include "shifty.hpp"

#define HIT_QUEUE_BASE_SIZE 16
#define HIT_QUEUE_SCALAR (HIT_QUEUE_BASE_SIZE - 1)
#define HIT_QUEUE_FULL_SIZE (HIT_QUEUE_BASE_SIZE * 4)

#define MUTE_FILTER_SIZE 24
#define MUTE_FILTER_COUNT 693
static const unsigned int MUTE_FILTERS [MUTE_FILTER_COUNT] = {0x0,0x1,0x2,0x4,0x8,0x10,0x20,0x40,0x80,0x100,0x200,0x400,0x800,0x1000,0x2000,0x4000,0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,0x800001,0x800002,0x800004,0x800008,0x800010,0x800020,0x800040,0x800080,0x800100,0x800200,0x800400,0x800800,0x801000,0x802000,0x804000,0x808000,0x810000,0x820000,0x840000,0x880000,0x900000,0xa00000,0xc00000,0xc00001,0xc00002,0xc00004,0xc00008,0xc00010,0xc00020,0xc00040,0xc00080,0xc00100,0xc00200,0xc00400,0xc00800,0xc01000,0xc02000,0xc04000,0xc08000,0xc10000,0xc20000,0xc40000,0xc80000,0xd00000,0xe00000,0xd00000,0xb00000,0xa80000,0xa40000,0x940000,0x920000,0x910000,0x890000,0x888000,0x884000,0x844000,0x842000,0x841000,0x821000,0x820800,0x820400,0x810400,0x810200,0x810100,0x808100,0x808080,0x808081,0x808101,0x810101,0x810102,0x810202,0x810204,0x810208,0x810408,0x820408,0x820410,0x820810,0x820820,0x821020,0x841020,0x841040,0x842040,0x844040,0x884040,0x884080,0x888080,0x888100,0x890100,0x910100,0x920100,0x920200,0x940200,0xa40200,0xa80200,0xa80400,0xb00400,0xd00400,0xd00800,0xe00800,0xe00801,0xe01001,0xe01002,0xe01004,0xe02004,0xe02008,0xe02010,0xe04010,0xe04020,0xe04040,0xe08040,0xe08080,0xd08080,0xd04080,0xb04080,0xb04040,0xa84040,0xa82040,0xa42040,0xa42020,0xa22020,0xa21020,0x921020,0x911020,0x910820,0x910810,0x908810,0x908410,0x904410,0x884410,0x884408,0x884208,0x882208,0x882210,0x882211,0x884211,0x884411,0x884421,0x884422,0x884442,0x884444,0x888444,0x888844,0x888884,0x888888,0x890888,0x890908,0x910908,0x911108,0x911110,0x921110,0x921210,0x922210,0x922410,0x942410,0x942420,0xa42420,0xa42820,0xa82820,0xa84820,0xa85040,0xb05040,0xb06040,0xd06040,0xd0a040,0xd0a080,0xe0a080,0xe0c080,0xd0c080,0xd0a080,0xd0a081,0xc8a081,0xc89081,0xc48881,0xc48882,0xc44882,0xc44482,0xc44442,0xc24442,0xc24242,0xc24244,0xc14244,0xc14144,0xc14148,0xc0c148,0xc0c0c8,0xa0c0c8,0xa0a0c8,0xa0a148,0xa12148,0xa12128,0xa12124,0x912124,0x911124,0x911224,0x911244,0x911248,0x912248,0x912249,0x912449,0x922449,0x922489,0x922491,0x922492,0x924492,0x924892,0x924912,0x924922,0x924924,0xa24924,0xa28924,0xa28a24,0xa28a28,0xc28a28,0xc30a28,0xc30c28,0xc30c30,0xc31430,0xc31431,0xc31451,0xc51451,0xc51461,0xc52461,0xc52462,0xc524a2,0xc624a2,0xc644a2,0xc644a4,0xc648a4,0xc648c4,0xc648c8,0xc64948,0xc68948,0xca8948,0xca8950,0xca8990,0xca8a90,0xcb0a90,0xcb0aa0,0xca8aa0,0xca86a0,0xca85a0,0xca45a0,0xca4560,0xca4550,0xaa4550,0xaa2550,0xaa1550,0xaa1350,0xaa12d0,0xaa12b0,0xaa12a8,0xaa0aa8,0x9a0aa8,0x960aa8,0x950aa8,0x9509a8,0x950968,0x950958,0x950954,0x950955,0x8d0955,0x8b0955,0x8a8955,0x8a88d5,0x8a88b5,0x8a88ad,0x8a88ab,0x9288ab,0x9248ab,0x92492b,0x92492d,0x92492e,0xa2492e,0xa4492e,0xa4892e,0xa4912e,0xa4922e,0xa4924e,0xa49256,0xa49257,0xa4925b,0xa4925d,0xa4945d,0xa4946d,0xa494ad,0xa4a4ad,0xa4a4b5,0xa524b5,0xa524b9,0xa524ba,0xa524da,0xa528da,0xa5295a,0xc5295a,0xc9295a,0xc9296a,0xc929aa,0xc949aa,0xc951aa,0xc952aa,0xc952b2,0xc952b4,0xc95334,0xc95354,0xc96354,0xca6554,0xca6654,0xcaa654,0xcaa664,0xcaa668,0xcaa6a8,0xcac6a8,0xcacaa8,0xcacca8,0xcacca9,0xaacca9,0xaaaca9,0xaaaaa9,0xaaaaaa,0xcaaaaa,0xccaaaa,0xcccaaa,0xccccaa,0xccccca,0xcccccc,0xccccd4,0xccccd5,0xcccd55,0xcd4d55,0xcd5555,0xcd5595,0xcd55a5,0xcd55a9,0xcd55aa,0xcd56aa,0xcd56ca,0xcd56cc,0xcd56ac,0xad56aa,0xab56aa,0xaad6aa,0xaab6aa,0xaab5aa,0xaab56a,0xaab566,0xaab556,0xaab555,0xaab595,0xaab5a5,0xaab5a6,0xb2b5a6,0xb4b5a6,0xb4d5a6,0xb4d9a6,0xb4daa6,0xb4daaa,0xb4daab,0xb4db2b,0xb4db4b,0xb4db4d,0xb55b55,0xb59b55,0xb5ab55,0xb5b359,0xb5b35a,0xb5b55a,0xb5b65a,0xb5b69a,0xd5b69a,0xd9b69a,0xd9b6aa,0xd9b6b2,0xd9b6b4,0xd9b6d4,0xdab6d4,0xdb36d4,0xdb56d4,0xdb66d4,0xdb6ad4,0xdb6cd4,0xdb6d54,0xdb6d64,0xdb6d68,0xdb6da8,0xd76da8,0xd76da9,0xd76ba9,0xd76aa9,0xd76ae9,0xd56ae9,0xd5eae9,0xd5dae9,0xd5d6e9,0xd5d6d9,0xd5d6da,0xd3d6da,0xd3d5da,0xd3d55a,0xd3d57a,0xd3d57c,0xb3d57c,0xabd57c,0xabe57c,0xabe97c,0xabea7c,0xabeabc,0xabeaac,0xabeaae,0xadeaae,0xadeace,0xcdead6,0xcdeada,0xd5eada,0xd5eadb,0xd6eadb,0xd76adb,0xd76cdb,0xd76d5b,0xd76d5d,0xdb6d5d,0xdb6d9d,0xdb6dad,0xdb6db5,0xdb6db6,0xeb6db6,0xf36db6,0xf3adb6,0xf3cdb6,0xf3ceb6,0xf3cf36,0xf3cf3a,0xf3cf3c,0xf5cf3c,0xf6cf3c,0xf6d73c,0xf6db3c,0xf6dd3c,0xf6dd3d,0xf6de3d,0xf6de5d,0xf6de6d,0xf6de75,0xf6de79,0xf6de7a,0xfade7a,0xfcde7a,0xfcee7a,0xfcf67a,0xfcfa7a,0xfcfc7a,0xfcfcba,0xfcfcda,0xfcfcdc,0xfafcdc,0xfaf4dc,0xfaf6dc,0xdaf6dc,0xdef6dc,0xded6dc,0xdededc,0xdedebc,0xdedeb4,0xdedeb6,0xdddeb6,0xdddab6,0xdddbb6,0xeddbb6,0xeddbb7,0xeedbb7,0xeeebb7,0xeeedb7,0xeeeeb7,0xeeeed7,0xeeeee7,0xeeeeeb,0xeeeeed,0xeeeeee,0xeeeef6,0xf6eefa,0xf6eefc,0xfaeefc,0xf9eefc,0xf9f6fc,0xf9fafc,0xf9fb7c,0xf9fbbc,0xf9fbdc,0xf9fbec,0xf9fbf4,0xf9fbf8,0xe9fbf8,0xedfbf8,0xedebf8,0xedeff8,0xedaff8,0xedbff8,0xedbdf8,0xadbdf8,0xbdbdf8,0xbdbdfc,0xbcbdfc,0xbcfdfc,0xbcf5fc,0xbcf7fc,0xbcf7fd,0xb4f7fd,0xb6f7fd,0xb6d7fd,0xb6dffd,0xb6defd,0xb6deff,0xb6daff,0xb6dbff,0xb6ebff,0xb6edff,0xb76dff,0xb76eff,0xb7aeff,0xb7b6ff,0xb7b77f,0xd7b7bf,0xd7b7df,0xe7b7ef,0xe7b7f7,0xebb7f7,0xedb7f7,0xedd7f7,0xeddbf7,0xedddf7,0xedddfb,0xedddfd,0xedddfe,0xeeddfe,0xef5dfe,0xef6dfe,0xef6efe,0xef6f7e,0xef6fbe,0xef6fee,0xefafee,0xefb7ee,0xefbbee,0xf7bbee,0xfbbbee,0xfbbbef,0xfbdbef,0xfbebef,0xfbedef,0xfbeeef,0xfbef6f,0xfbefaf,0xfbefb7,0xfbefbb,0xfbefbd,0xfbefbe,0xfbefde,0xfbefee,0xfbeff6,0xfbeffa,0xfbeffc,0xfdeffc,0xfeeffc,0xff6ffc,0xffaffc,0xffb7fc,0xffbbfc,0xffbdfc,0xffbefc,0xffbf7c,0xffbfbc,0xffbfdc,0xffbff4,0xffbff8,0xffbff9,0xffdff9,0xffeff9,0xfff7f9,0xfff7fa,0xfffbfa,0xfffdfa,0xfffefa,0xffff7a,0xffffba,0xffffda,0xffffea,0xfffff2,0xfffff4,0xfffff8,0xfdfff8,0xfdfbf8,0xfdfff8,0xfdfffc,0xddfffc,0xdffffc,0xdffdfc,0xdffdfe,0xdefdfe,0xdefffe,0xdefefe,0xdefeff,0xdf7eff,0xdfbeff,0xdfbf7f,0xdfdf7f,0xdfdfbf,0xdfefbf,0xdfefdf,0xdff7ef,0xdff7f7,0xdffbf7,0xdffbfb,0xdffdfb,0xdffdfd,0xdffdfe,0xeffdff,0xf7fdff,0xfbfdff,0xfdfdff,0xfdfeff,0xfdff7f,0xfdffbf,0xfdffdf,0xfdffef,0xfdfff7,0xfdfffb,0xfdfffd,0xfdfffe,0xfefffe,0xff7ffe,0xffbffe,0xffdffe,0xffeffe,0xfff7fe,0xfffbfe,0xfffdfe,0xffff7e,0xffffbe,0xfffff6,0xfffffc,0xffeffc,0xffeffe,0x7feffe,0x7ffffe,0x7fffff,0xbfffff,0xdfffff,0xefffff,0xf7ffff,0xfbffff,0xfdffff,0xfeffff,0xff7fff,0xffbfff,0xffdfff,0xffefff,0xfff7ff,0xfffbff,0xfffdff,0xfffeff,0xffff7f,0xffffbf,0xffffdf,0xffffef,0xfffff7,0xfffffb,0xfffffd,0xfffffe,0xffffff};

struct ShiftyMod : Module {
	enum ParamId {
		RAMP_PARAM,
		SAMPLE_AND_HOLD_PARAM,
		CLOCK_DIVIDER_PARAM,
		ENUMS(DELAY_SCALE_PARAMS, NUM_ROWS),
		ENUMS(ECHO_PARAMS, NUM_ROWS),
		ENUMS(MUTE_PARAMS, NUM_ROWS),		
		CLOCK_RATE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		TRIGGER_INPUT,
		ENUMS(DELAY_INPUTS, NUM_ROWS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(OUT_OUTPUTS, NUM_ROWS),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(SAMPLE_AND_HOLD_LIGHTS, NUM_ROWS),
		ENUMS(BUFFER_LIGHTS, NUM_ROWS),
		ENUMS(ECHO_LIGHTS, NUM_ROWS * 3),
		ENUMS(MUTE_LIGHTS, NUM_ROWS * 3),
		LIGHTS_LEN
	};

	int clockDividerCount = 0;
	float internalClock = 0;

	bool clockHigh = false;
	bool triggerHigh = false;

	bool outputOn [NUM_ROWS] = {};
	
	float noiseValue [NUM_ROWS] = {};

	bool prevHitPreMute [NUM_ROWS] = {};
	float muteCount [NUM_ROWS] = {};

	bool heldDelayOn [NUM_ROWS] = {};
	float heldDelayValue [NUM_ROWS] = {};


	// float delay_noiseValue [NUM_ROWS] = {};
	// float delay_preSaH [NUM_ROWS] = {};
	// float delay_postSaH [NUM_ROWS] = {};

	bool hitQueue [HIT_QUEUE_FULL_SIZE] = {}; 

	ShiftyExpanderBridge bridge = {};

	ShiftyMod() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(RAMP_PARAM, 0.f, 2.f, 1.f, "Ramp Delay"," beats / step down");
		//Note acutal range here is 1 to 16, but offseting by .4 so that 8 points straight up
		configParam(CLOCK_DIVIDER_PARAM, 0.6f, 15.6f, 8.f, "Clock Divider"," beats / trigger");
		configParam(CLOCK_RATE_PARAM, 10.f, 5000.f, 500.f, "Clock Rate"," bpm");
		configParam(SAMPLE_AND_HOLD_PARAM, 0.f, 1.f, 0.0f, "Sample & Hold", "% chance of hold", 0.f, 100.f, 0.f);
		configInput(CLOCK_INPUT, "Clock");
		configInput(TRIGGER_INPUT, "Trigger");
		for(int row = 0; row < NUM_ROWS; row ++){
			std::string rs = std::to_string(row+1);
			configParam(DELAY_SCALE_PARAMS + row, 0.f, 1.f, 0.f, "Delay CV (pre Ramp)"," beats",0.f,(float)HIT_QUEUE_BASE_SIZE,0.f);
			configParam(ECHO_PARAMS + row, 0.f, 1.f, 0.f, "Echo " + rs," hits",0.f,3.f,1.f);	
			configParam(MUTE_PARAMS + row, 0.f, 1.f, 0.f, "Mute " + rs," / " + std::to_string(MUTE_FILTER_SIZE) + " hits",0.f,(float)MUTE_FILTER_SIZE,0.f);
			configInput(DELAY_INPUTS + row, "Delay " + rs);
			configOutput(OUT_OUTPUTS + row, "Gate " + rs);
			configBypass(TRIGGER_INPUT, OUT_OUTPUTS + row);
		}
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);

		clockDividerCount = 0;
		internalClock = 0;

		clockHigh = false;
		triggerHigh = false;

		for(int row = 0; row < NUM_ROWS; row ++){
			outputOn [row] = false;
	
			noiseValue [row] = 0.f;

			prevHitPreMute [row] = false;
			muteCount [row] = 0.f;

			heldDelayOn [row] = false;
			heldDelayValue [row] = 0.f;
		}

		memset(hitQueue, 0, sizeof hitQueue);

		bridge = ShiftyExpanderBridge();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "version", json_string("2.1.0"));

		json_object_set_new(rootJ, "clockDividerCount" , json_integer(clockDividerCount));
		json_object_set_new(rootJ, "internalClock" , json_real(internalClock));

		json_object_set_new(rootJ, "clockHigh" , json_bool(clockHigh));
		json_object_set_new(rootJ, "triggerHigh" , json_bool(triggerHigh));

		json_t *rowsJ = json_array();
		for(int row = 0; row < NUM_ROWS; row ++){
			json_t *rowJ = json_object();

			json_object_set_new(rowJ, "outputOn" , json_bool(outputOn[row]));

			json_object_set_new(rowJ, "noiseValue" , json_real(noiseValue[row]));

			json_object_set_new(rowJ, "prevHitPreMute" , json_bool(prevHitPreMute[row]));
			json_object_set_new(rowJ, "muteCount" , json_real(muteCount[row]));

			json_object_set_new(rowJ, "heldDelayOn" , json_bool(heldDelayOn[row]));
			json_object_set_new(rowJ, "heldDelayValue" , json_real(heldDelayValue[row]));

			json_array_insert_new(rowsJ, row, rowJ);
		}
		json_object_set_new(rootJ, "rows", rowsJ);

		json_t *hitQueueJ = json_array();
		for(int hi = 0; hi < HIT_QUEUE_FULL_SIZE; hi ++){
			json_array_insert_new(hitQueueJ, hi, json_bool(hitQueue[hi]));
		}
		json_object_set_new(rootJ, "hitQueue", hitQueueJ);

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		
		clockDividerCount = json_integer_value(json_object_get(rootJ, "clockDividerCount"));
		internalClock = json_real_value(json_object_get(rootJ, "internalClock"));

		clockHigh = json_is_true(json_object_get(rootJ, "clockHigh"));
		triggerHigh = json_is_true(json_object_get(rootJ, "triggerHigh"));

		json_t *rowsJ = json_object_get(rootJ, "rows");
		for(int row = 0; row < NUM_ROWS; row ++){
			json_t *rowJ = json_array_get(rowsJ,row);

			outputOn[row] = json_is_true(json_object_get(rowJ, "outputOn"));
			
			noiseValue[row] = json_real_value(json_object_get(rowJ, "noiseValue"));

			prevHitPreMute[row] = json_is_true(json_object_get(rowJ, "prevHitPreMute"));			
			muteCount[row] = json_real_value(json_object_get(rowJ, "muteCount"));

			heldDelayOn[row] = json_is_true(json_object_get(rowJ, "heldDelayOn"));			
			heldDelayValue[row] = json_real_value(json_object_get(rowJ, "heldDelayValue"));
		}

		json_t *hitQueueJ = json_object_get(rootJ, "hitQueue");
		for(int hi = 0; hi < HIT_QUEUE_FULL_SIZE; hi ++){
			hitQueue[hi] = json_is_true(json_array_get(hitQueueJ, hi));
		}
	}

	void process(const ProcessArgs& args) override {

		if (rightExpander.module && rightExpander.module->model == modelShiftyExpander) {
			bridge = static_cast<ShiftyExpanderBase*>(rightExpander.module)->bridge;
		}

		bool hitEvent = false;
		bool clockEvent = false;
		bool sampleAndHoldEvent = false;

		bool triggerConnected = inputs[TRIGGER_INPUT].isConnected();

		float clock;
		if(inputs[CLOCK_INPUT].isConnected()){
			//External Clock
			clock = inputs[CLOCK_INPUT].getVoltage();
		}else{
			//Internal Clock
			internalClock += args.sampleTime / 60.f * (params[CLOCK_RATE_PARAM].getValue() + bridge.clockRate);
			while(internalClock > 1) internalClock--;
			clock = internalClock > 0.5 ? 10 : 0;
		}

		//Clock Edge Detection
		
		if (!clockHigh && clock > 2.0f) {
			clockHigh = true;
			clockEvent = true;

			clockDividerCount++;
			float clockDividerValue = floor(params[CLOCK_DIVIDER_PARAM].getValue() + bridge.clockDivider);
			if(clockDividerCount >= clockDividerValue){
				sampleAndHoldEvent = true;
				clockDividerCount = 0;
				if(!triggerConnected) hitEvent = true;
			}
		}else if(clockHigh && clock < 0.1f){
			clockHigh = false;
		}

		//Trigger Edge
		if(triggerConnected){
			float trigger = inputs[TRIGGER_INPUT].getVoltage();
			if (!triggerHigh && trigger > 2.0f) {
				triggerHigh = true;
				hitEvent = true;
			}else if(triggerHigh && trigger < 0.1f){
				triggerHigh = false;
			}
		}

		//Shift Interal Hit Queue
		if(clockEvent){
			//Call this part of clockEvent before hitEvent in case the clock and hit come on the same frame
			for(int x = HIT_QUEUE_FULL_SIZE-1; x >= 1; x--){
				hitQueue[x] = hitQueue[x-1];
			}
			hitQueue[0] = false;
		}

		//Add Hit to Hit Queue
		if(hitEvent){
			hitQueue[0] = true;
		}

		//Compute Hits 
		if(clockEvent || hitEvent){
			float rampKnob = params[RAMP_PARAM].getValue() + bridge.ramp;
			for(int row = 0; row < NUM_ROWS; row ++){				
				//Compute Delay	
				float delay;
				{
					if(heldDelayOn[row]){
						delay = heldDelayValue[row];
					}else{
						float delayScaleKnob = params[DELAY_SCALE_PARAMS + row].getValue();
						if(inputs[DELAY_INPUTS + row].isConnected()){
							delay = inputs[DELAY_INPUTS + row].getVoltage() / 10.f;
						}else{
							delay = noiseValue[row];
						}
						delay *= delayScaleKnob;
						delay += rampKnob * (float) row / (float) HIT_QUEUE_SCALAR;
						heldDelayValue[row] = delay;
					}
				}

				//Bool to track hit value through the next steps
				bool hit;

				//Base Hit
				{					
					int idx = (int)floor(delay * HIT_QUEUE_SCALAR);
					idx = idx % HIT_QUEUE_FULL_SIZE;
					if(idx < 0) idx += HIT_QUEUE_FULL_SIZE;
					hit = hitQueue[idx];
				}

				//Apply Echo
				bool hitPreEcho = hit;
				{
					float echo = params[ECHO_PARAMS + row].getValue() + bridge.echo[row];
					if(!hit && echo > 0.f){
						int idx = (int)floor(delay * (1.f + (abs(1.f - echo * 2.f))) * HIT_QUEUE_SCALAR);
						idx = idx % HIT_QUEUE_FULL_SIZE;
						if(idx < 0) idx += HIT_QUEUE_FULL_SIZE;
						hit = hitQueue[idx];
					}
					if(!hit && echo > 0.33f){
						float echoInRange = (echo - 0.33f) / 0.67f;
						int idx = (int)floor((delay + (3 * echoInRange)) * HIT_QUEUE_SCALAR);
						idx = idx % HIT_QUEUE_FULL_SIZE;
						if(idx < 0) idx += HIT_QUEUE_FULL_SIZE;
						hit = hitQueue[idx];
					}
					if(!hit && echo > 0.67f){
						float echoInRange = (echo - 0.67f) / 0.33f;
						int idx = (int)floor((delay + (3 * (1-echoInRange))) * HIT_QUEUE_SCALAR);
						idx = idx % HIT_QUEUE_FULL_SIZE;
						if(idx < 0) idx += HIT_QUEUE_FULL_SIZE;
						hit = hitQueue[idx];
					}
				}

				//Apply Mutes
				bool hitPreMute = hit;
				{					
					float mute = params[MUTE_PARAMS + row].getValue() + bridge.mute[row];
					bool prevHit = prevHitPreMute[row];
					prevHitPreMute[row] = hit;
					if(hit){
						muteCount[row]+=1;

						//If we have two hits in a row skip ahead in the mute by the delay amount
						//In the extreme case of the clock divider = 1, this means the delay, ramp, s&h and noise all affect the rythem
						if(prevHit) muteCount[row]+=delay;

						if(muteCount[row] >= MUTE_FILTER_SIZE){
							muteCount[row] -= MUTE_FILTER_SIZE;
						}
						int filterIndex = (int) floor(mute * (MUTE_FILTER_COUNT-0.9f)); //subtracting 0.9 so we don't overflow the array and only the top of the knob is all off
						if(((MUTE_FILTERS[filterIndex] >> (MUTE_FILTER_SIZE - 1 - (int)floor(muteCount[row]))) & 0x1) != 0x0){
							hit = false;
						}
					}
				}

				//Store Final Mutes
				outputOn[row] = hit;

				//Update Lights
				{
					lights[BUFFER_LIGHTS + row].setBrightness(hitPreEcho ? 1.f : 0.f);

					lights[ECHO_LIGHTS + row * 3 + 0].setBrightness(hitPreMute ? 100/255.f : 0.f);
					lights[ECHO_LIGHTS + row * 3 + 1].setBrightness(hitPreMute ? 0/255.f : 0.f);
					lights[ECHO_LIGHTS + row * 3 + 2].setBrightness(hitPreMute ? 100/255.f : 0.f);

					lights[MUTE_LIGHTS + row * 3 + 0].setBrightness(hit ? 180/255.f : 0.f);
					lights[MUTE_LIGHTS + row * 3 + 1].setBrightness(hit ? 50/255.f : 0.f);
					lights[MUTE_LIGHTS + row * 3 + 2].setBrightness(hit ? 5/255.f : 0.f);
				}
			}
		}

		if(sampleAndHoldEvent){			
			float sampleAndHold = params[SAMPLE_AND_HOLD_PARAM].getValue() + bridge.sample_and_hold;
			for(int row = 0; row < NUM_ROWS; row ++){
				//Roll Sample and Hold
				//Comes after main logic because otherwise it will prevent the new delay value from being computed
				bool held = rack::random::uniform() < sampleAndHold;
				heldDelayOn[row] = held;
				lights[SAMPLE_AND_HOLD_LIGHTS + row].setBrightness(held ? 1.f : 0);

				//Internal noise is updated only on sampleAndHold event
				//When trigger rate is low this prevents the delay from jumping around and double hitting or skippinga hit 
				noiseValue[row] = rack::random::uniform();
			}
		}

		//Set Output
		for(int row = 0; row < NUM_ROWS; row ++){
			bool on = clockHigh && outputOn[row];
			outputs[OUT_OUTPUTS + row].setVoltage((clockHigh && on) ? 10.f : 0);
		}
	}

};

struct ShiftyModWidget : ModuleWidget {
	ShiftyModWidget(ShiftyMod* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ShiftyMod.svg")));

		//Screws
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		//Upper Left
		addParam(createParamCentered<Trimpot>(mm2px(Vec(17.457, 32.899)), module, ShiftyMod::RAMP_PARAM));		
		addParam(createParamCentered<Trimpot>(mm2px(Vec(27.921, 32.9)), module, ShiftyMod::SAMPLE_AND_HOLD_PARAM));

		//Upper Right
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(72.817, 12.721)), module, ShiftyMod::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(59.55, 29.131)), module, ShiftyMod::TRIGGER_INPUT));
		addParam(createParamCentered<RotarySwitch<Trimpot>>(mm2px(Vec(73.863, 29.400)), module, ShiftyMod::CLOCK_DIVIDER_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(57.459, 12.538)), module, ShiftyMod::CLOCK_RATE_PARAM));

		//Rows
		const float ROW_YS [] = {48.385,58.969,69.557,80.017,90.723,101.301,111.862};

		for(int row = 0; row < NUM_ROWS; row ++){
			float y = ROW_YS[row] + 0.784f;

			//Delay Input
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.397, y)), module, ShiftyMod::DELAY_INPUTS + row));

			//Scale Knob
			addParam(createParamCentered<Trimpot>(mm2px(Vec(17.456, y)), module, ShiftyMod::DELAY_SCALE_PARAMS + row));

			//S&H Light
			addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(26.742, y)), module, ShiftyMod::SAMPLE_AND_HOLD_LIGHTS + row));

			//Buffer Light
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(Vec(36.759, y)), module, ShiftyMod::BUFFER_LIGHTS + row));

			//Echo Knob
			addParam(createParamCentered<Trimpot>(mm2px(Vec(46.956, y)), module, ShiftyMod::ECHO_PARAMS + row));
			//Echo Light
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(53.248, y+0.66f)), module, ShiftyMod::ECHO_LIGHTS + row * 3));

			//Mute Knob
			addParam(createParamCentered<Trimpot>(mm2px(Vec(59.602, y)), module, ShiftyMod::MUTE_PARAMS + row));
			//Mute Light
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(65.957, y+0.66f)), module, ShiftyMod::MUTE_LIGHTS + row * 3));

			//Output
			addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(73.524, y)), module, ShiftyMod::OUT_OUTPUTS + row));
		}
	}

	void appendContextMenu(Menu* menu) override {
		//auto module = dynamic_cast<ShiftyMod*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("Shifty"));
		
		if(module->rightExpander.module && module->rightExpander.module->model == modelShiftyExpander){
			menu->addChild(createMenuLabel("Expander already attached."));
		}else{
			menu->addChild(createMenuItem("Add Expander (right 8HP)", "", 
				[=]{
					addExpander();
				}
			));
		}
	}

	void addExpander(){
		Vec myPos = this->box.pos;
		Vec expanderPos = myPos.plus(Vec(this->box.size.x,0));

		Model* model = pluginInstance->getModel("ShiftyExpander");
		Module* module = model->createModule();
		APP->engine->addModule(module);
		
		ModuleWidget *moduleWidget = model->createModuleWidget(module);
	 	APP->scene->rack->setModulePosForce(moduleWidget, expanderPos);
	 	APP->scene->rack->addModule(moduleWidget);

		history::ModuleAdd *h = new history::ModuleAdd;
		h->name = "create expander module";
		h->setModule(moduleWidget);
		APP->history->push(h);
	}

	
};

Model* modelShiftyMod = createModel<ShiftyMod, ShiftyModWidget>("ShiftyMod");