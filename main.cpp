#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 111
#define NUM_SPHERES 36
#define LEDS_PER_SPHERE 3
#define MANUAL_BRIGHTNESS 200					// яркость в режиме с кнопкой (0-255)
#define AUTO_BRIGHTNESS 255						// яркость в автоматическом режиме (0-255)
#define INACTIVE_BRIGHTNESS_DIFFERENCE 200	 	// разница между яркостью активных и неактивных жемчужин (0 - разницы нет, 255 - неактивные не горят)
#define PEARL_DELAY 20
#define FIRE_DELAY 30
#define IDLE_TIMEOUT 10000						// время в миллисекундах, после которого включается автоматический режим
#define NEW_SPHERE_PERIOD 6000					// вермя в миллисекундах, после которого загорается новая активная сфера
#define STRIP_PIN A0
#define BUTTON_PIN A1

CRGB leds[NUM_LEDS];

// Цвета для теста

// CRGB colors[8][3] =	{{{255, 0, 0}, {0, 0, 0}, {0, 0, 0}},				//idle
// 					{{0, 0, 0}, {0, 255, 0}, {0, 0, 0}},				//1
// 					{{0, 0, 255}, {0, 0, 0}, {0, 0, 0}},			//2
// 					{{102, 204, 102}, {204, 255, 51}, {255, 225, 11}},			//3
// 					{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},				//4
// 					{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},			//5
// 					{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}},		//6	
// 					{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}};		//7

// Цвета Насти:

CRGB colors[8][3]= {{{0, 51, 204}, {51, 0, 153}, {153, 51, 204}},				//idle
							{{0, 51, 204}, {153, 51, 204}, {51, 204, 204}},				//1
							{{153, 51, 204}, {51, 204, 204}, {102, 204, 102}},			//2
							{{102, 204, 102}, {204, 255, 51}, {255, 225, 11}},			//3
							{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},				//4
							{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},			//5
							{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}},		//6	
							{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}};		//7

CRGB fireColors[3] = {{255, 0, 0}, {255, 128, 0}, {255, 255, 0}};


unsigned long previousPress = 0;
unsigned long previousPearlUpdate = 0;
unsigned long previousFireUpdate = 0;
unsigned long previousSphere = 0;

bool autoMode = false;

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
void changeBrightness(int currentBrightness, int finalBrightness);
void smoothBrightness();

void setup() {
	FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUM_LEDS);
	FastLED.setBrightness(MANUAL_BRIGHTNESS);
	startup();
}

void loop() {
	if(!autoMode && millis() - previousPress > IDLE_TIMEOUT) {
		autoMode = true;
		changeBrightness(MANUAL_BRIGHTNESS, AUTO_BRIGHTNESS);
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

	checkButton();
}

void mainAnimation() {
	colorBlend += 5;
	for(int i = 0; i < (NUM_LEDS - LEDS_PER_SPHERE); i += LEDS_PER_SPHERE) {
		int j = 0;
		if(colorBlendActive < 255 && (activeSpheres-1) * LEDS_PER_SPHERE == i) {
			leds[i] = blend(currentColor1, colors[circle+1][0], colorBlendActive);
			leds[i+1] = blend(currentColor2, colors[circle+1][1], colorBlendActive);
			leds[i+2] = blend(currentColor3, colors[circle+1][2], colorBlendActive);
			if(colorBlendActive < 252) colorBlendActive += 6;
			else if(shift % 3 == 0 && colorBlend == 5) colorBlendActive += 3;
		} 
		else {
		if(i < activeSpheres * LEDS_PER_SPHERE) j = 1;
		leds[i] = blend(colors[circle+j][shift%3], colors[circle+j][(shift+1)%3], colorBlend);
		leds[i] = blend(leds[i], CRGB::Black, j == 1 ? 0 : INACTIVE_BRIGHTNESS_DIFFERENCE);
		leds[i+1] = blend(colors[circle+j][(shift+1)%3], colors[circle+j][(shift+2)%3], colorBlend);
		leds[i+1] = blend(leds[i+1], CRGB::Black, j == 1 ? 0 : INACTIVE_BRIGHTNESS_DIFFERENCE);
		leds[i+2] = blend(colors[circle+j][(shift+2)%3], colors[circle+j][shift%3], colorBlend);
		leds[i+2] = blend(leds[i+2], CRGB::Black, j == 1 ? 0 : INACTIVE_BRIGHTNESS_DIFFERENCE);
		}
	}
	FastLED.show();
	if(colorBlend == 255) {
		shift++;
		colorBlend = 0;
	}
}

void checkButton() {
	if(millis() - previousPress > 250 && digitalRead(BUTTON_PIN)){
		if(autoMode) {
			autoMode = false;
			changeBrightness(AUTO_BRIGHTNESS, MANUAL_BRIGHTNESS);
			reset();
		}
		else newSphere();
		previousPress = millis();
	}
}

void fireAnimation() {
	fireColorBlend += 5;
	leds[NUM_LEDS-LEDS_PER_SPHERE] = blend(fireColors[fireShift%3], fireColors[(fireShift+1)%3], fireColorBlend);
	leds[NUM_LEDS-LEDS_PER_SPHERE+1] = blend(fireColors[(fireShift+1)%3], fireColors[(fireShift+2)%3], fireColorBlend);
	leds[NUM_LEDS-LEDS_PER_SPHERE+2] = blend(fireColors[(fireShift+2)%3], fireColors[fireShift%3], fireColorBlend);
	if(fireColorBlend == 255) {
		fireShift++;
		fireColorBlend = 0;
	}
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
			leds[j] = blend(leds[j], CRGB::Black, INACTIVE_BRIGHTNESS_DIFFERENCE);
			leds[j+1] = blend(CRGB::Black, colors[circle][1], i);
			leds[j+1] = blend(leds[j+1], CRGB::Black, INACTIVE_BRIGHTNESS_DIFFERENCE);
			leds[j+2] = blend(CRGB::Black, colors[circle][2], i);
			leds[j+2] = blend(leds[j+2], CRGB::Black, INACTIVE_BRIGHTNESS_DIFFERENCE);
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

void changeBrightness(int currentBrightness, int finalBrightness) {
	if(currentBrightness < finalBrightness) {
		while(currentBrightness != finalBrightness) {
			currentBrightness++;
			FastLED.setBrightness(currentBrightness);
			FastLED.show();
			delay(10);
		}
	}
	else {
		while(currentBrightness != finalBrightness) {
			currentBrightness--;
			FastLED.setBrightness(currentBrightness);
			FastLED.show();
			delay(10);
		}
	}
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