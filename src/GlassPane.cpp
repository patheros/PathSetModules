/*
Copyright: Andrew Hanson
License: GNU GPL-3.0
*/

#include "plugin.hpp"
#include "util.hpp"
#include "cvRange.hpp"

//Disable Debug Macro
//#undef DEBUG
//#define DEBUG(...) {}

#define NODE_MAX_PANE 16
#define NODE_MAX_PLUS 8
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

using std::vector;

struct GPRoot : Module {

	enum NodeMode{
		Cycle,
		Random,
		Arpeggiate
	};

	struct ModeParamQuantity : SwitchQuantity  {
		void setValue(float value) override {
			SwitchQuantity::setValue(value);

			auto _module = dynamic_cast<GPRoot*>(module);
			int ni = this->paramId - _module->modeButtonParam;
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
		
		//Non Persisted
		bool inHigh [NODE_IN_MAX] = {};		
		bool modeTriggerHigh = false;

		json_t *dataToJson() {
			json_t *jobj = json_object();
			json_object_set_new(jobj, "state", json_integer(state));
			json_object_set_new(jobj, "manualMode", json_integer(manualMode));
			json_object_set_new(jobj, "triggerSource", json_integer(triggerSource));
			return jobj;
		}

		void dataFromJson(json_t *jobj) {
			state = json_integer_value(json_object_get(jobj, "state"));
			manualMode = (NodeMode)json_integer_value(json_object_get(jobj, "manualMode"));
			DEBUG("Loading Manual Mode from jason as %i",manualMode);
			triggerSource = (TriggerSource)json_integer_value(json_object_get(jobj, "triggerSource"));
		}
	};


	struct ProcessContext{
		//Persistant
		int activeNode = 0;
		float activeVoltage = 0;

		//Not Persistant
		int clockLength = 0;
		bool clockLowEvent = false;
		bool clockHighEvent = false;
		int activeNode_snapShot = 0;
		bool endOfLine = false;

		//Arpeggiate (Not Persistant)
		int arpeggiateNode = 0; //Root arpeggiating Note
		int arpeggiateCounter = 0; //Number of process calls left before the arpeggiate gate goes up or down
		int arpeggiateLength = 0; //The total number of process calls between the arpeggiate gate going up or down 
		int arpeggiateLeft = 0; //The number of times the arpeggiate gate will go down again before exiting arpeggiation
		bool arpeggiateHigh = false; //If the arpeggiate gate is currently high
		bool arpHighEvent = false;
		bool arpLowEvent = false;

	};


	//Config
	int modeLight;
	int stateLight;
	int activeLight;
	int nodeMax;

	int modeButtonParam;
	int nodeOutput;
	int modeTriggerInput;
	int nodeInput;
	int cvKnobParam;	

	//Non Persisted State
	bool firstProcess;

	//Persisted State
	vector<Node> nodes;

	//Context Menu State
	CVRange range;
	int arpeggiateSpeed;
	bool weightedOdds;
	bool weightedCycle;

	GPRoot() {
		//Sub class is expected to call initalize();
		modeLight = 0;
		stateLight = 0;
		activeLight = 0;
		modeButtonParam = 0;
		modeTriggerInput = 0;
		cvKnobParam = 0;
	}

