#include <Arduino.h>
#include <FastLED.h>

//---------------------Настройки---------------------
// кол-во адресных светодиодов
#define NUM_LEDS 6
// кол-во адресных светодиодов в кнопке (не менять, а то все наебнется)
#define NUM_BUTTON_LEDS 8
// кол-во сфер (без гуру и кнопки)
#define NUM_SPHERES 6
// кол-во адресных светодиодов на сферу
#define LEDS_PER_SPHERE 8

// глобальная яркость в режиме с кнопкой
#define MANUAL_BRIGHTNESS 100
// глобальная яркость в авторежиме
#define AUTO_BRIGHTNESS 130
// относительная яркость неактивных сфер
#define INACTIVE_BRIGHTNESS 70
// скорость смены глобальной яркости
#define BRIGHTNESS_DELAY 10

// скорость смены кнопки
#define BUTTON_FADE_SPEED 5
// скорость угасания сферы при переходе с неактивной в активную
#define TRANSITION_FADE_OUT_SPEED 25
// скорость появления сферы при переходе с неактивной в активную
#define TRANSITION_FADE_IN_SPEED 5
// скорость глобального угасания при ресете
#define RESET_FADE_OUT_SPEED 1
// скорость угасания активных сфер при переходе на новый круг
#define INACTIVE_FADE_OUT_SPEED 5

// количество сфер, которые участвуют в градиенте яркости активных сфер
#define NUM_PEARLS_IN_GRADIENT 10
// количество цветовых палитр (2 на авторежим и 7 на кнопку, фактически 1 круг
// авто и 6 кнопка)
#define NUM_CIRCLES 9

// задержка (скорость) основной анимации
#define PEARL_DELAY 30
// задержка (скорость) гуру бусины (огня)
#define FIRE_DELAY 30
// время в миллисекундах до перехода в авторежим
#define IDLE_TIMEOUT 5000
// период в миллисекундах между новыми сферами в авторежиме
#define NEW_SPHERE_PERIOD 1000
// период между кругами в авторежиме
#define CIRCLE_PERIOD 15000

// начальное значение скорости перелива
#define BASE_TRANSITION_VALUE 8
// коэффициент умножения скорости в режиме с кнопкой относительно авто
#define MANUAL_MODE_TRANSITION_MULT 1.5

// время в миллисекундах удержания кнопки для ресета
#define HOLD_TIME_FOR_RESET 5000

CRGB mainColors[][3] = {
	{{255, 0, 0}, {0, 255, 0}, {0, 0, 255}}, // неактивная палитра авторежим
	{{230, 255, 51},
	 {255, 51, 212},
	 {51, 249, 255}}, // активная палитра авторежим
	{{255, 0, 0}, {0, 0, 0}, {0, 0, 0}},				 // 1
	{{0, 0, 0}, {0, 255, 0}, {0, 0, 0}},				 // 2
	{{0, 0, 255}, {0, 0, 0}, {0, 0, 0}},				 // 3
	{{255, 225, 11}, {255, 153, 0}, {255, 51, 51}},		 // 4
	{{255, 153, 0}, {255, 51, 51}, {255, 102, 204}},	 // 5
	{{255, 102, 204}, {153, 255, 255}, {255, 255, 217}}, // 6
	{{255, 255, 11}, {153, 255, 255}, {255, 255, 217}}}; // 7

CRGB fireColors[][3] = {{{255, 0, 0},
						 {255, 128, 0},
						 {255, 255, 0}}}; // цвета для огня (гуру бусина)
//---------------------------------------------------

#define STRIP_PIN A0
#define BUTTON_STRIP_PIN A1
#define BUTTON_PIN A2

CRGB leds[NUM_LEDS];
CRGB button_leds[NUM_BUTTON_LEDS];

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
int shift[NUM_SPHERES];
int sphereBrightness[NUM_SPHERES];
int transitionBrightness[NUM_SPHERES];

bool transition[NUM_SPHERES];
bool descending[NUM_SPHERES];

int buttonShift = 0;
int buttonBlend = 0;
int buttonBrightness = 255;

bool buttonDescend = false;
bool lastButtonState = false;

int fireColorBlend = 0;
int fireShift = 0;

int circle = 2;
int activeSpheres = 0;

void mainAnimation();
void checkButton();
void buttonAnimation();
void fireAnimation();
void autoplay();
void newSphere();
void changeGlobalBrightness();
void changeSphereBrightness(int sphere, int isActive);
void changeTransitionBrightness(int sphere);
void initStates();
void updateSphereParameters(int sphere, int &colorBlend, int &shift, bool main);
void updateSphere(int sphere, CRGB leds[], CRGB colorPalette[][3], int isActive,
				  int crcl, int shift, int clrBlend);
