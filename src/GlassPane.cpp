/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include "plugin.hpp"
#include "util.hpp"
#include "cvRange.hpp"

//Disable Debug Macro
#undef DEBUG
#define DEBUG(...) {}

#define NODE_MAX 16
#define NODE_IN_MAX 2
#define NODE_OUT_MAX 3
#define NODE_STATE_MAX 4
#define NO_STATE -1
#define NODE_MODE_MAX 3
#define CLOCK_HISTORY_MAX 24 //How many clock lengths to store from the past and use when arpeggiating

#define ARPEGGIATE_SPEED_MAX 5
const std::string ARP_SPEEDS_LABELS [] = {
	"Dynamic",
	"Whole Notes",
	"Half Notes",
	"Triplets",
	"Quarter Notes",
};

#define WEIGHTING_MAX 12
const int WEIGHTING [] = {0,1,0,2,0,1,0,3,0,1,0,2};
const int WEIGHTING_COUNTS [] = {6,3,2,1};

struct GlassPane : Module {
	enum ParamId {
		ENUMS(MODE_BUTTON_PARAM,NODE_MAX),
		ENUMS(CV_KNOB_PARAM,NODE_MAX),
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		ENUMS(NODE_IN_INPUT,NODE_MAX * NODE_IN_MAX),
		ENUMS(MODE_TRIGGER_INPUT,NODE_MAX),
		INPUTS_LEN
	};
	enum OutputId {
		GATE_OUTPUT,
		CV_OUTPUT,
		ENUMS(NODE_OUT_OUTPUT,NODE_MAX * NODE_OUT_MAX),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(MODE_LIGHT_LIGHT,NODE_MAX * 3),
		ENUMS(STATE_LIGHT,NODE_MAX * NODE_STATE_MAX),
		ENUMS(ACTIVE_LIGHT,NODE_MAX),
		LIGHTS_LEN
	};

	enum NodeMode{
		Cycle,
		Random,
		Arpeggiate
	};

	struct ModeParamQuantity : SwitchQuantity  {
		void setValue(float value) override {
			SwitchQuantity::setValue(value);

			auto _module = dynamic_cast<GlassPane*>(module);
			int ni = this->paramId - MODE_BUTTON_PARAM;
			_module->setModeLight(ni);
			_module->nodes[ni].manualMode = (NodeMode)(int)value;
			DEBUG("Set Manual Mode on %i to %f",ni,value);
		}
	};

	enum TriggerSource{
		TS_Input,
		TS_Clock,
		TS_Arpeggiate,
	};

	struct Node{
		//Persisted
		int state = NO_STATE; //-1 is off, 0 is self, 1 to 3 are outputs A, B, C
		NodeMode manualMode = Cycle;
		TriggerSource triggerSource = TS_Input;
		bool arpeggiating = false;
		
		//Non Persisted
		bool inHigh [NODE_IN_MAX] = {};		
		bool modeTriggerHigh = false;

		json_t *dataToJson() {
			json_t *jobj = json_object();
			json_object_set_new(jobj, "state", json_integer(state));
			json_object_set_new(jobj, "manualMode", json_integer(manualMode));
			json_object_set_new(jobj, "triggerSource", json_integer(triggerSource));
			json_object_set_new(jobj, "arpeggiating", json_bool(arpeggiating));
			return jobj;
		}

		void dataFromJson(json_t *jobj) {
			state = json_integer_value(json_object_get(jobj, "state"));
			manualMode = (NodeMode)json_integer_value(json_object_get(jobj, "manualMode"));
			DEBUG("Loading Manual Mode from jason as %i",manualMode);
			triggerSource = (TriggerSource)json_integer_value(json_object_get(jobj, "triggerSource"));
			arpeggiating = json_bool_value(json_object_get(jobj, "arpeggiating"));
		}
	};

	//Non Persisted State
	bool clockHigh;
	bool resetHigh;
	int clockCounter;
	int clockLength;