	void configNodes(int modeButtonParam, int cvKnobParam, int modeTriggerInput, int nodeInput, int nodeOutput){
		DEBUG("configNodes A");
		this->modeButtonParam = modeButtonParam;
		this->nodeOutput = nodeOutput;
		this->modeTriggerInput = modeTriggerInput;
		this->nodeInput = nodeInput;
		this->cvKnobParam = cvKnobParam;
		DEBUG("configNodes B nodeMax %i",nodeMax);
		for(int ni = 0; ni < nodeMax; ni++){
			configSwitch<ModeParamQuantity>(modeButtonParam + ni, 0.f, 2.f, 0.f, "Mode ", std::vector<std::string>{"Cycle","Random","Ratchet"});
			configParam<CVRangeParamQuantity<GPRoot>>(cvKnobParam + ni, 0.f, 1.f, 0.5f, "CV", "V");
			configInput(modeTriggerInput + ni, "Mode Trigger");
			for(int ii = 0 ; ii < NODE_IN_MAX; ii ++){
				char alphabet = 'X' + ii;
				std::string str(1, alphabet);
				configInput(nodeInput + ni * NODE_IN_MAX + ii, str);
			}

			for(int oi = 0 ; oi < NODE_OUT_MAX; oi ++){
				char alphabet = 'A' + oi;
				std::string str(1, alphabet);
				configOutput(nodeOutput + ni * NODE_OUT_MAX + oi, str);
			}
			DEBUG("configNodes C ni %i",ni);
		}
		DEBUG("configNodes D");
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	virtual void initalize(){
		nodes.resize(nodeMax);
		for(int ni = 0; ni < nodeMax; ni++){
			nodes[ni] = Node();
		}		
		arpeggiateSpeed = 2;
		weightedOdds = false;
		weightedCycle = false;
		range = Bipolar_1;
		firstProcess = true;
	}

	json_t *dataToJson() override{
		json_t *jobj = json_object();

		json_t *nodesJ = json_array();
		for(int ni = 0; ni < nodeMax; ni++){
			json_array_insert_new(nodesJ, ni, nodes[ni].dataToJson());
		}
		json_object_set_new(jobj, "nodes", nodesJ);

		//Context Menu State
		json_object_set_new(jobj, "range", json_integer(range));
		json_object_set_new(jobj, "arpeggiateSpeed", json_integer(arpeggiateSpeed));
		json_object_set_new(jobj, "weightedOdds", json_bool(weightedOdds));
		json_object_set_new(jobj, "weightedCycle", json_bool(weightedCycle));

		return jobj;
	}

	void dataFromJson(json_t *jobj) override {					
		json_t *nodesJ = json_object_get(jobj,"nodes");
		for(int ni = 0; ni < nodeMax; ni++){
			nodes[ni].dataFromJson(json_array_get(nodesJ,ni));
		}

		//Context Menu State
		range = (CVRange)json_integer_value(json_object_get(jobj, "range"));
		arpeggiateSpeed = json_integer_value(json_object_get(jobj, "arpeggiateSpeed"));
		weightedOdds = json_bool_value(json_object_get(jobj, "weightedOdds"));
		weightedCycle = json_bool_value(json_object_get(jobj, "weightedCycle"));
	}

	void preProcess() {
		if(firstProcess){
			firstProcess = false;
			for(int ni = 0; ni < nodeMax; ni++){
				setModeLight(ni);
			}
		}
	}

	void arpeggiateClock(ProcessContext& pc){
		//Arpeggiate Clock
		pc.arpHighEvent = false;
		pc.arpLowEvent = false;
		if(pc.arpeggiateCounter > 0){
			if(arpeggiateSpeed == 1){
				//Special case for whole note speed
				//In this case we don't use a counter and instead just pass the clock events on
				pc.arpHighEvent = pc.clockHighEvent;
				pc.arpLowEvent = pc.clockLowEvent;
			}else{
				//We are Arpeggiating
				pc.arpeggiateCounter--;
				if(pc.arpeggiateCounter == 0){
					//We hit an arpeggiate clock transition
					if(pc.arpeggiateHigh){
						//Low Transition
						pc.arpLowEvent = true;
						pc.arpeggiateHigh = false;					
					}else{
						//High Transition (or End)
						pc.arpHighEvent = true;
						pc.arpeggiateHigh = true;
					}					
				}
			}
		}
		if(pc.arpHighEvent){
			//High Transition
			pc.arpeggiateLeft --;
			if(pc.arpeggiateLeft > 0){
				resetArpeggiateCounter(pc);
				setActiveNode(pc,pc.arpeggiateNode);
				DEBUG("Arp going High. Left:%i",pc.arpeggiateLeft);
			}else{
				//End arpegiation
				pc.arpHighEvent = false; //Consume arpHighEvent
				DEBUG("End of arpegiation");
				if(pc.activeNode == pc.arpeggiateNode){
					DEBUG("End of arpegiation - setActiveNode(pc,0)");
					//Prevent endless arpeggiating on the same note
					setActiveNode(pc,0);
				}
				cleanUpArpeggiation(pc);
			}
		}
		if(pc.arpLowEvent){
			resetArpeggiateCounter(pc);
			DEBUG("Arp going Low. Left:%i",pc.arpeggiateLeft);
		}
	}

	void processNodeLoop(ProcessContext& pc) {
		//Loop through Nodes
		for(int ni = 0; ni < nodeMax; ni++){
			Node & node = nodes[ni];

			//Mode Trigger
			if(schmittTrigger(node.modeTriggerHigh,inputs[modeTriggerInput + ni].getVoltage())){
				NodeMode newMode = (NodeMode)((getNodeMode(ni) + 1) % NODE_MODE_MAX);
				//Set value directly to avoid the logic that updates manualMode inside ModeParamQuantity.setValue
				APP->engine->setParamValue(this, modeButtonParam + ni, newMode);
				//But then we have to update the light ourself too
				setModeLight(ni);
			}
			
			//Input Detection
			bool inputHighEvent = false; bool inputLowEvent = false;
			for(int ii = 0 ; ii < NODE_IN_MAX; ii ++){	
				schmittTrigger(node.inHigh[ii],inputs[nodeInput + ni * NODE_IN_MAX + ii].getVoltage(),inputHighEvent,inputLowEvent);
			}

			//Note we don't put the arp checks inside arpeggiateCounter > 0 because
			//then we miss the last gate down because the logic above clears it on
			//the same frame it set the arp events
			if(pc.arpHighEvent && ni == pc.activeNode_snapShot){
				pc.arpHighEvent = false; //Consume high event
				inputHighEvent = true;
				node.triggerSource = TS_Arpeggiate;
				DEBUG("High caused by ARP");
			}
			if(pc.arpLowEvent && node.triggerSource == TS_Arpeggiate){
				node.triggerSource = TS_Input; //revert back to default state
				inputLowEvent = true;
				DEBUG("Low caused by ARP");
			}
			if(pc.arpeggiateCounter <= 0){
				if(pc.clockHighEvent && ni == pc.activeNode_snapShot){
					pc.clockHighEvent = false; //Consume high event
					inputHighEvent = true;
					node.triggerSource = TS_Clock;
					DEBUG("High caused by CLOCK | ni:%i | clockHighEvent is now %i",ni,pc.clockHighEvent);
				}
				if(pc.clockLowEvent && node.triggerSource == TS_Clock){
					node.triggerSource = TS_Input; //revert back to default state
					inputLowEvent = true;
					DEBUG("Low caused by CLOCK");
				}
			}

			//Input High Event
			if(inputHighEvent){
				nodeHighEvent(ni,pc);

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

	void nodeHighEvent(int ni, ProcessContext& pc){
		DEBUG("nodeHighEvent ni:%i",ni);
		Node & node = nodes[ni];

		//Arpeggiate -> Cycle Ghosting
		bool doWeightedCycle = weightedCycle;
		NodeMode mode = getNodeMode(ni);
		//When arpeggiating, use cycle logic
		if(node.triggerSource == TS_Arpeggiate){
			mode = Cycle;
			doWeightedCycle = false;
		}
		//Prevent arpeggiation when clock length is 0
		if(mode == Arpeggiate && pc.clockLength <= 0){
			mode = Cycle; 
			doWeightedCycle = false;
		}

		//Mode Logic
		switch(mode){
			case Cycle:{
				setActiveNode(pc,ni);
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
								pc.endOfLine = true;
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
								pc.endOfLine = true;
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
				setActiveNode(pc,ni);
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
					pc.endOfLine = true;
				}
			}break;
			case Arpeggiate:{
				int connectedCount = 1;						
				for(int oi = 0 ; oi < NODE_OUT_MAX; oi ++){
					if(outputs[nodeOutput + ni * NODE_OUT_MAX + oi].isConnected()){
						connectedCount ++;
					}
				}
				if(pc.arpeggiateLeft <= 0){
					pc.arpeggiateNode = ni;
					node.state = 0; //Only reset cur output if this is the root arpeggiate note, otherwise keep it at prev value for more chaos 
				}
				if(arpeggiateSpeed <= 0){
					//Dynamic Speed
					pc.arpeggiateLength = pc.clockLength / (connectedCount * 2);
					pc.arpeggiateLeft += connectedCount;
				}else{
					//Fixed Speed
					pc.arpeggiateLength = pc.clockLength / (arpeggiateSpeed * 2);
					//Round up to the nearest multiple of arpeggiateSpeed so we don't end up with breaks in the beat
					pc.arpeggiateLeft += std::ceil((float)connectedCount / arpeggiateSpeed) * arpeggiateSpeed;
				}
				DEBUG("Arp starting. Left:%i",pc.arpeggiateLeft);

				resetArpeggiateCounter(pc);
				pc.arpeggiateHigh = true;
				node.triggerSource = TS_Arpeggiate;
				setActiveNode(pc,ni);
				setNodeOutput(ni,node.state);
			}break;
		}

	}

	inline void nodeLowEvent(int ni){
		DEBUG("nodeLowEvent ni:%i",ni);
		clearNodeOutputs(ni);
	}

	void setActiveNode(ProcessContext& pc, int node){

		//Update Internal State
		pc.activeNode = node;

		float cv = 0;
		if(pc.activeNode >= 0) cv = params[cvKnobParam + pc.activeNode].getValue();
		cv = mapCVRange(cv,range);
		pc.activeVoltage = cv;

	}

	void setNodeOutput(int ni, int output){
		DEBUG("setNodeOutput ni:%i output:%i",ni,output);
		{
			bool selfOn = output == 0;
			lights[stateLight + ni * NODE_STATE_MAX].setBrightness(selfOn ? 10.f : 0.f);
		}
		int outPort = output - 1;
		for(int oi = 0; oi < NODE_OUT_MAX; /*done inline*/){
			bool isOn = outPort == oi;
			outputs[nodeOutput + ni * NODE_OUT_MAX + oi].setVoltage(isOn ? 10.f : 0.f);
			oi++;//Doing this here since light indexes are +1
			lights[stateLight + ni * NODE_STATE_MAX + oi].setBrightness(isOn ? 1.f : 0.f);
		}
	}

	inline NodeMode getNodeMode(int ni){
		return (NodeMode)(int)params[modeButtonParam + ni].getValue();
	}

	void setModeLight(int ni){
		switch(getNodeMode(ni)){
			case Cycle:
				lights[modeLight + ni * 3 + 0].setBrightness(0.f);
				lights[modeLight + ni * 3 + 1].setBrightness(0.f);
				lights[modeLight + ni * 3 + 2].setBrightness(1.f);
				break;
			case Random:
				lights[modeLight + ni * 3 + 0].setBrightness(180/255.f);
				lights[modeLight + ni * 3 + 1].setBrightness(50/255.f);
				lights[modeLight + ni * 3 + 2].setBrightness(5/255.f);
				break;
			case Arpeggiate:
				lights[modeLight + ni * 3 + 0].setBrightness(180/255.f);
				lights[modeLight + ni * 3 + 1].setBrightness(0/255.f);
				lights[modeLight + ni * 3 + 2].setBrightness(180/255.f);
				break;
		}
	}

	void resetNodesFromTrigger(){		
		for(int ni = 0; ni < nodeMax; ni++){
			nodes[ni].state = NO_STATE;
			DEBUG("node %i manualMode:%i mode:%f",ni,nodes[ni].manualMode,params[modeButtonParam + ni].getValue());
			params[modeButtonParam + ni].setValue((float)(int)nodes[ni].manualMode);
			DEBUG("node %i after mode:%f",ni,params[modeButtonParam + ni].getValue());
			setModeLight(ni);//setValue on param doesn't trigger setValue on ParamQuantity, so this is needed here
			clearNodeOutputs(ni);
			updateNodeLights(ni);
		}
	}

	void clearNodeOutputs(int node){
		for(int oi = 0; oi < NODE_OUT_MAX; oi ++){
			outputs[nodeOutput + node * NODE_OUT_MAX + oi].setVoltage(0.f);
		}
	}

	void updateNodeLights(int node){
		for(int si = 0; si < NODE_STATE_MAX; si ++){
			lights[stateLight + node * NODE_STATE_MAX + si].setBrightness(0.f);
		}
	}

	bool isStateConnected(int nodeIndex, int state){
		if(state == 0) return true;
		int outPort = state - 1;
		return outputs[nodeOutput + nodeIndex * NODE_OUT_MAX + outPort].isConnected();
	}

	bool nothingConnected(int nodeIndex){
		for(int oi = 0; oi < NODE_OUT_MAX; oi ++){
			if(outputs[nodeOutput + nodeIndex * NODE_OUT_MAX + oi].isConnected()){
				return false;
			}
		}
		return true;
	}

	inline void resetArpeggiateCounter(ProcessContext& pc){
		pc.arpeggiateCounter = pc.arpeggiateLength;
		pc.arpeggiateCounter --; //little bit of padding
		if(pc.arpeggiateCounter < 10) pc.arpeggiateCounter = 10;
		DEBUG("resetArpeggiateCounter to %i",pc.arpeggiateCounter);
	}

	void cleanUpArpeggiation(ProcessContext& pc){		
		pc.arpeggiateLeft = 0;
		pc.arpeggiateNode = 0;
		pc.arpeggiateCounter = 0;
		pc.arpeggiateHigh = false;
	}

};

struct GPRootWidget : ModuleWidget {

	//Config
	int modeButtonParam;
	int cvKnobParam;
	int nodeInput;
	int modeTriggerInput;
	int nodeOutput;
	int modeLight;
	int stateLight;
	int activeLight;

	struct WhiteKnob : RoundKnob {
		WhiteKnob(){
			setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/WhiteKnob.svg")));
			bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance,"res/WhiteKnob_bg.svg")));
		}
	};

