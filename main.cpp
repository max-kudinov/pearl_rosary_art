#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 60
#define NUM_SPHERES 19
#define LEDS_PER_SPHERE 3
#define MANUAL_BRIGHTNESS 50
#define AUTO_BRIGHTNESS 255
#define BRIGHTNESS_DELAY 10 
#define INACTIVE_BRIGHTNESS_DIFFERENCE 200
#define PEARL_DELAY 20
#define FIRE_DELAY 30
// #define IDLE_TIMEOUT 600000
#define IDLE_TIMEOUT 10000
// #define NEW_SPHERE_PERIOD 25 * 1000
#define NEW_SPHERE_PERIOD 6000
#define STRIP_PIN A0
#define BUTTON_PIN A1

CRGB leds[NUM_LEDS];

// For testing

CRGB colors[8][3] =	{{{255, 0, 0}, {0, 0, 0}, {0, 0, 0}},				//idle
					{{0, 0, 0}, {0, 255, 0}, {0, 0, 0}},				//1
					{{0, 0, 255}, {0, 0, 0}, {0, 0, 0}},			//2
					{{102, 204, 102}, {204, 255, 51}, {255, 225, 11}},			//3
					{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},				//4
					{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},			//5
					{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}},		//6	
					{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}};		//7

// CRGB colors[8][3] =	{{{255, 0, 0}, {255, 0, 0}, {255, 0, 0}},				//idle
// 					{{0, 255, 0}, {0, 255, 0}, {0, 255, 0}},				//1
// 					{{0, 0, 255}, {0, 0, 0}, {0, 0, 0}},			//2
// 					{{102, 204, 102}, {204, 255, 51}, {255, 225, 11}},			//3
// 					{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},				//4
// 					{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},			//5
// 					{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}},		//6	
// 					{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}};		//7

// CRGB colors[8][3]= {{{0, 51, 204}, {51, 0, 153}, {153, 51, 204}},				//idle
// 							{{0, 51, 204}, {153, 51, 204}, {51, 204, 204}},				//1
// 							{{153, 51, 204}, {51, 204, 204}, {102, 204, 102}},			//2
// 							{{102, 204, 102}, {204, 255, 51}, {255, 225, 11}},			//3
// 							{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},				//4
// 							{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},			//5
// 							{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}},		//6	
// 							{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}};		//7

CRGB fireColors[3] = {{255, 0, 0}, {255, 128, 0}, {255, 255, 0}};


unsigned long previousPress = 0;
unsigned long previousPearlUpdate = 0;
unsigned long previousFireUpdate = 0;
unsigned long previousSphere = 0;
unsigned long previousBrightnessCheck = 0;

bool autoMode = false;
int globalBrightness = MANUAL_BRIGHTNESS;

CRGB currentColor1;
CRGB currentColor2;
CRGB currentColor3;

uint8_t colorBlend = 0;
uint8_t colorBlendActive = 0;
uint8_t shift = 0;
uint8_t fireColorBlend = 0;
uint8_t fireShift = 0;

uint8_t circle = 0;
uint8_t activeSpheres = 0;

void mainAnimation();
void checkButton();
void finalAnimation();
void fireAnimation();
void startup();
void autoplay();
void reset();
void newSphere();
void blackout();
void changeBrightness();
void smoothBrightness();
void setBrightness(int index, int isActive);

void setup() {
	FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUM_LEDS);
	FastLED.setBrightness(MANUAL_BRIGHTNESS);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	startup();
	Serial.begin(9600);
}

void loop() {
	if(!autoMode && millis() - previousPress > IDLE_TIMEOUT) {
		autoMode = true;
		reset();
	}

	if(autoMode) {
		autoplay();
	}

	if(millis() - previousPearlUpdate > PEARL_DELAY){
		mainAnimation();
		previousPearlUpdate = millis();
	}

	if(millis() - previousFireUpdate > FIRE_DELAY){
		fireAnimation();
		previousFireUpdate = millis();
	}

	if(millis() - previousBrightnessCheck > BRIGHTNESS_DELAY) {
		changeBrightness();
		previousBrightnessCheck = millis();
	}

	checkButton();

	// for(int i = 0; i < NUM_LEDS; i++) {
	// 	leds[i] = CRGB::White;
	// 	FastLED.show();
	// }
}

