#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 60
#define NUM_BUTTON_LEDS 8
#define NUM_SPHERES 19
#define LEDS_PER_SPHERE 3

#define MANUAL_BRIGHTNESS 255
#define AUTO_BRIGHTNESS 130
#define INACTIVE_BRIGHTNESS 70
#define BRIGHTNESS_DELAY 10 

#define NUM_PEARLS_IN_GRADIENT 10
#define NUM_CIRCLES 9

#define PEARL_DELAY 30
#define FIRE_DELAY 30
#define IDLE_TIMEOUT 10000
#define NEW_SPHERE_PERIOD 1000
#define CIRCLE_PERIOD 15000

#define MANUAL_MODE_TRANSITION_MULT 1.5

#define STRIP_PIN A0
#define BUTTON_STRIP_PIN A1
#define BUTTON_PIN A2
#define HOLD_TIME_FOR_RESET 2000

CRGB leds[NUM_LEDS];
CRGB button_leds[NUM_BUTTON_LEDS];

// For testing
CRGB colors[NUM_CIRCLES][3] =	{
					{{255, 0, 0}, {0, 255, 0}, {0, 0, 255}},			//3
					{{230, 255, 51}, {255, 51, 212}, {51, 249, 255}},
					{{255, 0, 0}, {0, 0, 0}, {0, 0, 0}},				//idle
					{{0, 0, 0}, {0, 255, 0}, {0, 0, 0}},				//1
					{{0, 0, 255}, {0, 0, 0}, {0, 0, 0}},			//2
					{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},				//4
					{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},			//5
					{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}},		//6	
					{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}
					};		//7

CRGB fireColors[3] = {{255, 0, 0}, {255, 128, 0}, {255, 255, 0}};


unsigned long previousActivation = 0;
unsigned long previousPress = 0;
unsigned long previousPearlUpdate = 0;
unsigned long previousFireUpdate = 0;
unsigned long previousSphere = 0;
unsigned long previousBrightnessCheck = 0;
unsigned long previousCircle = 0;

bool autoMode = false;
bool reset = false;
int globalBrightness = 0;

int colorBlend[NUM_SPHERES];
uint8_t shift [NUM_SPHERES];
uint8_t sphereBrightness[NUM_SPHERES];
uint8_t transitionBrightness[NUM_SPHERES];
bool transition[NUM_SPHERES];
bool descending[NUM_SPHERES];

uint8_t buttonShift = 0;
int buttonBlend = 0;
int buttonBrightness = 255;
bool buttonDescend = false;
bool lastButtonState = false;

uint8_t fireColorBlend = 0;
uint8_t fireShift = 0;

uint8_t circle = 2;
uint8_t activeSpheres = 0;

void mainAnimation();
void checkButton();
void buttonAnimation();
void fireAnimation();
void autoplay();
void newSphere();
void changeGlobalBrightness();
void changeSphereBrightness(int sphere, int isActive);
void changeTransitionBrightness(int sphere, int index);
void initStates();

void setup() {
	randomSeed(analogRead(A5));
	initStates();
	FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUM_LEDS);
	FastLED.addLeds<WS2811, BUTTON_STRIP_PIN, GRB>(button_leds, NUM_BUTTON_LEDS);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	Serial.begin(9600);
}

void loop() {
	if(!autoMode && millis() - previousActivation > IDLE_TIMEOUT) {
		autoMode = true;
		reset = true;;
	}

	if(autoMode && millis() - previousCircle > CIRCLE_PERIOD) {
		autoplay();
	}

	if(millis() - previousPearlUpdate > PEARL_DELAY){
		mainAnimation();
		buttonAnimation();
		previousPearlUpdate = millis();
	}

	if(millis() - previousFireUpdate > FIRE_DELAY){
		fireAnimation();
		previousFireUpdate = millis();
	}

	if(millis() - previousBrightnessCheck > BRIGHTNESS_DELAY) {
		changeGlobalBrightness();
		previousBrightnessCheck = millis();
	}

	checkButton();

	// for(int i = 0; i < NUM_LEDS; i++) {
	// 	leds[i] = CRGB::White;
	// 	FastLED.show();
	// }
}

void mainAnimation() {
	for(int i = 0; i < NUM_SPHERES; i++) {
		int inc;

		if(reset) inc = 8;
		else inc = autoMode ? 8 : 8 * MANUAL_MODE_TRANSITION_MULT;

		if(i < activeSpheres && !descending[i]) inc *= 2;
        colorBlend[i] += inc;
        if (colorBlend[i] > 255) 
		{
			colorBlend[i] = 0;
			shift[i]++;
		}
	}

	for(int i = 0; i < (NUM_LEDS - LEDS_PER_SPHERE); i += LEDS_PER_SPHERE) {
		int currentSphere = i / LEDS_PER_SPHERE;
		int currentShift = shift[currentSphere];
		int currentBlend = colorBlend[currentSphere];
		int isActive = 0;

        if(currentSphere < activeSpheres && !descending[currentSphere]) isActive = 1;
		changeSphereBrightness(currentSphere, isActive);

        for(int j = 0; j < 3; j++) {
            CRGB color1 = colors[circle + isActive][(currentShift + j) % 3];
            CRGB color2 = colors[circle + isActive][(currentShift+ j + 1) % 3];
            leds[i + j] = blend(color1, color2, currentBlend);
			leds[i + j] = blend(CRGB::Black, leds[i + j], sphereBrightness[currentSphere]);
        }
		changeTransitionBrightness(currentSphere, i);
	}
	FastLED.show();
}