	void addModule(GPRoot* module, Vec delta, int ni){
		addParam(createParamCentered<ModeSwitch<VCVButton>>(mm2px(delta+Vec(26.15, 25.868)), module, modeButtonParam + ni));
		addParam(createParamCentered<WhiteKnob>(mm2px(delta+Vec(7.544, 36.457)), module, cvKnobParam + ni));		
		addInput(createInputCentered<PJ301MPort>(mm2px(delta+Vec(7.544, 25.868)), module, nodeInput + ni * NODE_IN_MAX + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(delta+Vec(16.847, 25.868)), module, nodeInput + ni * NODE_IN_MAX + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(delta+Vec(35.453, 25.868)), module, modeTriggerInput + ni));	
		addOutput(createOutputCentered<PJ301MPort>(mm2px(delta+Vec(16.847, 36.451)), module, nodeOutput + ni * NODE_OUT_MAX + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(delta+Vec(26.15, 36.451)), module, nodeOutput + ni * NODE_OUT_MAX + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(delta+Vec(35.453, 36.451)), module, nodeOutput + ni * NODE_OUT_MAX + 2));
		addChild(createLightCentered<VCVBezelLight<RedGreenBlueLight>>(mm2px(delta+Vec(26.15, 25.868)), module, modeLight + ni * 3));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(11.055, 40.484)), module, stateLight + ni * NODE_STATE_MAX + 0));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(20.487, 40.484)), module, stateLight + ni * NODE_STATE_MAX + 1));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(29.918, 40.484)), module, stateLight + ni * NODE_STATE_MAX + 2));
		addChild(createLightCentered<SmallLight<BlueLight>>(mm2px(delta+Vec(39.35, 40.484)), module, stateLight + ni * NODE_STATE_MAX + 3));
		{
			auto light = createLight<RectangleLight<BlueLight>>(mm2px(delta+Vec(2.2, 30.364)), module, activeLight + ni);
			light->box.size = mm2px(Vec(38.719, 1.590));
			light->module = module;
			addChild(light);
		}
	}

	void appendBaseContextMenu(GPRoot* module, Menu* menu) {

		addRangeSelectMenu<GPRoot>(module,menu);

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

struct PlusPane : GPRoot {
	enum ParamId {
		ENUMS(MODE_BUTTON_PARAM,NODE_MAX_PLUS),
		ENUMS(CV_KNOB_PARAM,NODE_MAX_PLUS),
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(NODE_IN_INPUT,NODE_MAX_PLUS * NODE_IN_MAX),
		ENUMS(MODE_TRIGGER_INPUT,NODE_MAX_PLUS),
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(NODE_OUT_OUTPUT,NODE_MAX_PLUS * NODE_OUT_MAX),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(MODE_LIGHT_LIGHT,NODE_MAX_PLUS * 3),
		ENUMS(STATE_LIGHT,NODE_MAX_PLUS * NODE_STATE_MAX),
		ENUMS(ACTIVE_LIGHT,NODE_MAX_PLUS),
		LIGHTS_LEN
	};

	//Non Persisted State

	//Persisted State

	PlusPane() {
		nodeMax = NODE_MAX_PLUS;
		modeLight = MODE_LIGHT_LIGHT;
		stateLight = STATE_LIGHT;
		activeLight = ACTIVE_LIGHT;

		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configNodes(MODE_BUTTON_PARAM, CV_KNOB_PARAM, MODE_TRIGGER_INPUT, NODE_IN_INPUT, NODE_OUT_OUTPUT);	

		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}	
};

struct PlusPaneWidget : GPRootWidget {

	PlusPaneWidget(PlusPane* module) {

		modeButtonParam = PlusPane::MODE_BUTTON_PARAM;
		cvKnobParam = PlusPane::CV_KNOB_PARAM;
		nodeInput = PlusPane::NODE_IN_INPUT;
		modeTriggerInput = PlusPane::MODE_TRIGGER_INPUT;
		nodeOutput = PlusPane::NODE_OUT_OUTPUT;
		modeLight = PlusPane::MODE_LIGHT_LIGHT;
		stateLight = PlusPane::STATE_LIGHT;
		activeLight = PlusPane::ACTIVE_LIGHT;

		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PlusPane.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const Vec ORIGIN = Vec(7.544, 25.868);
		const Vec PANE_POS [] = {
			Vec(8.360, 25.868),Vec(49.988, 25.868),
			Vec(8.360, 50.562),Vec(49.988, 50.562),
			Vec(8.360, 75.257),Vec(49.988, 75.257),
			Vec(8.360, 99.951),Vec(49.988, 99.951)
		};

		for(int ni = 0; ni < NODE_MAX_PLUS; ni++){
			Vec delta = PANE_POS[ni] - ORIGIN;
			addModule(module,delta,ni);
		}
	}

	void appendContextMenu(Menu* menu) override {
		auto module = dynamic_cast<PlusPane*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("+Pane"));

		appendBaseContextMenu(module,menu);	
	}
};


