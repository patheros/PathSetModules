/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/


#include "plugin.hpp"
#include "dsp/ringbuffer.hpp"
#include "filters/pitchshifter.h"
#include "util.hpp"
#include <iostream>
#include <fstream>

#define BUFFER_COUNT 6

//Note this assumes 44.1khz to achive a max cube size of 10 seconds
#define ASSUMED_SAMPLE_RATE 44100

//In Seconds
#define CROSS_FADE_SECONDS 0.15f 

#define CROSS_FADE_AMT static_cast<int>(CROSS_FADE_SECONDS * ASSUMED_SAMPLE_RATE)

#define BUFFER_LENGTH_SECONDS_KNOB_MAX 10.f
#define BUFFER_LENGTH_SECONDS static_cast<int>(BUFFER_LENGTH_SECONDS_KNOB_MAX + CROSS_FADE_SECONDS * 2)
#define BUFFER_SIZE_MAX static_cast<int>((BUFFER_LENGTH_SECONDS * ASSUMED_SAMPLE_RATE) + 2)

#define BUFFER_TAIL_PADDING 1

static const int READ_PATTERN_NEG [][6] = {
	{1,1,1,1,1,1},
	{2,1,1,1,1,1},
	{2,1,1,2,1,1},
	{2,1,2,1,2,1},
	{2,2,1,2,2,1},
	{2,2,2,2,2,1},
	{2,2,2,2,2,2},
	{3,2,2,2,2,2},
	{3,2,2,3,2,2},
	{3,2,3,2,3,2},
	{3,3,2,3,3,2},
	{3,3,3,3,3,2},
	{3,3,3,3,3,3},
	{4,3,3,3,3,3},
	{4,3,3,4,3,3},
	{4,3,4,3,4,3},
	{4,4,3,4,4,3},
	{4,4,4,4,4,3},
	{4,4,4,4,4,4},
	{5,4,4,4,4,4},
	{5,4,4,5,4,4},
	{5,4,5,4,5,4},
	{5,5,4,5,5,4},
	{5,5,5,5,5,4},
	{5,5,5,5,5,5},
};

static const int READ_PATTERN_POS [][6] = {
	{1,2,3,4,5,6},
	{1,2,3,4,6,5},
	{1,2,3,5,4,6},
	{1,2,4,3,5,6},
	{1,3,2,4,5,6},
	{1,2,3,5,6,4},
	{1,2,3,6,4,5},
	{1,2,3,6,5,4},
	{1,2,4,3,6,5},
	{1,2,4,5,3,6},
	{1,2,5,3,4,6},
	{1,2,5,4,3,6},
	{1,3,2,4,6,5},
	{1,3,2,5,4,6},
	{1,3,4,2,5,6},
	{1,4,2,3,5,6},
	{1,4,3,2,5,6},
	{1,2,4,5,6,3},
	{1,2,4,6,3,5},
	{1,2,4,6,5,3},
	{1,2,5,3,6,4},
	{1,2,5,4,6,3},
	{1,2,6,3,4,5},
	{1,2,6,3,5,4},
	{1,2,6,4,3,5},
	{1,2,6,4,5,3},
	{1,3,2,5,6,4},
	{1,3,2,6,4,5},
	{1,3,2,6,5,4},
	{1,3,4,2,6,5},
	{1,3,4,5,2,6},
	{1,3,5,2,4,6},
	{1,3,5,4,2,6},
	{1,4,2,3,6,5},
	{1,4,2,5,3,6},
	{1,4,3,2,6,5},
	{1,4,3,5,2,6},
	{1,5,2,3,4,6},
	{1,5,2,4,3,6},
	{1,5,3,2,4,6},
	{1,5,3,4,2,6},
	{1,2,5,6,3,4},
	{1,2,5,6,4,3},
	{1,2,6,5,3,4},
	{1,2,6,5,4,3},
	{1,3,4,5,6,2},
	{1,3,4,6,2,5},
	{1,3,4,6,5,2},
	{1,3,5,2,6,4},
	{1,3,5,4,6,2},
	{1,3,6,2,4,5},
	{1,3,6,2,5,4},
	{1,3,6,4,2,5},
	{1,3,6,4,5,2},
	{1,4,2,5,6,3},
	{1,4,2,6,3,5},
	{1,4,2,6,5,3},
	{1,4,3,5,6,2},
	{1,4,3,6,2,5},
	{1,4,3,6,5,2},
	{1,4,5,2,3,6},
	{1,4,5,3,2,6},
	{1,5,2,3,6,4},
	{1,5,2,4,6,3},
	{1,5,3,2,6,4},
	{1,5,3,4,6,2},
	{1,5,4,2,3,6},
	{1,5,4,3,2,6},
	{1,6,2,3,4,5},
	{1,6,2,3,5,4},
	{1,6,2,4,3,5},
	{1,6,2,4,5,3},
	{1,6,3,2,4,5},
	{1,6,3,2,5,4},
	{1,6,3,4,2,5},
	{1,6,3,4,5,2},
	{1,3,5,6,2,4},
	{1,3,5,6,4,2},
	{1,3,6,5,2,4},
	{1,3,6,5,4,2},
	{1,4,5,2,6,3},
	{1,4,5,3,6,2},
	{1,4,6,2,3,5},
	{1,4,6,2,5,3},
	{1,4,6,3,2,5},
	{1,4,6,3,5,2},
	{1,5,2,6,3,4},
	{1,5,2,6,4,3},
	{1,5,3,6,2,4},
	{1,5,3,6,4,2},
	{1,5,4,2,6,3},
	{1,5,4,3,6,2},
	{1,6,2,5,3,4},
	{1,6,2,5,4,3},
	{1,6,3,5,2,4},
	{1,6,3,5,4,2},
	{1,6,4,2,3,5},
	{1,6,4,2,5,3},
	{1,6,4,3,2,5},
	{1,6,4,3,5,2},
	{1,4,5,6,2,3},
	{1,4,5,6,3,2},
	{1,4,6,5,2,3},
	{1,4,6,5,3,2},
	{1,5,4,6,2,3},
	{1,5,4,6,3,2},
	{1,5,6,2,3,4},
	{1,5,6,2,4,3},
	{1,5,6,3,2,4},
	{1,5,6,3,4,2},
	{1,5,6,4,2,3},
	{1,5,6,4,3,2},
	{1,6,4,5,2,3},
	{1,6,4,5,3,2},
	{1,6,5,2,3,4},
	{1,6,5,2,4,3},
	{1,6,5,3,2,4},
	{1,6,5,3,4,2},
	{1,6,5,4,2,3},
	{1,6,5,4,3,2},
};