void mainAnimation() {
	colorBlend += 8;
	if(colorBlend == 0) shift++;
	for(int i = 0; i < (NUM_LEDS - LEDS_PER_SPHERE); i += LEDS_PER_SPHERE) {
		int isActive = 0;
		if(colorBlendActive < 255 && (activeSpheres-1) * LEDS_PER_SPHERE == i) {
			for(int j = 0; j < 3; j++) {
				leds[i + j] = blend(currentColor1, colors[circle+1][j], colorBlendActive);
			}
			if(colorBlendActive < 252) colorBlendActive += 6;
			else if(shift % 3 == 0 && colorBlend == 0) colorBlendActive += 3;
		} 
		else {
			if(i < activeSpheres * LEDS_PER_SPHERE) isActive = 1;
			for(int j = 0; j < 3; j++) {
				CRGB color1 = colors[circle + isActive][(shift + j) % 3];
				CRGB color2 = colors[circle + isActive][(shift+ j + 1) % 3];
				leds[i + j] = blend(color1, color2, colorBlend);
				setBrightness(i + j, isActive);
			}
		}
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

void setBrightness(int index, int isActive) {
	if(isActive == 1) leds[index] = blend(leds[index], CRGB::Black, 0);
	else leds[index] = blend(leds[index], CRGB::Black, INACTIVE_BRIGHTNESS_DIFFERENCE);
}

void finalAnimation() {
	for(int i = 0; i < NUM_LEDS; i++) {
		leds[i] = CRGB::White;
		FastLED.show();
		delay(20);
	}
	blackout();
	startup();
}

void startup() {
	fireAnimation();
	for(int i = 0; i < 255; i++) {
		for(int j = 0; j < NUM_LEDS - LEDS_PER_SPHERE; j += LEDS_PER_SPHERE) {
			leds[j] = blend(CRGB::Black, colors[circle][0], i);
			leds[j+1] = blend(CRGB::Black, colors[circle][1], i);
			leds[j+2] = blend(CRGB::Black, colors[circle][2], i);
			setBrightness(j, 0);
			setBrightness(j + 1, 0);
			setBrightness(j + 2, 0);
		}
		FastLED.show();
		delay(5);
	}
}

void newSphere() {
	activeSpheres++;

	currentColor1 = leds[(activeSpheres-1) * LEDS_PER_SPHERE];
	currentColor2 = leds[(activeSpheres-1) * LEDS_PER_SPHERE+1];
	currentColor3 = leds[(activeSpheres-1) * LEDS_PER_SPHERE+2];

	colorBlendActive = 0;

	if(activeSpheres == NUM_SPHERES + 1) {
		activeSpheres = 1;
		circle++;
		currentColor1 = leds[0];
		currentColor2 = leds[1];
		currentColor3 = leds[2];
		smoothBrightness();
		if(circle == 8) reset();
	}
}

void autoplay() {
	if(millis() - previousSphere > NEW_SPHERE_PERIOD) {
		newSphere();
		previousSphere = millis();
	}
}

void reset() {
	shift = 0;
	colorBlend = 0;
	circle = 0;
	activeSpheres = 0;
	finalAnimation();
}

void blackout() {
	for(int i = 0; i < NUM_LEDS; i++) {
		leds[i] = CRGB::Black;
		FastLED.show();
		delay(20);
	}
}

void changeBrightness() {
	if(autoMode && globalBrightness != AUTO_BRIGHTNESS) {
		if (globalBrightness > AUTO_BRIGHTNESS) globalBrightness--;
		else if (globalBrightness < AUTO_BRIGHTNESS) globalBrightness++;
	}
	if(!autoMode && globalBrightness != MANUAL_BRIGHTNESS) {
		if (globalBrightness > MANUAL_BRIGHTNESS) globalBrightness--;
		else if (globalBrightness < MANUAL_BRIGHTNESS) globalBrightness++;
	}
	FastLED.setBrightness(globalBrightness);
}

void smoothBrightness() {
	for(int i = 0; i < INACTIVE_BRIGHTNESS_DIFFERENCE; i++) {
		for(int k = 0; k < NUM_LEDS - LEDS_PER_SPHERE; k += 3) {
			leds[k] = blend(currentColor1, CRGB::Black, i);
			leds[k+1] = blend(currentColor2, CRGB::Black, i);
			leds[k+2] = blend(currentColor3, CRGB::Black, i);
		}
		FastLED.show();
		delay(5);
	}
}

void checkButton() {
	if(millis() - previousPress > 250 && !digitalRead(BUTTON_PIN)){
		if(autoMode) {
			autoMode = false;
			reset();
		}
		else newSphere();
		previousPress = millis();
	}
}