Model* modelPlusPane = createModel<PlusPane, PlusPaneWidget>("PlusPane");

struct GlassPane : GPRoot {
	enum ParamId {
		ENUMS(MODE_BUTTON_PARAM,NODE_MAX_PANE),
		ENUMS(CV_KNOB_PARAM,NODE_MAX_PANE),
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		ENUMS(NODE_IN_INPUT,NODE_MAX_PANE * NODE_IN_MAX),
		ENUMS(MODE_TRIGGER_INPUT,NODE_MAX_PANE),
		INPUTS_LEN
	};
	enum OutputId {
		GATE_OUTPUT,
		CV_OUTPUT,
		ENUMS(NODE_OUT_OUTPUT,NODE_MAX_PANE * NODE_OUT_MAX),
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(MODE_LIGHT_LIGHT,NODE_MAX_PANE * 3),
		ENUMS(STATE_LIGHT,NODE_MAX_PANE * NODE_STATE_MAX),
		ENUMS(ACTIVE_LIGHT,NODE_MAX_PANE),
		LIGHTS_LEN
	};

	//Persistant and Non Persistant State
	ProcessContext pc;

	//Non Persisted State
	bool clockHigh;
	bool resetHigh;
	int clockCounter;

	//Persisted State

	GlassPane() {
		nodeMax = NODE_MAX_PANE;
		modeLight = MODE_LIGHT_LIGHT;
		stateLight = STATE_LIGHT;
		activeLight = ACTIVE_LIGHT;

		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configOutput(GATE_OUTPUT, "Gate");
		configOutput(CV_OUTPUT, "CV");

		configNodes(MODE_BUTTON_PARAM, CV_KNOB_PARAM, MODE_TRIGGER_INPUT, NODE_IN_INPUT, NODE_OUT_OUTPUT);		

		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	void initalize() override{
		GPRoot::initalize();
		
		clockHigh = false;
		resetHigh = false;
		clockCounter = 0;

		pc = ProcessContext();
	}

	json_t *dataToJson() override{
		json_t *jobj = GPRoot::dataToJson();

		json_object_set_new(jobj, "activeNode", json_integer(pc.activeNode));		
		json_object_set_new(jobj, "activeVoltage", json_real(pc.activeVoltage));

		return jobj;
	}

	void dataFromJson(json_t *jobj) override {	
		GPRoot::dataFromJson(jobj);

		pc.activeNode = json_integer_value(json_object_get(jobj, "activeNode"));
		pc.activeVoltage = json_real_value(json_object_get(jobj, "activeVoltage"));	
	}

	void process(const ProcessArgs& args) override {

		int prevActiveNote = pc.activeNode;

		GPRoot::preProcess();

		//Clock In
		pc.clockHighEvent = false;
		pc.clockLowEvent = false;
		schmittTrigger(clockHigh,inputs[CLOCK_INPUT].getVoltage(),pc.clockHighEvent, pc.clockLowEvent);

		if(pc.clockHighEvent) DEBUG("clockHighEvent");
		if(pc.clockLowEvent) DEBUG("clockLowEvent");

		countClockLength(clockCounter,pc.clockLength,pc.clockHighEvent);

		GPRoot::arpeggiateClock(pc);

		//Reset In
		if(schmittTrigger(resetHigh,inputs[RESET_INPUT].getVoltage())){
			DEBUG("reset triggering");
			setActiveNode(pc,0);
			cleanUpArpeggiation(pc);
			pc.clockLowEvent = true;
			resetNodesFromTrigger();
		}

		//Reset Handle
		if(pc.endOfLine){
			setActiveNode(pc,0);
			pc.clockHighEvent = true;
			pc.endOfLine = false;
			DEBUG("endOfLine triggering clockHighEvent");
		}

		pc.activeNode_snapShot = pc.activeNode;

		//Main Gate Output
		//Has to come before node loop since it consumes some events
		if(pc.arpHighEvent || (pc.clockHighEvent && pc.arpeggiateCounter <= 0)){
			DEBUG("Main Gate High");
			outputs[GATE_OUTPUT].setVoltage(10.f);
		}
		if(pc.arpLowEvent || (pc.clockLowEvent && pc.arpeggiateCounter <= 0)){
			DEBUG("Main Gate Low");				
			outputs[GATE_OUTPUT].setVoltage(0.f);
		}	

		GPRoot::processNodeLoop(pc);

		//Update Main Gate Output
		if(prevActiveNote != pc.activeNode){
			prevActiveNote = pc.activeNode;

			//Update CV Output
			outputs[CV_OUTPUT].setVoltage(pc.activeVoltage);

			//Update Active Lights
			for(int ni = 0; ni < NODE_MAX_PANE; ni++){
				if(pc.activeNode == ni){
					lights[ACTIVE_LIGHT + ni].setBrightness(1.f);
				}else if(pc.arpeggiateNode == ni && pc.arpeggiateLeft > 0){
					lights[ACTIVE_LIGHT + ni].setBrightness(0.3f);
				}else{
					lights[ACTIVE_LIGHT + ni].setBrightness(0.f);
				}
			}
			//TODO Ripple Lights to Expanders
		}
	}

};


struct GlassPaneWidget : GPRootWidget {