#define PITCH_BUFF_SIZE 1024

#define MAX_CHANNELS 2

void writeDoubleBuffer(std::ostream * out, dsp::DoubleRingBuffer<float,PITCH_BUFF_SIZE> * buffer){
	out->write( (char *)& buffer->start, sizeof(float));
	out->write( (char *)& buffer->end, sizeof(float));
	out->write( (char *)& buffer->data, PITCH_BUFF_SIZE * 2 * sizeof(float));
}

void readDoubleBuffer(std::fstream * in, dsp::DoubleRingBuffer<float,PITCH_BUFF_SIZE> * buffer){
	in->read( (char *)& buffer->start, sizeof(float));
	in->read( (char *)& buffer->end, sizeof(float));
	in->read( (char *)& buffer->data, PITCH_BUFF_SIZE * 2 * sizeof(float));
}

struct IceTray : Module {
	enum ParamId {
		SPEED_NUM_PARAM,
		SPEED_NUM_CK_PARAM,
		SPEED_DENOM_PARAM,		
		SPEED_DENOM_CK_PARAM,
		FROST_PARAM,
		FROST_CK_PARAM,
		RECORD_LENGTH_PARAM,
		PLAYBACK_MODE_PARAM,		
		REPEATS_PARAM,
		PATERN_PARAM,		
		FEEDBACK_PARAM,		
		FEEDBACK_CK_PARAM,	
		ENUMS(CUBE_SWITCH_PARAM, BUFFER_COUNT),
		PARAMS_LEN,		
	};
	enum InputId {
		LEFT_INPUT,
		CLOCK_RECORD_INPUT,
		CLOCK_PLAYBACK_INPUT,
		SPEED_NUM_CV_INPUT,
		SPEED_DENOM_CV_INPUT,
		REPEATS_CV_INPUT,
		PATERN_CV_INPUT,		
		FROST_CV_INPUT,		
		FEEDBACK_CV_INPUT,
		RIGHT_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		LEFT_OUTPUT,
		RIGHT_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(CUBE_LIGHT, BUFFER_COUNT),
		ENUMS(RP_LIGHT, BUFFER_COUNT * 3),
		LIGHTS_LEN
	};

	enum LockLevel {
		NONE,
		RECORD,
		ALL,
	};

	float buffers [BUFFER_COUNT][BUFFER_SIZE_MAX][MAX_CHANNELS];
	LockLevel bufferLockLevel [BUFFER_COUNT] = {NONE,NONE,NONE,ALL,ALL,ALL};
	int loopSize [BUFFER_COUNT];

	float playbackCrossFadeBuffer [CROSS_FADE_AMT][MAX_CHANNELS];
	int playbackCrossFadeBufferIndex = 0;
	float recordCrossFadePreBuffer [CROSS_FADE_AMT][MAX_CHANNELS];
	float recordCrossFadePreBufferIndex = 0;

	float recordIndex = 0;
	int recordBuffer = 0;
	int playbackIndex = 0;
	int playbackBuffer = -1;

	bool playbackClockHigh = false;
	bool recordClockHigh = false;
	bool firstRecordClock = true;

	float feedbackValue [MAX_CHANNELS] = {0,0};
	int playbackRepeatCount = 0;

	int nextReadPatternIndex = -1;

	float prevInput [MAX_CHANNELS] = {0,0};
	int fadeInStart = 0;

	dsp::TRCFilter<float> lowpassFilter [MAX_CHANNELS];
	dsp::TRCFilter<float> highpassFilter [MAX_CHANNELS];

	dsp::DoubleRingBuffer<float, PITCH_BUFF_SIZE> in_Buffer [MAX_CHANNELS];
	dsp::DoubleRingBuffer<float, PITCH_BUFF_SIZE> ps_Buffer [MAX_CHANNELS];
	PitchShifter *pShifter [MAX_CHANNELS];

	bool cubeButtonDown [BUFFER_COUNT];
	bool cubeButtonDir [BUFFER_COUNT];

	bool pitchCorrectionOn = true;