int isActive(int sphere);

void setup() {
	randomSeed(analogRead(A5));
	initStates();
	FastLED.addLeds<WS2811, STRIP_PIN, GRB>(leds, NUM_LEDS);
	FastLED.addLeds<WS2811, BUTTON_STRIP_PIN, GRB>(button_leds,
												   NUM_BUTTON_LEDS);
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	Serial.begin(9600);
}

void loop() {
	if (!autoMode && millis() - previousActivation > IDLE_TIMEOUT) {
		autoMode = true;
		reset = true;
	}

	if (autoMode && millis() - previousCircle > CIRCLE_PERIOD) {
		autoplay();
	}

	if (millis() - previousPearlUpdate > PEARL_DELAY) {
		mainAnimation();
		buttonAnimation();
		previousPearlUpdate = millis();
	}

	if (millis() - previousFireUpdate > FIRE_DELAY) {
		fireAnimation();
		previousFireUpdate = millis();
	}

	if (millis() - previousBrightnessCheck > BRIGHTNESS_DELAY) {
		changeGlobalBrightness();
		previousBrightnessCheck = millis();
	}

	checkButton();
	FastLED.show();

	// for(int i = 0; i < NUM_LEDS; i++) {
	// 	leds[i] = CRGB::White;
	// 	FastLED.show();
	// }
}

void mainAnimation() {
	for (int sphere = 0; sphere < NUM_SPHERES; sphere++) {
		updateSphereParameters(sphere, colorBlend[sphere], shift[sphere], true);
		updateSphere(sphere, leds, mainColors, isActive(sphere), circle,
					 shift[sphere], colorBlend[sphere]);
		changeSphereBrightness(sphere, isActive(sphere));
		changeTransitionBrightness(sphere);
	}
}

void fireAnimation() {
	updateSphereParameters(NUM_SPHERES, fireColorBlend, fireShift, false);
	updateSphere(NUM_SPHERES, leds, fireColors, 0, 0, fireShift,
				 fireColorBlend);
}

void changeSphereBrightness(int sphere, int isActive) {
	if (isActive == 1) {
		int position = activeSpheres - sphere + 1;
		int brightGradient = 255 - ((255 - (INACTIVE_BRIGHTNESS + 30)) /
									NUM_PEARLS_IN_GRADIENT) *
									   position;

		if (position < NUM_PEARLS_IN_GRADIENT &&
			sphereBrightness[sphere] < brightGradient)
			sphereBrightness[sphere] += 1;
		else if (position < NUM_PEARLS_IN_GRADIENT &&
				 sphereBrightness[sphere] > brightGradient)
			sphereBrightness[sphere] -= 1;
		else if (position >= NUM_PEARLS_IN_GRADIENT &&
				 sphereBrightness[sphere] > INACTIVE_BRIGHTNESS + 30)
			sphereBrightness[sphere] -= 1;
	} else if (isActive == 0 && sphereBrightness[sphere] > INACTIVE_BRIGHTNESS)
		sphereBrightness[sphere] -= INACTIVE_FADE_OUT_SPEED;

	int sphereIndex = sphere * LEDS_PER_SPHERE;
	for (int i = 0; i < LEDS_PER_SPHERE; i++) {
		leds[sphereIndex + i] =
			blend(CRGB::Black, leds[sphereIndex + i], sphereBrightness[sphere]);
	}
}

void newSphere() {
	activeSpheres++;

	if (activeSpheres == NUM_SPHERES + 1) {
		if (autoMode) {
			reset = true;
			previousCircle = millis();
		} else if (circle + 2 == NUM_CIRCLES)
			reset = true;
		else {
			activeSpheres = 0;
			circle++;
			buttonDescend = true;
		}
	} else {
		transitionBrightness[activeSpheres - 1] = 255;
		transition[activeSpheres - 1] = true;
		descending[activeSpheres - 1] = true;
	}
}

void autoplay() {
	if (millis() - previousSphere > NEW_SPHERE_PERIOD && !reset) {
		newSphere();
		previousSphere = millis();
	}
}