	GlassPaneWidget(GlassPane* module) {
		modeButtonParam = GlassPane::MODE_BUTTON_PARAM;
		cvKnobParam = GlassPane::CV_KNOB_PARAM;
		nodeInput = GlassPane::NODE_IN_INPUT;
		modeTriggerInput = GlassPane::MODE_TRIGGER_INPUT;
		nodeOutput = GlassPane::NODE_OUT_OUTPUT;
		modeLight = GlassPane::MODE_LIGHT_LIGHT;
		stateLight = GlassPane::STATE_LIGHT;
		activeLight = GlassPane::ACTIVE_LIGHT;

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

		const Vec PANE_POS [] = {
			Vec(7.544, 25.868),Vec(49.172, 25.868),Vec(90.8, 25.868),Vec(132.427, 25.868),
			Vec(7.544, 50.562),Vec(49.172, 50.562),Vec(90.8, 50.562),Vec(132.427, 50.562),
			Vec(7.544, 75.257),Vec(49.172, 75.257),Vec(90.8, 75.257),Vec(132.427, 75.257),
			Vec(7.544, 99.951),Vec(49.172, 99.951),Vec(90.8, 99.951),Vec(132.427, 99.951)
		};

		for(int ni = 0; ni < NODE_MAX_PANE; ni++){
			Vec delta = PANE_POS[ni] - PANE_POS[0];			
			addModule(module,delta,ni);
		}
	}

	void appendContextMenu(Menu* menu) override {
		auto module = dynamic_cast<GlassPane*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("GlassPane"));

		appendBaseContextMenu(module,menu);	
	}
};


Model* modelGlassPane = createModel<GlassPane, GlassPaneWidget>("GlassPane");