	IceTray() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		
		configParam(SPEED_NUM_PARAM, 1.f, 11.f, 6.f, "Record Speed Numerator");
		configParam(SPEED_NUM_CK_PARAM, -1.f, 1.f, 0.f, "Numerator CV Scalar", "%", 0.f, 100.f, 0.f);
		configParam(SPEED_DENOM_PARAM, 1.f, 11.f, 6.f, "Record Speed Denominator");		
		configParam(SPEED_DENOM_CK_PARAM, -1.f, 1.f, 0.f, "Denominator CV Sclar", "%", 0.f, 100.f, 0.f);
		configParam(FROST_PARAM, 0.f, 1.f, 0.5f, "Frozen Percent", "%", 0.f, 100.f, 0.f);
		configParam(FROST_CK_PARAM, -1.f, 1.f, 0.f, "Frozen Percent CV Scalar", "%", 0.f, 100.f, 0.f);
		configParam(RECORD_LENGTH_PARAM, 0.1f, BUFFER_LENGTH_SECONDS_KNOB_MAX, 1.f, "Record Length", " Seconds");
		configSwitch(PLAYBACK_MODE_PARAM, 0.f, 1.f, 1.f, "Playback Clock Rests Position", std::vector<std::string>{"Off","On"});
		configParam(REPEATS_PARAM, 1.f, 10.f, 1.f, "Playback Repeat Count");
		configParam(PATERN_PARAM, -1.f, 1.f, 0.f, "Playback Pattern");
		configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.0f, "Feedback Percent", "%", 0.f, 100.f, 0.f);
		configParam(FEEDBACK_CK_PARAM, -1.f, 1.f, 0.f, "Frozen Percent CV Scalar", "%", 0.f, 100.f, 0.f);
		for(int i = 0; i < BUFFER_COUNT; i++){
			std::string cs = std::to_string(i+1);
			configButton(CUBE_SWITCH_PARAM + i, "Cube " + cs);
		}

		configInput(LEFT_INPUT, "Left Audio Input");
		configInput(RIGHT_INPUT, "Right Audio Input");
		configInput(CLOCK_RECORD_INPUT, "Record Clock");
		configInput(CLOCK_PLAYBACK_INPUT, "Playback Clock");
		configInput(SPEED_NUM_CV_INPUT, "Numerator CV");
		configInput(SPEED_DENOM_CV_INPUT, "Denominator CV");
		configInput(REPEATS_CV_INPUT, "Playback Repeat Count CV");
		configInput(PATERN_CV_INPUT, "Playback Patern CV");		
		configInput(FROST_CV_INPUT, "Frozen Track Percent CV");	
		configInput(FEEDBACK_CV_INPUT, "Feedback CV");
		configOutput(LEFT_OUTPUT, "Left Audio Output");
		configOutput(RIGHT_OUTPUT, "Right Audio Output");

		configBypass(LEFT_INPUT, LEFT_OUTPUT);
		configBypass(RIGHT_INPUT, RIGHT_OUTPUT);

		pShifter[0] = new PitchShifter();
		pShifter[1] = new PitchShifter();

		// Initialize filter cutoffs with default sample rate (44100)
		float defaultSampleRate = 44100.0f;
		lowpassFilter[0].setCutoff(20000 / defaultSampleRate);
		lowpassFilter[1].setCutoff(20000 / defaultSampleRate);
		highpassFilter[0].setCutoff(20 / defaultSampleRate);
		highpassFilter[1].setCutoff(20 / defaultSampleRate);

		clearCubes();
		in_Buffer[0].clear();
		in_Buffer[1].clear();
		ps_Buffer[0].clear();
		ps_Buffer[1].clear();
	}

	~IceTray() override {
		delete pShifter[0];
		delete pShifter[1];
	}

	void clearCubes(){
		memset(buffers, 0, sizeof buffers);
		bufferLockLevel[0] = NONE;
		bufferLockLevel[1] = NONE;
		bufferLockLevel[2] = NONE;
		bufferLockLevel[3] = ALL;
		bufferLockLevel[4] = ALL;
		bufferLockLevel[5] = ALL;
		memset(loopSize, 0, sizeof loopSize);
		memset(playbackCrossFadeBuffer, 0, sizeof playbackCrossFadeBuffer);
		memset(recordCrossFadePreBuffer, 0, sizeof recordCrossFadePreBuffer);

		recordIndex = 0;
		recordBuffer = 0;
		playbackIndex = 0;
		playbackBuffer = -1;
		playbackCrossFadeBufferIndex = 0;
		recordCrossFadePreBufferIndex = 0;

		playbackClockHigh = false;
		recordClockHigh = false;
		firstRecordClock = true;

		feedbackValue[0] = 0;
		feedbackValue[1] = 0;
		playbackRepeatCount = 0;

		nextReadPatternIndex = 0;

		prevInput[0] = 0;
		prevInput[1] = 0;
		fadeInStart = 0;

		updateCubeLights();
		updateRecordAndPlaybackLights();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);

		clearCubes();
		in_Buffer[0].clear();
		in_Buffer[1].clear();
		ps_Buffer[0].clear();
		ps_Buffer[1].clear();

		pitchCorrectionOn = true;

		lowpassFilter[0].reset();
		lowpassFilter[1].reset();
		highpassFilter[0].reset();
		highpassFilter[1].reset();
	}

	void onAdd(const AddEvent& e) override {
		std::string path = system::join(createPatchStorageDirectory(), "buffers.dat");
		DEBUG("Reading data file '%s' ",path.c_str());
		// Read file...
		std::fstream dataFile(path, ios::binary | ios::in);
		if (dataFile.is_open())
		{
			DEBUG("Data file is open");
	    	dataFile.read( (char *)& buffers[0][0][0], BUFFER_COUNT * BUFFER_SIZE_MAX * MAX_CHANNELS * sizeof(float) );
			dataFile.read( (char *)& playbackCrossFadeBuffer[0][0], CROSS_FADE_AMT * MAX_CHANNELS * sizeof(float) );
			dataFile.read( (char *)& recordCrossFadePreBuffer[0][0], CROSS_FADE_AMT * MAX_CHANNELS * sizeof(float) );
			// readDoubleBuffer(& dataFile, & in_Buffer[0]);
			// readDoubleBuffer(& dataFile, & in_Buffer[1]);
			// readDoubleBuffer(& dataFile, & ps_Buffer[0]);
			// readDoubleBuffer(& dataFile, & ps_Buffer[1]);
	    	dataFile.close();
	  	}
	  	else
	  	{
	  		DEBUG("Unable to open data file");
	  	}

		updateCubeLights();
		updateRecordAndPlaybackLights();
	}

	void onSave(const SaveEvent& e) override {
		std::string path = system::join(createPatchStorageDirectory(), "buffers.dat");
		DEBUG("Saving data file '%s' ",path.c_str());
		// Write file...
		std::fstream dataFile(path, ios::binary | ios::out);
		dataFile.write( (char *)& buffers[0][0][0], BUFFER_COUNT * BUFFER_SIZE_MAX * MAX_CHANNELS * sizeof(float) );
		dataFile.write( (char *)& playbackCrossFadeBuffer[0][0], CROSS_FADE_AMT * MAX_CHANNELS * sizeof(float) );
		dataFile.write( (char *)& recordCrossFadePreBuffer[0][0], CROSS_FADE_AMT * MAX_CHANNELS * sizeof(float) );
		// writeDoubleBuffer(& dataFile, & in_Buffer[0]);
		// writeDoubleBuffer(& dataFile, & in_Buffer[1]);
		// writeDoubleBuffer(& dataFile, & ps_Buffer[0]);
		// writeDoubleBuffer(& dataFile, & ps_Buffer[1]);
		dataFile.close();
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "version", json_string("2.1.0"));

		for(int bi = 0; bi < BUFFER_COUNT; bi++){
			std::string bis = std::to_string(bi);
			json_object_set_new(rootJ, std::string("bufferLockLevel." + bis).c_str(), json_integer(bufferLockLevel[bi]));
			json_object_set_new(rootJ, std::string("loopSize." + bis).c_str(), json_integer(loopSize[bi]));
		}

		json_object_set_new(rootJ, "playbackCrossFadeBufferIndex" , json_integer(playbackCrossFadeBufferIndex));
		json_object_set_new(rootJ, "recordCrossFadePreBufferIndex" , json_integer(recordCrossFadePreBufferIndex));

		json_object_set_new(rootJ, "recordIndex" , json_real(recordIndex));
		json_object_set_new(rootJ, "recordBuffer" , json_integer(recordBuffer));
		json_object_set_new(rootJ, "playbackIndex" , json_integer(playbackIndex));
		json_object_set_new(rootJ, "playbackBuffer" , json_integer(playbackBuffer));

		json_object_set_new(rootJ, "playbackClockHigh" , json_bool(playbackClockHigh));
		json_object_set_new(rootJ, "recordClockHigh" , json_bool(recordClockHigh));

		json_object_set_new(rootJ, "feedbackValue.0" , json_real(feedbackValue[0]));
		json_object_set_new(rootJ, "feedbackValue.1" , json_real(feedbackValue[1]));
		json_object_set_new(rootJ, "playbackRepeatCount" , json_integer(playbackRepeatCount));

		json_object_set_new(rootJ, "nextReadPatternIndex" , json_integer(nextReadPatternIndex));

		json_object_set_new(rootJ, "prevInput.0" , json_real(prevInput[0]));
		json_object_set_new(rootJ, "prevInput.1" , json_real(prevInput[1]));
		json_object_set_new(rootJ, "fadeInStart" , json_integer(fadeInStart));

		json_object_set_new(rootJ, "pitchCorrectionOn" , json_bool(pitchCorrectionOn));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {

		for(int bi = 0; bi < BUFFER_COUNT; bi++){
			std::string bis = std::to_string(bi);
			bufferLockLevel[bi] = (LockLevel)json_integer_value(json_object_get(rootJ, std::string("bufferLockLevel." + bis).c_str()));
			loopSize[bi] = json_integer_value(json_object_get(rootJ, std::string("loopSize." + bis).c_str()));
		}

		playbackCrossFadeBufferIndex = json_integer_value(json_object_get(rootJ, "playbackCrossFadeBufferIndex"));
		recordCrossFadePreBufferIndex = json_integer_value(json_object_get(rootJ, "recordCrossFadePreBufferIndex"));

		recordIndex = json_real_value(json_object_get(rootJ, "recordIndex"));
		recordBuffer = json_integer_value(json_object_get(rootJ, "recordBuffer"));
		playbackIndex = json_integer_value(json_object_get(rootJ, "playbackIndex"));
		playbackBuffer = json_integer_value(json_object_get(rootJ, "playbackBuffer"));

		playbackClockHigh = json_is_true(json_object_get(rootJ, "playbackClockHigh"));
		recordClockHigh = json_is_true(json_object_get(rootJ, "recordClockHigh"));

		feedbackValue[0] = json_real_value(json_object_get(rootJ, "feedbackValue.0"));
		feedbackValue[1] = json_real_value(json_object_get(rootJ, "feedbackValue.1"));
		playbackRepeatCount = json_integer_value(json_object_get(rootJ, "playbackRepeatCount"));

		nextReadPatternIndex = json_integer_value(json_object_get(rootJ, "nextReadPatternIndex"));

		prevInput[0] = json_real_value(json_object_get(rootJ, "prevInput.0"));
		prevInput[1] = json_real_value(json_object_get(rootJ, "prevInput.1"));
		fadeInStart = json_integer_value(json_object_get(rootJ, "fadeInStart"));

		pitchCorrectionOn = json_is_true(json_object_get(rootJ, "pitchCorrectionOn"));
	}

	void process(const ProcessArgs& args) override {
		for(int bi = 0; bi < BUFFER_COUNT; bi++){
			bool button = params[CUBE_SWITCH_PARAM + bi].getValue() > 0;
			if(!cubeButtonDown[bi] && button){
				cubeButtonDown[bi] = true;
				switch(bufferLockLevel[bi]){
					case RECORD:
						bufferLockLevel[bi] = cubeButtonDir[bi] ? ALL : NONE;
						break;
					case ALL:
						bufferLockLevel[bi] = RECORD;
						cubeButtonDir[bi] = false;
						break;
					case NONE:
						bufferLockLevel[bi] = RECORD;
						cubeButtonDir[bi] = true;
						break;
				}
				if(recordBuffer == -1 && bufferLockLevel[bi] == NONE) record_jumpToNextTrack();
				else if(bi == recordBuffer && bufferLockLevel[bi] != ALL) record_jumpToNextTrack(); 

				if(playbackBuffer == -1 && bufferLockLevel[bi] != NONE) playback_jumpToNextTrack(false, false);
				else if(bi == playbackBuffer && bufferLockLevel[bi] == NONE) playback_jumpToNextTrack(false, false);
				
				updateCubeLights();
			}else if(cubeButtonDown[bi] && !button){
				cubeButtonDown[bi] = false;
			}
		}

		float bufferLengthSeconds = inputs[CLOCK_RECORD_INPUT].isConnected() ? BUFFER_LENGTH_SECONDS_KNOB_MAX : params[RECORD_LENGTH_PARAM].getValue();
		int bufferLength = round(ASSUMED_SAMPLE_RATE * bufferLengthSeconds);

		float recordClock = inputs[CLOCK_RECORD_INPUT].getVoltage();
		if (!recordClockHigh && recordClock > 2.0f) {
			recordClockHigh = true;
			if(firstRecordClock){
				firstRecordClock = false;
			}else{
				record_jumpToNextTrack();
			}
		}else if(recordClockHigh && recordClock < 0.1f){
			recordClockHigh = false;
		}

		float playbackClock = inputs[CLOCK_PLAYBACK_INPUT].getVoltage();
		if (!playbackClockHigh && playbackClock > 2.0f) {
			playbackClockHigh = true;
			playback_jumpToNextTrack(false, false);
		}else if(playbackClockHigh && playbackClock < 0.1f){
			playbackClockHigh = false;
		}

		float speedNumerator = round(params[SPEED_NUM_PARAM].getValue() + params[SPEED_NUM_CK_PARAM].getValue() * inputs[SPEED_NUM_CV_INPUT].getVoltage());
		float speedDenomonator = round(params[SPEED_DENOM_PARAM].getValue() + params[SPEED_DENOM_CK_PARAM].getValue() * inputs[SPEED_NUM_CV_INPUT].getVoltage());
		if(speedNumerator < 1) speedNumerator = 1;
		if(speedDenomonator < 1) speedDenomonator = 1;
		float speed = clamp(speedNumerator / speedDenomonator,0.001f,1000.f);
		float speedInvert = 1/speed;

		float feedbackScalar = params[FEEDBACK_PARAM].getValue() + params[FEEDBACK_CK_PARAM].getValue() * inputs[FEEDBACK_CV_INPUT].getVoltage() / 10.f;
		feedbackValue[0] *= feedbackScalar;
		feedbackValue[1] *= feedbackScalar;

		float rawInput [MAX_CHANNELS] = {inputs[LEFT_INPUT].getVoltageSum(), inputs[RIGHT_INPUT].getVoltageSum()};
		bool inputConnected [MAX_CHANNELS] = {inputs[LEFT_INPUT].isConnected(), inputs[RIGHT_INPUT].isConnected()};

		float pitchShiftedInput [MAX_CHANNELS] = {0,0};

		for(int ci = 0; ci < MAX_CHANNELS; ci++){
			if(!inputConnected[ci]) continue;

			if(pitchCorrectionOn){
				in_Buffer[ci].push(rawInput[ci] / 10.0f);

				if (in_Buffer[ci].full()) {
					pShifter[ci]->process(speedInvert, in_Buffer[ci].startData(), ps_Buffer[ci].endData());
					ps_Buffer[ci].endIncr(PITCH_BUFF_SIZE);
					in_Buffer[ci].clear();
				}

				if (ps_Buffer[ci].size() > 0) {
					pitchShiftedInput[ci] = *ps_Buffer[ci].startData() * 6.6f;
					ps_Buffer[ci].startIncr(1);
				}
			}else{
				pitchShiftedInput[ci] = rawInput[ci];
			}
		}

		float input [MAX_CHANNELS] = {pitchShiftedInput[0] + feedbackValue[0], pitchShiftedInput[1] + feedbackValue[1]};

		int steps = floor(speedInvert);
		//Pre Record Buffer Record
		{
			int low = floor(recordCrossFadePreBufferIndex);
			recordCrossFadePreBufferIndex += speedInvert;
			if(recordCrossFadePreBufferIndex >= CROSS_FADE_AMT) recordCrossFadePreBufferIndex -= CROSS_FADE_AMT;

			for(int d = 0; d <= steps; d++){

				int ri = low + d;
				float percent = d / speedInvert;
				float writeVal0 = prevInput[0] * (1-percent) + input[0] * percent;
				float writeVal1 = prevInput[1] * (1-percent) + input[1] * percent;

				recordCrossFadePreBuffer[ri][0] = writeVal0;
				recordCrossFadePreBuffer[ri][1] = writeVal1;
				ri++;
				if(ri >= CROSS_FADE_AMT){
					low -= CROSS_FADE_AMT;
				}
			}			
		}

		//Main Buffer Record
		if(recordBuffer >= 0){
			int low = floor(recordIndex);			
			recordIndex += speedInvert; //Do this before the loop so if record_jumpToNextTrack gets called, it overrides this value
			for(int d = 0; d <= steps; d++){

				int ri = low + d;
				float percent = d / speedInvert;
				float writeVal0 = prevInput[0] * (1-percent) + input[0] * percent;
				float writeVal1 = prevInput[1] * (1-percent) + input[1] * percent;

				buffers[recordBuffer][ri][0] = writeVal0;
				buffers[recordBuffer][ri][1] = writeVal1;
				ri++;
				if(ri >= bufferLength){
					record_jumpToNextTrack();
					break;
					//low = floor(recordIndex) - d;
				}
			}			
		}

		prevInput[0] = input[0];
		prevInput[1] = input[1];

		float output [MAX_CHANNELS] = {0,0};
		if(playbackBuffer >= 0){
			getPlaybackOuput(output[0],output[1],playbackIndex);
			playbackIndex++;

			if(params[PLAYBACK_MODE_PARAM].getValue() == 1){
				if(playbackIndex >= loopSize[playbackBuffer]){
					playback_jumpToNextTrack(false, true);
				}
			}
			if(playbackIndex >= bufferLength){
				playback_jumpToNextTrack(true, true);
			}
		}

		if(playbackCrossFadeBufferIndex < CROSS_FADE_AMT){
			float crossOut0 = playbackCrossFadeBuffer[playbackCrossFadeBufferIndex][0];
			float crossOut1 = playbackCrossFadeBuffer[playbackCrossFadeBufferIndex][1];
			output[0] += crossOut0;
			output[1] += crossOut1;
			playbackCrossFadeBufferIndex++;
		}

		for(int ci = 0; ci < MAX_CHANNELS; ci++){
			if(std::isnan(output[ci])) output[ci] = 0;
			lowpassFilter[ci].process(output[ci]);
			output[ci] = lowpassFilter[ci].lowpass();
			highpassFilter[ci].process(output[ci]);
			output[ci] = highpassFilter[ci].highpass();
		}
		outputs[LEFT_OUTPUT].setVoltage(output[0]);
		outputs[RIGHT_OUTPUT].setVoltage(output[1]);
		feedbackValue[0] = output[0];
		feedbackValue[1] = output[1];
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		pShifter[0]->cleanup();
		pShifter[1]->cleanup();

		pShifter[0]->init(PITCH_BUFF_SIZE, 8, e.sampleRate);
		pShifter[1]->init(PITCH_BUFF_SIZE, 8, e.sampleRate);

		lowpassFilter[0].setCutoff(20000 / e.sampleRate);
		lowpassFilter[1].setCutoff(20000 / e.sampleRate);
		highpassFilter[0].setCutoff(20 / e.sampleRate);
		highpassFilter[1].setCutoff(20 / e.sampleRate);
	}

	void getPlaybackOuput(float & out0, float & out1, int index){
		int ls = loopSize[playbackBuffer];		
		int pbi = index;

		while(pbi > ls) pbi -= ls;
		float o0 = buffers[playbackBuffer][pbi][0];
		float o1 = buffers[playbackBuffer][pbi][1];

		//Note toStart is calclauted two ways:
		//1. before wrapping
		//2. after wrapping, so the wrapped in segment doesn't clip
		// int toStart = std::min(index - fadeInStart, pbi);

		int toStart = index - fadeInStart;
		if(toStart < CROSS_FADE_AMT){
			float scalar = ((float)toStart/CROSS_FADE_AMT);
			scalar = clamp(scalar,0.f,1.0f);
			o0 *= scalar;
			o1 *= scalar;
		}

		// int toEnd = ls - pbi;
		// if(toEnd < CROSS_FADE_AMT){
		// 	float scalar = ((float)toEnd/CROSS_FADE_AMT);
		// 	scalar = clamp(scalar,0.f,1.0f);
		// 	o0 *= scalar;
		// 	o1 *= scalar;
		// }

		out0 = o0;
		out1 = o1;
	}

	void updateCubeLights(){
		for(int i = 0; i < 6; i++){
			float brightness;
			switch(bufferLockLevel[i]){
				case NONE:
					brightness = 1.f;
					break;
				case RECORD:
					brightness = 0.25f;
					break;
				case ALL:
				default:
					brightness = 0.f;
					break;
			}
			lights[CUBE_LIGHT + i].setBrightness(brightness);
		}
	}

	void updateRecordAndPlaybackLights(){
		for(int i = 0; i < BUFFER_COUNT; i++){
			lights[RP_LIGHT + i * 3 + 0].setBrightness(i == recordBuffer ? 1.f : 0);
			lights[RP_LIGHT + i * 3 + 1].setBrightness(i == playbackBuffer ? 1.f : 0);
		}
	}

	void record_jumpToNextTrack() {
		if(recordBuffer != -1){
			loopSize[recordBuffer] = clamp((int)recordIndex, 0, BUFFER_SIZE_MAX-CROSS_FADE_AMT);

			//Cross fade out the tail end of the current buffer
			for(int ci = 0; ci < CROSS_FADE_AMT; ci++){
				int i = loopSize[recordBuffer] - ci;
				if(i >= 0){
					float scalar = (float)ci/CROSS_FADE_AMT;
					buffers[recordBuffer][i][0] *= scalar;
					buffers[recordBuffer][i][1] *= scalar;
				}
			}

			//Also fade anything after the current end so the overflow mode doesn't have clicks
			for(int ci = 0; ci < CROSS_FADE_AMT; ci++){
				int i = loopSize[recordBuffer] + ci;
				if(i >= 0){
					float scalar = (float)ci/CROSS_FADE_AMT;
					buffers[recordBuffer][i][0] *= scalar;
					buffers[recordBuffer][i][1] *= scalar;
				}
			}

			//If intial recording, copy it over to the buffer 3 down
			if(recordBuffer < 3 && loopSize[recordBuffer + 3] == 0){
				int bi = recordBuffer + 3;
				int ls = loopSize[recordBuffer];
				loopSize[bi] = ls;
				for(int i = 0; i < ls; i++){
					buffers[bi][i][0] = buffers[recordBuffer][i][0];
					buffers[bi][i][1] = buffers[recordBuffer][i][1];
				}
			}
		}

		int freeBuffer = record_nextFreeBuffer();
		recordBuffer = freeBuffer;
		recordIndex = recordCrossFadePreBufferIndex - floor(recordCrossFadePreBufferIndex) + CROSS_FADE_AMT - 1;

		//Copy the PRE recording buffer into start of new buffer
		if(recordBuffer != -1){
			for(int ci = 0; ci < CROSS_FADE_AMT; ci++){
				int i = 1 + ci + floor(recordCrossFadePreBufferIndex);
				if(i >= CROSS_FADE_AMT) i -= CROSS_FADE_AMT;
				float scalar = (float)ci / CROSS_FADE_AMT;
				buffers[recordBuffer][ci][0] = recordCrossFadePreBuffer[i][0] * scalar;
				buffers[recordBuffer][ci][1] = recordCrossFadePreBuffer[i][1] * scalar;
			}
		}

		if(playbackBuffer == -1 && recordBuffer != -1){
			//If we don't hve a playback buffer, look for one
			//But only do this if we do have a recordBuffer, otherwise we might get into an infinate loop
			playback_jumpToNextTrack(true, false);
		}
		updateBufferLocks();
		updateRecordAndPlaybackLights();
	}

	//setCurCrossFadeTo0 is true if we've already handled the current frames output and we should set playbackCrossFadeBuffer[0] to 0
	void playback_jumpToNextTrack(bool forceResetPBI, bool setCurCrossFadeTo0) {

		bool runover = params[PLAYBACK_MODE_PARAM].getValue() == 0;

		if(playbackBuffer != -1){
			//Queue of the remainder of this buffer into the cross fade
			int ls = std::min(loopSize[playbackBuffer], BUFFER_SIZE_MAX);
			for(int ci = 0; ci < CROSS_FADE_AMT; ci++){
				int i = ci + playbackIndex;
				float cxFade0 = 0;
				float cxFade1 = 0;
				if(ci > 0 || !setCurCrossFadeTo0){
					if(runover){
						//while(i > ls) i -= ls;
						getPlaybackOuput(cxFade0,cxFade1,i);
					}else{
						if(i < ls){
							getPlaybackOuput(cxFade0,cxFade1,i);
						}
					}
					float scalar = 1.f-((float)ci/CROSS_FADE_AMT);
					cxFade0 *= scalar;
					cxFade1 *= scalar;
					//Also carry forward any previous crossFade
					int previousCrossFadeIndex = ci + playbackCrossFadeBufferIndex;
					if(previousCrossFadeIndex < CROSS_FADE_AMT){
						cxFade0 += playbackCrossFadeBuffer[previousCrossFadeIndex][0];
						cxFade1 += playbackCrossFadeBuffer[previousCrossFadeIndex][1];
					}
				}
				playbackCrossFadeBuffer[ci][0] = cxFade0;
				playbackCrossFadeBuffer[ci][1] = cxFade1;
			}
			playbackCrossFadeBufferIndex = 0;
		}

		if(forceResetPBI || !runover){
			playbackIndex = 0;
			fadeInStart = 0;
		}else{
			fadeInStart = playbackIndex;
		}

		if(playbackBuffer != -1){
			playbackRepeatCount ++;	
			
			int playbackRepeatMax = std::round(params[REPEATS_PARAM].getValue() * std::abs(inputs[REPEATS_CV_INPUT].getNormalVoltage(1.f)));
			if(playbackRepeatMax < 1) playbackRepeatMax = 1;

			if(playbackRepeatCount < playbackRepeatMax) return;
			playbackRepeatCount = 0;
		}		

		int freeBuffer = playback_nextFreeBuffer();
		playbackBuffer = freeBuffer;
		if(recordBuffer == -1){
			record_jumpToNextTrack();
		}
		updateBufferLocks();
		updateRecordAndPlaybackLights();
	}

	int record_nextFreeBuffer(){
		for(int i = 1; i <= BUFFER_COUNT; i++){
			int test = recordBuffer + i;
			while(test < 0) test += BUFFER_COUNT;
			while(test >= BUFFER_COUNT) test -= BUFFER_COUNT;
			if(isBufferFree(test,true)){
				return test;
			}
		}
		return -1;
	}

	int playback_nextFreeBuffer(){
		float pattern = params[PATERN_PARAM].getValue() * inputs[PATERN_CV_INPUT].getNormalVoltage(10.f)/10.f;

		if(pattern < 0){
			int patternIndex = std::round(-pattern*24);
			int maxJump = READ_PATTERN_NEG[patternIndex][nextReadPatternIndex];
			int firstJump = floor(rack::random::uniform() * rack::random::uniform() * maxJump + 1);
			for(int i = firstJump; i >= 1; i--){
				int test = playbackBuffer + i;
				while(test < 0) test += BUFFER_COUNT;
				while(test >= BUFFER_COUNT) test -= BUFFER_COUNT;
				if(isBufferFree(test,false)){
					return test;
				}
			}
			for(int i = firstJump+1; i < BUFFER_COUNT; i++){
				int test = playbackBuffer + i;
				while(test < 0) test += BUFFER_COUNT;
				while(test >= BUFFER_COUNT) test -= BUFFER_COUNT;
				if(isBufferFree(test,false)){
					return test;
				}
			}
		}else{
			int patternIndex = std::round(pattern*119);
			for(int i = 1; i < BUFFER_COUNT; i++){
				nextReadPatternIndex++;
				if(nextReadPatternIndex >= BUFFER_COUNT) nextReadPatternIndex = 0;
				int test = READ_PATTERN_POS[patternIndex][nextReadPatternIndex];

				while(test < 0) test += BUFFER_COUNT;
				while(test >= BUFFER_COUNT) test -= BUFFER_COUNT;
				if(isBufferFree(test,false)){
					return test;
				}
			}
		}
		return -1;
	}

	bool isBufferFree(int test, bool isRecord){
		if(test == playbackBuffer) return false;
		if(test == recordBuffer) return false;
		if(bufferLockLevel[test] == ALL) return false;
		if(bufferLockLevel[test] == RECORD && isRecord) return false;
		if(loopSize[test] == 0 && !isRecord) return false;
		return true;
	}

	void updateBufferLocks(){
		int recordCnt = 0;
		int frozenCnt = 0;
		for(int bi = 0; bi < BUFFER_COUNT; bi++){
			if(bufferLockLevel[bi] == NONE) recordCnt++;
			if(bufferLockLevel[bi] == ALL) frozenCnt++;
		}
		float lockChangeChance = 1.f - (params[FROST_PARAM].getValue() + params[FROST_CK_PARAM].getValue() * inputs[FROST_CV_INPUT].getVoltage()/10.f);

		if(rack::random::uniform() < lockChangeChance){
			int index = floor(rack::random::uniform() * BUFFER_COUNT);
			if(index == recordBuffer) return;
			if(index == playbackBuffer) return;
			int level = bufferLockLevel[index];
			if(recordCnt < 2) level = level - 1;
			else if(frozenCnt < 1) level = level + 1;
			else if(level == RECORD){
				if(rack::random::uniform() < 0.3f) level = (rack::random::uniform() < 0.5f) ? ALL : NONE;
			}else{
				if(rack::random::uniform() < 0.5f) level = RECORD;
			}
			level = clamp(level,NONE,ALL);
			bufferLockLevel[index] = (LockLevel)level;
			updateCubeLights();
		}
		
	}
};