void changeGlobalBrightness() {
	if (reset) {
		if (globalBrightness > 0)
			globalBrightness -= RESET_FADE_OUT_SPEED;
		if (globalBrightness <= 0) {
			if (globalBrightness < 0)
				globalBrightness = 0;
			reset = false;
			activeSpheres = 0;
			if (autoMode)
				circle = 0;
			else
				circle = 2;
		}
	} else {
		if (autoMode && globalBrightness != AUTO_BRIGHTNESS) {
			if (globalBrightness > AUTO_BRIGHTNESS)
				globalBrightness--;
			else if (globalBrightness < AUTO_BRIGHTNESS)
				globalBrightness++;
		}
		if (!autoMode && globalBrightness != MANUAL_BRIGHTNESS) {
			if (globalBrightness > MANUAL_BRIGHTNESS)
				globalBrightness--;
			else if (globalBrightness < MANUAL_BRIGHTNESS)
				globalBrightness++;
		}
	}
	FastLED.setBrightness(globalBrightness);
}

void changeTransitionBrightness(int sphere) {
	int index = sphere * LEDS_PER_SPHERE;
	if (transition[sphere]) {
		if (descending[sphere]) {
			transitionBrightness[sphere] -= TRANSITION_FADE_OUT_SPEED;
			if (transitionBrightness[sphere] <= 0) {
				descending[sphere] = false;
				if (transitionBrightness[sphere] < 0)
					transitionBrightness[sphere] = 0;
			}
		} else {
			transitionBrightness[sphere] += TRANSITION_FADE_IN_SPEED;
			if (transitionBrightness[sphere] >= 255) {
				transition[sphere] = false;
				if (transitionBrightness[sphere] > 255)
					transitionBrightness[sphere] = 255;
			}
		}

		for (int i = 0; i < LEDS_PER_SPHERE; i++) {
			leds[index + i] = blend(CRGB::Black, leds[index + i],
									transitionBrightness[sphere]);
		}
	}
}

void checkButton() {
	bool buttonState = !digitalRead(BUTTON_PIN);
	if (millis() - previousActivation > 250 && lastButtonState &&
		!buttonState) {
		if (autoMode) {
			autoMode = false;
			reset = true;
		} else if (!reset)
			newSphere();
		previousActivation = millis();
	}
	if (!buttonState && !lastButtonState)
		previousPress = millis();
	if (millis() - previousPress > HOLD_TIME_FOR_RESET && !reset)
		reset = true;

	lastButtonState = buttonState;
}

void buttonAnimation() {
	int next = buttonDescend ? 0 : 1;

	updateSphereParameters(-1, buttonBlend, buttonShift, false);
	updateSphere(0, button_leds, mainColors, next, circle, buttonShift,
				 buttonBlend);

	if (buttonDescend) {
		buttonBrightness -= BUTTON_FADE_SPEED;
		if (buttonBrightness <= 0)
			buttonDescend = false;
	} else if (buttonBrightness < 255) {
		buttonBrightness += BUTTON_FADE_SPEED;
		if (buttonBrightness > 255)
			buttonBrightness = 255;
	}

	for (int i = 0; i < NUM_BUTTON_LEDS; i++) {
		button_leds[i] = blend(CRGB::Black, button_leds[i], buttonBrightness);
	}
}

void initStates() {
	for (int i = 0; i < NUM_SPHERES; i++) {
		colorBlend[i] = random(0, 32) * 8;
		shift[i] = random(0, 3);
		sphereBrightness[i] = INACTIVE_BRIGHTNESS;
		transition[i] = false;
		descending[i] = false;
	}
}

void updateSphereParameters(int sphere, int &colorBlend, int &shift,
							bool main) {
	int inc;

	if (reset)
		inc = BASE_TRANSITION_VALUE;
	else
		inc = autoMode ? BASE_TRANSITION_VALUE
					   : BASE_TRANSITION_VALUE * MANUAL_MODE_TRANSITION_MULT;

	if (main && sphere < activeSpheres && !descending[sphere])
		inc *= 2;
	colorBlend += inc;
	if (colorBlend > 255) {
		colorBlend = 0;
		shift++;
		if (shift == 3)
			shift = 0;
	}
}

void updateSphere(int sphere, CRGB leds[], CRGB colorPalete[][3], int isActive,
				  int crcl, int shift, int clrBlend) {
	int sphereIndex = sphere * LEDS_PER_SPHERE;
	for (int i = 0; i < 3; i++) {
		CRGB color1 = colorPalete[crcl + isActive][(shift + i) % 3];
		CRGB color2 = colorPalete[crcl + isActive][(shift + i + 1) % 3];

		leds[sphereIndex + (i * 3)] = blend(color1, color2, clrBlend);
		leds[sphereIndex + (i * 3) + 1] = leds[sphereIndex + (i * 3)];
		if (i != 2)
			leds[sphereIndex + (i * 3) + 2] = leds[sphereIndex + (i * 3)];
	}
}

int isActive(int sphere) {
	int isActive = 0;
	if (sphere < activeSpheres && !descending[sphere])
		isActive = 1;
	return isActive;
}
