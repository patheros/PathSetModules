#define NUM_ROWS 7

struct ShiftyExpanderBridge{
	float clockRate;
	int clockDivider;
	float ramp;
	float sample_and_hold;
	float echo[NUM_ROWS];
	float mute[NUM_ROWS];
};

struct ShiftyExpanderBase : Module{
	ShiftyExpanderBridge bridge;
};
