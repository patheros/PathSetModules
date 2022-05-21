#define NUM_ROWS 7

struct ShiftyExpanderBridge{
	float clockRate = 0;
	int clockDivider = 0;
	float ramp = 0;
	float sample_and_hold = 0;
	float echo[NUM_ROWS] = {};
	float mute[NUM_ROWS] = {};
};

struct ShiftyExpanderBase : Module{
	ShiftyExpanderBridge bridge = {};
};