struct IceTrayWidget : ModuleWidget {
	IceTrayWidget(IceTray* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/IceTray.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RotarySwitch<RoundBigBlackKnob>>(mm2px(Vec(24.892, 48.457)), module, IceTray::SPEED_NUM_PARAM));
		addParam(createParamCentered<RotarySwitch<RoundBigBlackKnob>>(mm2px(Vec(24.759, 84.597)), module, IceTray::SPEED_DENOM_PARAM));
		addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(95.815, 70.543)), module, IceTray::FROST_PARAM));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(38.803, 23.035)), module, IceTray::RECORD_LENGTH_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(46.972, 111.165)), module, IceTray::FEEDBACK_PARAM));		
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(109.893, 37.896)), module, IceTray::PATERN_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(94.19, 38.163)), module, IceTray::REPEATS_PARAM));

		addParam(createParamCentered<CKSS>(mm2px(Vec(81.562, 23.129)), module, IceTray::PLAYBACK_MODE_PARAM));

		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.58, 29.611)), module, IceTray::SPEED_NUM_CK_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(98.726, 86.827)), module, IceTray::FROST_CK_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(20.58, 100.34)), module, IceTray::SPEED_DENOM_CK_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(61.806, 110.63)), module, IceTray::FEEDBACK_CK_PARAM));

		addParam(createParamCentered<VCVButton>(mm2px(Vec(51.936, 42.374)), module, IceTray::CUBE_SWITCH_PARAM + 0));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(51.936, 67.321)), module, IceTray::CUBE_SWITCH_PARAM + 1));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(51.936, 92.267)), module, IceTray::CUBE_SWITCH_PARAM + 2));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(74.756, 42.422)), module, IceTray::CUBE_SWITCH_PARAM + 3));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(74.756, 67.321)), module, IceTray::CUBE_SWITCH_PARAM + 4));
		addParam(createParamCentered<VCVButton>(mm2px(Vec(74.851, 92.22)), module, IceTray::CUBE_SWITCH_PARAM + 5));

		
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.230, 58.512)), module, IceTray::LEFT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.230, 72.621)), module, IceTray::RIGHT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(49.942, 22.883)), module, IceTray::CLOCK_RECORD_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(71.911, 22.93)), module, IceTray::CLOCK_PLAYBACK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.61, 24.487)), module, IceTray::SPEED_NUM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.106, 49.952)), module, IceTray::REPEATS_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(109.675, 50.152)), module, IceTray::PATERN_CV_INPUT));		
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(110.877, 91.044)), module, IceTray::FROST_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(9.515, 103.673)), module, IceTray::SPEED_DENOM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(76.066, 110.288)), module, IceTray::FEEDBACK_CV_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(99.205, 108.852)), module, IceTray::LEFT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(110.318, 108.852)), module, IceTray::RIGHT_OUTPUT));

		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(51.936, 42.374)), module, IceTray::CUBE_LIGHT + 0));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(51.936, 67.321)), module, IceTray::CUBE_LIGHT + 1));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(51.936, 92.267)), module, IceTray::CUBE_LIGHT + 2));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(74.756, 42.422)), module, IceTray::CUBE_LIGHT + 3));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(74.756, 67.321)), module, IceTray::CUBE_LIGHT + 4));		
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(74.851, 92.22)), module, IceTray::CUBE_LIGHT + 5));

		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(48.61, 47.288)), module, IceTray::RP_LIGHT + 0 * 3));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(48.543, 72.144)), module, IceTray::RP_LIGHT + 1 * 3));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(48.543, 97.067)), module, IceTray::RP_LIGHT + 2 * 3));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(71.461, 47.288)), module, IceTray::RP_LIGHT + 3 * 3));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(71.394, 72.144)), module, IceTray::RP_LIGHT + 4 * 3));
		addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(71.461, 97.0)), module, IceTray::RP_LIGHT + 5 * 3));
	}

	void appendContextMenu(Menu* menu) override {
		IceTray* module = dynamic_cast<IceTray*>(this->module);

		menu->addChild(new MenuEntry);
		menu->addChild(createMenuLabel("Ice Tray"));

		struct ClearCubes : MenuItem {
			IceTray* module;
			void onAction(const event::Action& e) override {
				module->clearCubes();
			}
		};

		{
			ClearCubes* menuItem = createMenuItem<ClearCubes>("Clear Cubes");
			menuItem->module = module;
			menu->addChild(menuItem);
		}

		menu->addChild(new MenuEntry);
		menu->addChild(createMenuLabel("Pitch Correction"));

		struct PitchCorrectionMenuItem : MenuItem {
			IceTray* module;
			bool value;
			void onAction(const event::Action& e) override {
				module->pitchCorrectionOn = value;
			}
		};

		PitchCorrectionMenuItem* mi = createMenuItem<PitchCorrectionMenuItem>("On");
		mi->rightText = CHECKMARK(module->pitchCorrectionOn);
		mi->module = module;
		mi->value = true;
		menu->addChild(mi);

		mi = createMenuItem<PitchCorrectionMenuItem>("Off (Saves CPU)");
		mi->rightText = CHECKMARK(!module->pitchCorrectionOn);
		mi->module = module;
		mi->value = false;
		menu->addChild(mi);
	}
};


Model* modelIceTray = createModel<IceTray, IceTrayWidget>("IceTray");