void fireAnimation() {
	fireColorBlend += 8;
	if(fireColorBlend == 0) fireShift++;
	for(int j = 0; j < 3; j++) {
		CRGB color1 = fireColors[(fireShift + j) % 3];
		CRGB color2 = fireColors[(fireShift + j + 1) % 3];
		leds[NUM_LEDS - LEDS_PER_SPHERE + j] = blend(color1, color2, fireColorBlend);
	}
}

void changeSphereBrightness(int sphere, int isActive) {
	if(isActive == 1) {
		int position = activeSpheres - sphere + 1;
		int brightGradient = 255 - ((255 - (INACTIVE_BRIGHTNESS + 30)) / NUM_PEARLS_IN_GRADIENT) * position;

		if(position < NUM_PEARLS_IN_GRADIENT && sphereBrightness[sphere] < brightGradient) sphereBrightness[sphere] += 5;
		else if(position < NUM_PEARLS_IN_GRADIENT && sphereBrightness[sphere] > brightGradient) sphereBrightness[sphere] -= 5;
		else if (position >= NUM_PEARLS_IN_GRADIENT && sphereBrightness[sphere] > INACTIVE_BRIGHTNESS + 30) sphereBrightness[sphere] -= 5;
	}
	else if(isActive == 0 && sphereBrightness[sphere] > INACTIVE_BRIGHTNESS) sphereBrightness[sphere]--;
}

void newSphere() {
	activeSpheres++;

	if(activeSpheres == NUM_SPHERES + 1) {
		if(autoMode) {
			reset = true;
			previousCircle = millis();
		}
		else if(circle + 2 == NUM_CIRCLES) reset = true;
		else {
			activeSpheres = 0;
			circle++;
			buttonDescend = true;
		}
	}
	else {
		transitionBrightness[activeSpheres - 1] = 250;
		transition[activeSpheres - 1] = true;
		descending[activeSpheres - 1] = true;
	}
}

void autoplay() {
	if(millis() - previousSphere > NEW_SPHERE_PERIOD && !reset) {
		newSphere();
		previousSphere = millis();
	}
}

void changeGlobalBrightness() {
	if(reset) {
		if(globalBrightness > 0) globalBrightness--;
		if(globalBrightness == 0) {
			reset = false;
			activeSpheres = 0;
			if(autoMode) circle = 0;
			else circle = 2;
		}
	}
	else {
		if(autoMode && globalBrightness != AUTO_BRIGHTNESS) {
			if (globalBrightness > AUTO_BRIGHTNESS) globalBrightness--;
			else if (globalBrightness < AUTO_BRIGHTNESS) globalBrightness++;
		}
		if(!autoMode && globalBrightness != MANUAL_BRIGHTNESS) {
			if (globalBrightness > MANUAL_BRIGHTNESS) globalBrightness--;
			else if (globalBrightness < MANUAL_BRIGHTNESS) globalBrightness++;
		}
	}
	FastLED.setBrightness(globalBrightness);
}

void changeTransitionBrightness(int sphere, int index) {
	if(transition[sphere]) {
		if(descending[sphere]) {
			transitionBrightness[sphere] -= 25;
			if (transitionBrightness[sphere] == 0) descending[sphere] = false;
		}
		else {
			transitionBrightness[sphere] += 5;
			if (transitionBrightness[sphere] == 250) transition[sphere] = false;
		}

		for(int j = 0; j < 3; j++) {
			leds[index + j] = blend(CRGB::Black, leds[index + j], transitionBrightness[sphere]);
		}
	}
}

void checkButton() {
	bool buttonState = !digitalRead(BUTTON_PIN);
	if(millis() - previousActivation > 250 && lastButtonState && !buttonState){
		if(autoMode) {
			autoMode = false;
			reset = true;
		}
		else if(!reset) newSphere();
		previousActivation = millis();
	}
	if(!buttonState && !lastButtonState) previousPress = millis();
	if(millis() - previousPress > HOLD_TIME_FOR_RESET && !reset) reset = true;

	lastButtonState = buttonState;
}

void buttonAnimation() {
	// buttonBlend += 16 * MANUAL_MODE_TRANSITION_MULT;
	if(reset) buttonBlend += 8;
	else buttonBlend += autoMode ? 8 : 8 * MANUAL_MODE_TRANSITION_MULT;
	if (buttonBlend > 255) {
		buttonBlend = 0;
		buttonShift++;
	}

	if(buttonDescend) {
		buttonBrightness -= 5;
		if(buttonBrightness == 0) buttonDescend = false;
	}
	else if(buttonBrightness < 255) buttonBrightness += 5;

	int next = buttonDescend ? 0 : 1;

	for(int i = 0; i < 3; i++) {
		CRGB color1 = colors[circle + next][(buttonShift + i) % 3];
		CRGB color2 = colors[circle + next][(buttonShift+ i + 1) % 3];
		button_leds[i * 3] = blend(color1, color2, buttonBlend);
		button_leds[i * 3 + 1] = button_leds[i * 3];
		if(i != 2) button_leds[i * 3 + 2] = button_leds[i * 3];
	}

	for(int i = 0; i < NUM_BUTTON_LEDS; i++) {
		button_leds[i] = blend(CRGB::Black, button_leds[i], buttonBrightness);
	}
}

void initStates() {
	for(int i = 0; i < NUM_SPHERES; i++) {
		colorBlend[i] = random(0, 32) * 8;
		shift[i] = random(0, 3);
		sphereBrightness[i] = INACTIVE_BRIGHTNESS;
		transitionBrightness[i] = 0;
		transition[i] = true;
		descending[i] = false;
	}
}