	bool endOfLine;

	bool firstProcess;

	//Persisted State
	Node nodes [NODE_MAX];
	int activeNode;

	//Arpeggiate
	int arpeggiateNode; //Root arpeggiating Note	
	int arpeggiateCounter; //Number of process calls left before the arpeggiate gate goes up or down
	int arpeggiateLength; //The total number of process calls between the arpeggiate gate going up or down 
	int arpeggiateLeft; //The number of times the arpeggiate gate will go down again before exiting arpeggiation
	bool arpeggiateHigh; //If the arpeggiate gate is currently high

	//Context Menu State
	CVRange range;
	int arpeggiateSpeed;
	bool weightedOdds;
	bool weightedCycle;

	GlassPane() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV_OUTPUT, "CV");

		for(int ni = 0; ni < NODE_MAX; ni++){
			configSwitch<ModeParamQuantity>(MODE_BUTTON_PARAM + ni, 0.f, 2.f, 0.f, "Mode ", std::vector<std::string>{"Cycle","Random","Ratchet"});
			configParam<CVRangeParamQuantity<GlassPane>>(CV_KNOB_PARAM + ni, 0.f, 1.f, 0.5f, "CV", "V");
			configInput(MODE_TRIGGER_INPUT + ni, "Mode Trigger");
			for(int ii = 0 ; ii < NODE_IN_MAX; ii ++){
				char alphabet = 'X' + ii;
				std::string str(1, alphabet);
				configInput(NODE_IN_INPUT + ni * NODE_IN_MAX + ii, str);
			}

			for(int oi = 0 ; oi < NODE_OUT_MAX; oi ++){
				char alphabet = 'A' + oi;
				std::string str(1, alphabet);
				configOutput(NODE_OUT_OUTPUT + ni * NODE_OUT_MAX + oi, str);
			}
		}
		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	void initalize(){
		for(int ni = 0; ni < NODE_MAX; ni++){
			nodes[ni] = Node();
		}
		activeNode = 0;
		arpeggiateNode = NO_STATE;
		clockHigh = false;
		resetHigh = false;
		clockCounter = 0;
		clockLength = 0;
		arpeggiateCounter = 0;
		arpeggiateLength = 0;
		arpeggiateLeft = 0;
		arpeggiateHigh = false;
		endOfLine = false;
		arpeggiateSpeed = 2;
		weightedOdds = false;
		weightedCycle = false;
		range = Bipolar_1;
		firstProcess = true;
	}

	json_t *dataToJson() override{
		json_t *jobj = json_object();

		json_t *nodesJ = json_array();
		for(int ni = 0; ni < NODE_MAX; ni++){
			json_array_insert_new(nodesJ, ni, nodes[ni].dataToJson());
		}
		json_object_set_new(jobj, "nodes", nodesJ);
		json_object_set_new(jobj, "activeNode", json_integer(activeNode));

		//Arpeggiate
		json_object_set_new(jobj, "arpeggiateNode", json_integer(arpeggiateNode));
		json_object_set_new(jobj, "arpeggiateCounter", json_integer(arpeggiateCounter));
		json_object_set_new(jobj, "arpeggiateLength", json_integer(arpeggiateLength));
		json_object_set_new(jobj, "arpeggiateLeft", json_integer(arpeggiateLeft));
		json_object_set_new(jobj, "arpeggiateHigh", json_bool(arpeggiateHigh));

		//Context Menu State
		json_object_set_new(jobj, "range", json_integer(range));
		json_object_set_new(jobj, "arpeggiateSpeed", json_integer(arpeggiateSpeed));
		json_object_set_new(jobj, "weightedOdds", json_bool(weightedOdds));
		json_object_set_new(jobj, "weightedCycle", json_bool(weightedCycle));

		return jobj;
	}

	void dataFromJson(json_t *jobj) override {					
		json_t *nodesJ = json_object_get(jobj,"nodes");
		for(int ni = 0; ni < NODE_MAX; ni++){
			nodes[ni].dataFromJson(json_array_get(nodesJ,ni));
		}
		activeNode = json_integer_value(json_object_get(jobj, "activeNode"));

		//Arpeggiate
		arpeggiateNode = json_integer_value(json_object_get(jobj, "arpeggiateNode"));
		arpeggiateCounter = json_integer_value(json_object_get(jobj, "arpeggiateCounter"));
		arpeggiateLength = json_integer_value(json_object_get(jobj, "arpeggiateLength"));
		arpeggiateLeft = json_integer_value(json_object_get(jobj, "arpeggiateLeft"));
		arpeggiateHigh = json_bool_value(json_object_get(jobj, "arpeggiateHigh"));

		//Context Menu State
		range = (CVRange)json_integer_value(json_object_get(jobj, "range"));
		arpeggiateSpeed = json_integer_value(json_object_get(jobj, "arpeggiateSpeed"));
		weightedOdds = json_bool_value(json_object_get(jobj, "weightedOdds"));
		weightedCycle = json_bool_value(json_object_get(jobj, "weightedCycle"));		
	}

	void process(const ProcessArgs& args) override {

		if(firstProcess){
			firstProcess = false;
			for(int ni = 0; ni < NODE_MAX; ni++){
				setModeLight(ni);
			}
		}

		//Clock In
		bool clockHighEvent = false; bool clockLowEvent = false;
		schmittTrigger(clockHigh,inputs[CLOCK_INPUT].getVoltage(),clockHighEvent, clockLowEvent);

		if(clockHighEvent) DEBUG("clockHighEvent");
		if(clockLowEvent) DEBUG("clockLowEvent");

		countClockLength(clockCounter,clockLength,clockHighEvent);

		//Arpeggiate Clock
		bool arpHighEvent = false; bool arpLowEvent = false;
		if(arpeggiateCounter > 0){
			if(arpeggiateSpeed == 1){
				//Special case for whole note speed
				//In this case we don't use a counter and instead just pass the clock events on
				arpHighEvent = clockHighEvent;
				arpLowEvent = clockLowEvent;
			}else{
				//We are Arpeggiating
				arpeggiateCounter--;
				if(arpeggiateCounter == 0){
					//We hit an arpeggiate clock transition
					if(arpeggiateHigh){
						//Low Transition
						arpLowEvent = true;
						arpeggiateHigh = false;					
					}else{
						//High Transition (or End)
						arpHighEvent = true;
						arpeggiateHigh = true;
					}					
				}
			}
		}
		if(arpHighEvent){
			//High Transition
			arpeggiateLeft --;
			if(arpeggiateLeft > 0){
				resetArpeggiateCounter();
				setActiveNode(arpeggiateNode);
				DEBUG("Arp going High. Left:%i",arpeggiateLeft);
			}else{
				//End arpegiation
				arpHighEvent = false; //Consume arpHightEvent
				DEBUG("End of arpegiation");
				if(activeNode == arpeggiateNode){
					DEBUG("End of arpegiation - setActiveNode(0)");
					//Prevent endless arpeggiating on the same note
					setActiveNode(0);
				}
				cleanUpArpeggiation();
			}
		}
		if(arpLowEvent){
			resetArpeggiateCounter();
			DEBUG("Arp going Low. Left:%i",arpeggiateLeft);
		}
		

		//Reset In
		if(schmittTrigger(resetHigh,inputs[RESET_INPUT].getVoltage())){
			DEBUG("reset triggering");
			setActiveNode(0);
			cleanUpArpeggiation();
			clockLowEvent = true;
			for(int ni = 0; ni < NODE_MAX; ni++){
				nodes[ni].state = NO_STATE;
				DEBUG("node %i manualMode:%i mode:%f",ni,nodes[ni].manualMode,params[MODE_BUTTON_PARAM + ni].getValue());
				params[MODE_BUTTON_PARAM + ni].setValue((float)(int)nodes[ni].manualMode);
				DEBUG("node %i after mode:%f",ni,params[MODE_BUTTON_PARAM + ni].getValue());
				setModeLight(ni);//setValue on param doesn't trigger setValue on ParamQuantity, so this is needed here
				clearNodeOutputs(ni);
				updateNodeLights(ni);
			}
		}

		//Reset Handle
		if(endOfLine){
			setActiveNode(0);
			clockHighEvent = true;
			endOfLine = false;
			DEBUG("endOfLine triggering clockHighEvent");
		}

		int activeNode_snapShot = activeNode;

		//Main Gate Output
		//Has to come before node loop since it consumes some events
		if(arpHighEvent || (clockHighEvent && arpeggiateCounter <= 0)){
			DEBUG("Main Gate High");
			outputs[GATE_OUTPUT].setVoltage(10.f);
		}
		if(arpLowEvent || (clockLowEvent && arpeggiateCounter <= 0)){
			DEBUG("Main Gate Low");				
			outputs[GATE_OUTPUT].setVoltage(0.f);
		}

		//Loop through Nodes
		for(int ni = 0; ni < NODE_MAX; ni++){
			Node & node = nodes[ni];

			//Mode Trigger
			if(schmittTrigger(node.modeTriggerHigh,inputs[MODE_TRIGGER_INPUT + ni].getVoltage())){
				NodeMode newMode = (NodeMode)((getNodeMode(ni) + 1) % NODE_MODE_MAX);
				//Set value directly to avoid the logic that updates manualMode inside ModeParamQuantity.setValue
				APP->engine->setParamValue(this, MODE_BUTTON_PARAM + ni, newMode);
				//But then we have to update the light ourself too
				setModeLight(ni);
			}
			
			//Input Detection
			bool inputHighEvent = false; bool inputLowEvent = false;
			for(int ii = 0 ; ii < NODE_IN_MAX; ii ++){	
				schmittTrigger(node.inHigh[ii],inputs[NODE_IN_INPUT + ni * NODE_IN_MAX + ii].getVoltage(),inputHighEvent,inputLowEvent);
			}

			//Note we don't put the arp checks inside arpeggiateCounter > 0 because
			//then we miss the last gate down because the logic above clears it on
			//the same frame it set the arp events
			if(arpHighEvent && ni == activeNode_snapShot){
				arpHighEvent = false; //Consume high event
				inputHighEvent = true;
				node.triggerSource = TS_Arpeggiate;
				DEBUG("High caused by ARP");
			}
			if(arpLowEvent && node.triggerSource == TS_Arpeggiate){
				node.triggerSource = TS_Input; //revert back to default state
				inputLowEvent = true;
				DEBUG("Low caused by ARP");
			}
			if(arpeggiateCounter <= 0){
				if(clockHighEvent && ni == activeNode_snapShot){
					clockHighEvent = false; //Consume high event
					inputHighEvent = true;
					node.triggerSource = TS_Clock;
					DEBUG("High caused by CLOCK | ni:%i | clockHighEvent is now %i",ni,clockHighEvent);
				}
				if(clockLowEvent && node.triggerSource == TS_Clock){
					node.triggerSource = TS_Input; //revert back to default state
					inputLowEvent = true;
					DEBUG("Low caused by CLOCK");
				}
			}

			//Input High Event
			if(inputHighEvent){
				nodeHighEvent(ni);

				// DEBUG("inputHighEvent ni:%i, activeNode:%i, activeNode_snapShot:%i",ni,activeNode,activeNode_snapShot);

				// if(ni == activeNode_snapShot){
				// 	DEBUG("Main Gate High");
				// 	outputs[GATE_OUTPUT].setVoltage(10.f);
				// }
			}

			if(inputLowEvent){
				nodeLowEvent(ni);

				// DEBUG("inputLowEvent ni:%i, activeNode:%i, activeNode_snapShot:%i",ni,activeNode,activeNode_snapShot);

				// if(ni == activeNode_snapShot){
				// 	DEBUG("Main Gate Low");				
				// 	outputs[GATE_OUTPUT].setVoltage(0.f);	
				// }
			}
		}
	}

	void cleanUpArpeggiation(){		
		arpeggiateLeft = 0;
		arpeggiateNode = NO_STATE;
		arpeggiateCounter = 0;
		arpeggiateHigh = false;
		for(int j = 0; j < NODE_MAX; j++) nodes[j].arpeggiating = false;
	}

	void nodeHighEvent(int ni){
		DEBUG("nodeHighEvent ni:%i",ni);
		Node & node = nodes[ni];

		//Arpeggiate -> Cycle Ghosting
		bool doWeightedCycle = weightedCycle;
		NodeMode mode = getNodeMode(ni);
		//When arpeggiating, use cycle logic
		if(node.arpeggiating){
			mode = Cycle;
			doWeightedCycle = false;
		}
		//Prevent arpeggiation when clock length is 0
		if(mode == Arpeggiate && clockLength <= 0){
			mode = Cycle; 
			doWeightedCycle = false;
		}

		//Mode Logic
		switch(mode){
			case Cycle:{
				setActiveNode(ni);
				if(doWeightedCycle){
					bool firstPlay = node.state == -1;
					int prevOutput = node.state == -1 ? -1 : WEIGHTING[node.state];
					for(int i = 0; i < WEIGHTING_MAX; i++){
						node.state++;
						if(node.state >= WEIGHTING_MAX){
							node.state = 0;
						}
						int output = WEIGHTING[node.state];
						if(isStateConnected(ni,output)){
							if(!firstPlay && nothingConnected(ni)){
								//Special case for nothing is connected, after first play go back node 0.
								//Note check for output != 0 is reundant with nothingConnected
								node.state = NO_STATE;
								DEBUG("Weighted Cycle, no connections triggering endOfLine");
								endOfLine = true;
							}else if(output == prevOutput){
								//Prevent Repeated outputs. Can happen if certain output ports are not conected
								continue;
							}else{
								//Otherwise play new output
								setNodeOutput(ni,output);
							}
							break;
						}
					}
				}else{
					for(int i = 0; i < NODE_STATE_MAX; i++){
						node.state++;
						if(node.state >= NODE_STATE_MAX){
							if(nothingConnected(ni)){
								//If nothing is connected then go back to no state and trigger end of line
								node.state = NO_STATE;
								DEBUG("Flat Cycle, no connections triggering endOfLine");
								endOfLine = true;
								break;
							}else{
								node.state = 0;
							}
						}
						if(isStateConnected(ni,node.state)){
							setNodeOutput(ni,node.state);
							break;
						}
					}
				}
			}break;
			case Random:{
				setActiveNode(ni);
				std::vector<int> options;
				for(int si = 0; si < NODE_STATE_MAX; si++){
					if(si == 0 && si == node.state){
						//Don't repeate state 0
						continue;
					}
					if(isStateConnected(ni,si)){
						//Decide how to weight this option
						int weight = weightedOdds ? WEIGHTING_COUNTS[si] : 1;
						for(int x = 0; x < weight; x++){
							options.push_back(si);
						}
					}
				}
				int optionCount = options.size();
				if(optionCount > 0){
					//Select an option
					int selectedIndex = std::floor(rack::random::uniform() * optionCount);
					int selectedOutput = options[selectedIndex];
					node.state = selectedOutput;
					setNodeOutput(ni,selectedOutput);
				}else{
					//If there are no options reset
					//This can happen if nothing is connected because of the check above that prevents repeat state 0
					node.state = NO_STATE;
					DEBUG("Random triggering endOfLine");
					endOfLine = true;
				}
			}break;
			case Arpeggiate:{
				int connectedCount = 1;						
				for(int oi = 0 ; oi < NODE_OUT_MAX; oi ++){
					if(outputs[NODE_OUT_OUTPUT + ni * NODE_OUT_MAX + oi].isConnected()){
						connectedCount ++;
					}
				}
				if(arpeggiateNode == -1){
					arpeggiateNode = ni;
					node.state = 0; //Only reset cur output if this is the root arpeggiate note, otherwise keep it at prev value for more chaos 
				}
				if(arpeggiateSpeed <= 0){
					//Dynamic Speed
					arpeggiateLength = clockLength / (connectedCount * 2);
					arpeggiateLeft += connectedCount;
				}else{
					//Fixed Speed
					arpeggiateLength = clockLength / (arpeggiateSpeed * 2);
					//Round up to the nearest multiple of arpeggiateSpeed so we don't end up with breaks in the beat
					arpeggiateLeft += std::ceil((float)connectedCount / arpeggiateSpeed) * arpeggiateSpeed;
				}
				DEBUG("Arp starting. Left:%i",arpeggiateLeft);

				resetArpeggiateCounter();
				arpeggiateHigh = true;
				node.arpeggiating = true;
				node.triggerSource = TS_Arpeggiate;
				setActiveNode(ni);
				setNodeOutput(ni,node.state);
			}break;
		}

	}

	inline void resetArpeggiateCounter(){
		arpeggiateCounter = arpeggiateLength;
		arpeggiateCounter --; //little bit of padding
		if(arpeggiateCounter < 10) arpeggiateCounter = 10;
		DEBUG("resetArpeggiateCounter to %i",arpeggiateCounter);
	}

	inline void nodeLowEvent(int ni){
		DEBUG("nodeLowEvent ni:%i",ni);
		clearNodeOutputs(ni);
	}

	void setNodeOutput(int ni, int output){
		DEBUG("setNodeOutput ni:%i output:%i",ni,output);
		{
			bool selfOn = output == 0;
			lights[STATE_LIGHT + ni * NODE_STATE_MAX].setBrightness(selfOn ? 10.f : 0.f);
		}
		int outPort = output - 1;
		for(int oi = 0; oi < NODE_OUT_MAX; /*done inline*/){
			bool isOn = outPort == oi;
			outputs[NODE_OUT_OUTPUT + ni * NODE_OUT_MAX + oi].setVoltage(isOn ? 10.f : 0.f);
			oi++;//Doing this here since light indexes are +1
			lights[STATE_LIGHT + ni * NODE_STATE_MAX + oi].setBrightness(isOn ? 1.f : 0.f);
		}
	}

	inline NodeMode getNodeMode(int ni){
		return (NodeMode)(int)params[MODE_BUTTON_PARAM + ni].getValue();
	}

	void setActiveNode(int node){

		//Update Internal State
		activeNode = node;

		//Update CV Output
		float cv = 0;
		if(activeNode >= 0) cv = params[CV_KNOB_PARAM + activeNode].getValue();
		outputs[CV_OUTPUT].setVoltage(mapCVRange(cv,range));

		//Update Active Lights
		for(int ni = 0; ni < NODE_MAX; ni++){
			if(activeNode == ni){
				lights[ACTIVE_LIGHT + ni].setBrightness(1.f);
			}else if(arpeggiateNode == ni){
				lights[ACTIVE_LIGHT + ni].setBrightness(0.3f);
			}else{
				lights[ACTIVE_LIGHT + ni].setBrightness(0.f);
			}
		}
	}

	void clearNodeOutputs(int node){
		for(int oi = 0; oi < NODE_OUT_MAX; oi ++){
			outputs[NODE_OUT_OUTPUT + node * NODE_OUT_MAX + oi].setVoltage(0.f);
		}
	}

	void updateNodeLights(int node){
		for(int si = 0; si < NODE_STATE_MAX; si ++){
			lights[STATE_LIGHT + node * NODE_STATE_MAX + si].setBrightness(0.f);
		}
	}

	void setModeLight(int ni){
		switch(getNodeMode(ni)){
			case Cycle:
				lights[MODE_LIGHT_LIGHT + ni * 3 + 0].setBrightness(0.f);
				lights[MODE_LIGHT_LIGHT + ni * 3 + 1].setBrightness(0.f);
				lights[MODE_LIGHT_LIGHT + ni * 3 + 2].setBrightness(1.f);
				break;
			case Random:
				lights[MODE_LIGHT_LIGHT + ni * 3 + 0].setBrightness(180/255.f);
				lights[MODE_LIGHT_LIGHT + ni * 3 + 1].setBrightness(50/255.f);
				lights[MODE_LIGHT_LIGHT + ni * 3 + 2].setBrightness(5/255.f);
				break;
			case Arpeggiate:
				lights[MODE_LIGHT_LIGHT + ni * 3 + 0].setBrightness(180/255.f);
				lights[MODE_LIGHT_LIGHT + ni * 3 + 1].setBrightness(0/255.f);
				lights[MODE_LIGHT_LIGHT + ni * 3 + 2].setBrightness(180/255.f);
				break;
		}
	}

	bool isStateConnected(int nodeIndex, int state){
		if(state == 0) return true;
		int outPort = state - 1;
		return outputs[NODE_OUT_OUTPUT + nodeIndex * NODE_OUT_MAX + outPort].isConnected();
	}

	bool nothingConnected(int nodeIndex){
		for(int oi = 0; oi < NODE_OUT_MAX; oi ++){
			if(outputs[NODE_OUT_OUTPUT + nodeIndex * NODE_OUT_MAX + oi].isConnected()){
				return false;
			}
		}
		return true;
	}
};


const Vec PANE_POS [] = {
	Vec(7.544, 25.868),	Vec(49.172, 25.868),Vec(90.8, 25.868),Vec(132.427, 25.868),
	Vec(7.544, 50.562),Vec(49.172, 50.562),Vec(90.8, 50.562),Vec(132.427, 50.562),
	Vec(7.544, 75.257),Vec(49.172, 75.257),Vec(90.8, 75.257),Vec(132.427, 75.257),
	Vec(7.544, 99.951),Vec(49.172, 99.951),Vec(90.8, 99.951),Vec(132.427, 99.951)
};

struct WhiteKnob : RoundKnob {
	WhiteKnob(){
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/WhiteKnob.svg")));
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/WhiteKnob_bg.svg")));
	}
};

struct GlassPaneWidget : ModuleWidget {
	GlassPaneWidget(GlassPane* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GlassPane.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.544, 13.571)), module, GlassPane::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(16.847, 13.571)), module, GlassPane::RESET_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(151.033, 13.571)), module, GlassPane::GATE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(160.336, 13.571)), module, GlassPane::CV_OUTPUT));

		for(int ni = 0; ni < NODE_MAX; ni++){
			Vec delta = PANE_POS[ni] - PANE_POS[0];
			addParam(createParamCentered<ModeSwitch<VCVButton>>(mm2px(delta+Vec(26.15, 25.868)), module, GlassPane::MODE_BUTTON_PARAM + ni));
			addParam(createParamCentered<WhiteKnob>(mm2px(delta+Vec(7.544, 36.457)), module, GlassPane::CV_KNOB_PARAM + ni));		
			addInput(createInputCentered<PJ301MPort>(mm2px(delta+Vec(7.544, 25.868)), module, GlassPane::NODE_IN_INPUT + ni * NODE_IN_MAX + 0));
			addInput(createInputCentered<PJ301MPort>(mm2px(delta+Vec(16.847, 25.868)), module, GlassPane::NODE_IN_INPUT + ni * NODE_IN_MAX + 1));
			addInput(createInputCentered<PJ301MPort>(mm2px(delta+Vec(35.453, 25.868)), module, GlassPane::MODE_TRIGGER_INPUT + ni));				
			addOutput(createOutputCentered<PJ301MPort>(mm2px(delta+Vec(16.847, 36.451)), module, GlassPane::NODE_OUT_OUTPUT + ni * NODE_OUT_MAX + 0));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(delta+Vec(26.15, 36.451)), module, GlassPane::NODE_OUT_OUTPUT + ni * NODE_OUT_MAX + 1));
			addOutput(createOutputCentered<PJ301MPort>(mm2px(delta+Vec(35.453, 36.451)), module, GlassPane::NODE_OUT_OUTPUT + ni * NODE_OUT_MAX + 2));
			addChild(createLightCentered<VCVBezelLight<RedGreenBlueLight>>(mm2px(delta+Vec(26.15, 25.868)), module, GlassPane::MODE_LIGHT_LIGHT + ni * 3));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(11.055, 40.484)), module, GlassPane::STATE_LIGHT + ni * NODE_STATE_MAX + 0));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(20.487, 40.484)), module, GlassPane::STATE_LIGHT + ni * NODE_STATE_MAX + 1));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(29.918, 40.484)), module, GlassPane::STATE_LIGHT + ni * NODE_STATE_MAX + 2));
			addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(39.35, 40.484)), module, GlassPane::STATE_LIGHT + ni * NODE_STATE_MAX + 3));
			{
				auto light = createLight<RectangleLight<BlueLight>>(mm2px(delta+Vec(2.2, 30.364)), module, GlassPane::ACTIVE_LIGHT + ni);
				light->box.size = mm2px(Vec(38.719, 1.590));
				light->module = module;
				addChild(light);
			}

		}
	}

	void appendContextMenu(Menu* menu) override {
		auto module = dynamic_cast<GlassPane*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("GlassPane"));

		addRangeSelectMenu<GlassPane>(module,menu);

		menu->addChild(createSubmenuItem("Cycle", module->weightedCycle ? "Weighted" : "Evenly",
			[=](Menu* menu) {
				menu->addChild(createMenuLabel("Controls if Cylce steps play Evenly, or Weighted to output A."));
				menu->addChild(createMenuItem("Evenly", CHECKMARK(module->weightedCycle == false), [module]() { 
					module->weightedCycle = false;
				}));
				menu->addChild(createMenuItem("Weighted", CHECKMARK(module->weightedCycle == true), [module]() { 
					module->weightedCycle = true;
				}));
			}
		));

		menu->addChild(createSubmenuItem("Odds", module->weightedOdds ? "Weighted" : "Evenly",
			[=](Menu* menu) {
				menu->addChild(createMenuLabel("Controls if Random steps are Evenly distributed or Weighted to output A."));
				menu->addChild(createMenuItem("Evenly", CHECKMARK(module->weightedOdds == false), [module]() { 
					module->weightedOdds = false;
				}));
				menu->addChild(createMenuItem("Weighted", CHECKMARK(module->weightedOdds == true), [module]() { 
					module->weightedOdds = true;
				}));
			}
		));

		menu->addChild(createSubmenuItem("Ratchet Speed", ARP_SPEEDS_LABELS[module->arpeggiateSpeed],
			[=](Menu* menu) {
				menu->addChild(createMenuLabel("Change note subdvision when at an Ratchet step."));
				for(int i = 0; i < ARPEGGIATE_SPEED_MAX; i++){
					menu->addChild(createMenuItem(ARP_SPEEDS_LABELS[i], CHECKMARK(module->arpeggiateSpeed == i), [module,i]() { 
						module->arpeggiateSpeed = i;
					}));
				}
			}
		));		
	}
};


Model* modelGlassPane = createModel<GlassPane, GlassPaneWidget>("GlassPane");