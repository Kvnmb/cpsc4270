// FishTankGame.cpp - 2D fish tank simulation game executable file
// Kevin Bui, Christian Buenafe
// 

#include <glad.h>
#include <GLFW/glfw3.h>
#include "Draw.h"
#include "GLXtras.h"
#include "Sprite.h"
#include <algorithm>
#include <time.h>
#include "Text.h"
#include <iostream>
#include <string>
#include <iomanip>
#include "Wav.h"

Wav wav("C:/Assets/Audio/fishgamesong.wav"); // music for the game
float volume = 0.25;


// sprites
Sprite background, fish,
	playButton, shopButton, foodButton, xButton, boat, chest, volcano, snail, upgrade,
	goldfish, redfish, displaySnail, displayGoldfish, displayRedfish,
	* actors[] = { &fish, &playButton, &shopButton,
	&foodButton, &xButton, &boat, &chest, &volcano, &snail, &upgrade, &goldfish, &redfish, &displaySnail,
	&displaySnail, &displayGoldfish, &displayRedfish, }, * selected = NULL;

double money = 0; // game currency

// values for text
int winWidth = 1920, winHeight = 1080;
vec3 org(1, 1, 1);

// booleans to change background and what is visible
bool startGame = false;
bool feedingTime = false;
bool displayShop = false;

// shop booleans
bool buyBoat = false;
bool buyChest = false;
bool buyVolcano = false;
bool buySnail = false;
bool buyUpgrade = false;
bool buyRedfish = false;
bool buyGoldfish = false;

int boatCost = 30, chestCost = 40, volcanoCost = 50, snailCost = 20, upgradeCost = 10, goldfishCost = 5, redfishCost = 15;

// values for tank capacity
int numUpgrades = 0;
int numFish = 1;
int capacity = 1;

enum ItemType { // shop's buy button index values
	BOAT = 0,
	CHEST = 1,
	VOLCANO = 2,
	SNAIL = 3,
	UPGRADE = 4,
	GOLDFISH = 5,
	REDFISH = 6,
};

// vector of buy button sprites
vector<Sprite>buyButtonsVec;
vec2 buyButtonPositions[] = { {-1.2f, -0.7f}, {0.0f, -0.7f}, {0.9f, -0.7f }, { -1.2f, 0.1f }, {-0.4f, 0.1f}, {0.5f, 0.1f}, {1.2f, 0.1f} };

// vector of messes to be spawned in
vector<Sprite>messVec;
float messZ = -0.05f; // will vary from -0.05f to -0.4f for unique clicks

vector<Sprite>pelletsVec;

// sensor locations (wrt fish sprite)
vec2 fishSensors[] = { {-.45f, .0f}, {.5f, -.4f}, {.45f, .4f}, {.85f, .0f}, {-.05f, .65f}, {-.3f, -.35f}, {-.4f, .5f}, {-.05f, -.8f} };
const int nFishSensors = sizeof(fishSensors) / sizeof(vec2);
vec3 fishProbes[nFishSensors];


// booleans to fix fish flipping
bool hitWall = false;
bool needToFlip = false;
bool direction = true; // True is right, false is left


// initialize function for many sprites
void gameInitialize() {
	vector<string> a{ "C:/Assets/Images/fishleft.png", "C:/Assets/Images/fishright.png" };
	fish.Initialize(a, "", -1.f, 0.25);
	fish.SetScale(vec2(.3f, .3f));
	fish.SetFrame(0);

	shopButton.Initialize("C:/Assets/Images/shopbutton.png", -.98f, false);
	shopButton.SetScale(vec2(.1f, .1f));
	shopButton.SetPosition(vec2(0.6f, .9f));

	vector<string> c{ "C:/Assets/Images/foodbutton.png", "C:/Assets/Images/foodbuttonpressed.png" };
	foodButton.Initialize(c, "", -.96f, false);
	foodButton.autoAnimate = false;
	foodButton.SetScale(vec2(.2f, .1f));
	foodButton.SetPosition(vec2(-1.4f, -0.9f));
	foodButton.SetFrame(0);

	xButton.Initialize("C:/Assets/Images/x.png", -.94f, false);
	xButton.SetScale(vec2(.4f, .4f));
	xButton.SetPosition(vec2(0.9f, .64f));

	boat.Initialize("C:/Assets/Images/boat.png", -.92f, false);
	boat.SetScale(vec2(.3f, .3f));
	boat.SetPosition(vec2(-0.7f, -0.2f));


	vector<string> d{ "C:/Assets/Images/buybutton.png", "C:/Assets/Images/checkmark.png" };
	float z = -0.5f;
	for (vec2 position : buyButtonPositions) { // create buyButton sprites for each coordinate in vector
		Sprite buyButton;
		buyButton.Initialize(d, "", z, false);
		buyButton.SetScale(vec2(.3f, .3f));
		buyButton.autoAnimate = false;
		buyButton.SetFrame(0);
		buyButton.SetPosition(position);
		buyButtonsVec.push_back(buyButton);
		z -= 0.02f; // need to change z value so buttons aren't all linked, 
	}

	chest.Initialize("C:/Assets/Images/Chest.png", -.92f, false);
	chest.SetScale(vec2(.2f, .2f));
	chest.SetPosition(vec2(-.6f, -0.5f));

	vector<string> e{ "C:/Assets/Images/volcano_1.png", "C:/Assets/Images/volcano_2.png" };
	volcano.Initialize(e, "", -.92f, 0.5);
	volcano.SetScale(vec2(.4f, .4f));
	volcano.SetPosition(vec2(0.5f, -0.4f));
	volcano.SetFrame(0);

	upgrade.Initialize("C:/Assets/Images/aquariumPlus.png", -.92f, 0.5);
	upgrade.SetScale(vec2(.4f, .4f));
	upgrade.SetPosition(vec2(-0.35f, 0.4f));

	vector<string> f{ "C:/Assets/Images/gary_1.png", "C:/Assets/Images/gary_2.png" };
	snail.Initialize(f, "", -1.f, 0.5);
	snail.SetScale(vec2(.4f, .4f));
	snail.SetPosition(vec2(-1.4f, -0.7f));
	snail.SetFrame(0);
	snail.autoAnimate = false;

	vector<string> g{ "C:/Assets/Images/goldfish.png", "C:/Assets/Images/gold_fish_3.png" };
	goldfish.Initialize(g, "", -1.f, 0.5);
	goldfish.SetScale(vec2(.4f, .4f));
	goldfish.SetPosition(vec2(-0.6f, -0.1f));
	goldfish.SetFrame(0);
	goldfish.autoAnimate = false;

	// goldfish starts going left
	goldfish.uvTransform = goldfish.uvTransform * Scale(-1, 1, 1);
	goldfish.ptTransform = goldfish.ptTransform * Scale(-1, 1, 1);

	vector<string> h{ "C:/Assets/Images/red_fish_2.png", "C:/Assets/Images/red_fish_3.png" };
	redfish.Initialize(h, "", -1.f, 0.5);
	redfish.SetScale(vec2(.4f, .4f));
	redfish.SetPosition(vec2(1.f, .6f));
	redfish.SetFrame(0);
	redfish.autoAnimate = false;

	// sprites for shop display
	displaySnail.Initialize("C:/Assets/Images/gary_1.png", -.92f, false);
	displayGoldfish.Initialize("C:/Assets/Images/red_fish_2.png", -.92f, false);
	displayRedfish.Initialize("C:/Assets/Images/goldfish.png", -.92f, false);

}

void spawnMess() {
	Sprite mess;
	mess.Initialize("C:/Assets/Images/Algae.png", messZ, false);
	mess.SetScale(vec2(.3f, .3f));
	float xSpawn = -1.f + (float)(rand()) / RAND_MAX * (1.f - (-1.f)); // calculating random spot on screen
	float ySpawn = -1.f + (float)(rand()) / RAND_MAX * (1.f - (-1.f));
	mess.SetPosition(vec2(xSpawn, ySpawn));

	messVec.push_back(mess);
	messZ -= 0.02f;

	if (messZ <= -0.4f) // not enough z values, need to stay within a range
		messZ = -0.05f;
}

void spawnPellet(float x, float y) {
	Sprite pellet;
	pellet.Initialize("C:/Assets/Images/fishpellet.png", -0.85f, false);
	pellet.SetScale(vec2(0.025f, 0.025f));
	pellet.SetScreenPosition(x, y);

	pelletsVec.push_back(pellet);
}


// Display

vec3 Probe(vec2 ndc) {
	// ndc (normalized device coords) lower left (-1,-1) to upper right (1,1)
	// return screen-space s: s.z is depth-buffer value at pixel (s.x, s.y)
	int4 vp = VP();
	vec3 s(vp[0] + (ndc.x + 1) * vp[2] / 2, vp[1] + (ndc.y + 1) * vp[3] / 2, 0);
	DepthXY((int)s.x, (int)s.y, s.z);
	return s;
}

vec3 Probe(vec2 v, mat4 m) {
	// promote v to 4D with arbitrary z and w = 1, transform by m
	return Probe(Vec2(m * vec4(v, 0, 1)));
}

void textDisplay() { // function to display both the money and the fish capacity
	float fontSize = 36;
	string moneyText = to_string(money);
	const char* moneycstr = moneyText.c_str();
	int wMoney = TextWidth(fontSize, moneycstr);

	vec2 s1(winWidth - wMoney + 150, winHeight - 150);

	string capacityText = to_string(numFish) + " / " + to_string(capacity) + " Capacity";
	const char* capacitycstr = capacityText.c_str();

	vec2 s2(0, winHeight - 150);

	glDisable(GL_DEPTH_TEST);
	Text(s1.x, s1.y, org, fontSize, moneycstr);
	Text(s2.x, s2.y, org, fontSize, capacitycstr);
	glEnable(GL_DEPTH_TEST);
}

void displayBoughtStuff() { // checks through bought booleans to determine whether to display on home screen
	if (buyBoat) {
		boat.SetScale(vec2(.3f, .3f));
		boat.SetPosition(vec2(0.7f, -0.4f));
		boat.Display();
	}

	if (buyChest) {
		chest.SetScale(vec2(.2f, .2f));
		chest.SetPosition(vec2(-.6f, -0.5f));
		chest.Display();
	}

	if (buyVolcano) {
		volcano.SetScale(vec2(.4f, .4f));
		volcano.SetPosition(vec2(0.5f, -0.4f));
		volcano.autoAnimate = true;
		volcano.Display();
	}

	if (buySnail) {
		snail.autoAnimate = true;
		snail.Display();
	}

	if (buyGoldfish) {
		goldfish.autoAnimate = true;
		goldfish.Display();
	}

	if (buyRedfish) {
		redfish.autoAnimate = true;
		redfish.Display();
	}
}

void displayShopStuff() { // shows things available in shop screen
	xButton.Display(); // button to close shop


	// setting position and scale since it is different in home screen

	boat.SetScale(vec2(.3f, .3f)); 
	boat.SetPosition(vec2(-0.7f, -0.2f));
	boat.Display();


	chest.SetScale(vec2(.2f, .2f));
	chest.SetPosition(vec2(0.0f, -0.3f));
	chest.Display();


	volcano.SetScale(vec2(.3f, .3f));
	volcano.SetPosition(vec2(.8f, -0.2f));
	volcano.autoAnimate = false;
	volcano.Display();


	upgrade.Display();

	// sprite to display the snail, goldfish, and redfish as a still placeholder, since these move

	
	displaySnail.SetScale(vec2(.4f, .4f));
	displaySnail.SetPosition(vec2(-0.7f, 0.4f));


	displaySnail.Display();
	
	displayRedfish.SetScale(vec2(.2f, .2f));
	displayRedfish.SetPosition(vec2(0.3f, 0.4f));


	displayRedfish.Display();
	
	displayGoldfish.SetScale(vec2(.2f, .2f));
	displayGoldfish.SetPosition(vec2(.7f, 0.4f));


	displayGoldfish.Display();

	// display prices

	float fontSize = 30;

	string boatStr = to_string(boatCost);
	const char* boatcstr = boatStr.c_str();

	string chestStr = to_string(chestCost);
	const char* chestcstr = chestStr.c_str();

	string volcanoStr = to_string(volcanoCost);
	const char* volcanocstr = volcanoStr.c_str();

	string snailStr = to_string(snailCost);
	const char* snailcstr = snailStr.c_str();

	string upgradeStr = to_string(upgradeCost);
	const char* upgradecstr = upgradeStr.c_str();

	string goldfishStr = to_string(goldfishCost);
	const char* goldfishcstr = goldfishStr.c_str();

	string redfishStr = to_string(redfishCost);
	const char* redfishcstr = redfishStr.c_str();
	

	glDisable(GL_DEPTH_TEST);
	Text(100, 150, org, fontSize, boatcstr);
	Text(800, 150, org, fontSize, chestcstr);
	Text(1300, 150, org, fontSize, volcanocstr);
	Text(100, 550, org, fontSize, snailcstr);
	Text(550, 550, org, fontSize, upgradecstr);
	Text(1100, 550, org, fontSize, goldfishcstr);
	Text(1500, 550, org, fontSize, redfishcstr);
	glEnable(GL_DEPTH_TEST);

	for (Sprite& button : buyButtonsVec) {
		button.Display();

	}
}


void Display() {
	vec3 red(1, 0, 0), grn(0, .7f, 0), yel(1, 1, 0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);



	background.Display();

	if (!startGame) {
		playButton.Display();
	}

	for (int i = 0; i < nFishSensors; i++)
		fishProbes[i] = Probe(fishSensors[i], fish.ptTransform);

	// code to flip fish if it bumps into wall
	if (hitWall || needToFlip) {
		fish.uvTransform = fish.uvTransform * Scale(-1, 1, 1);
		fish.ptTransform = fish.ptTransform * Scale(-1, 1, 1);
		hitWall = false;
		needToFlip = false;
		direction = !direction;

		for (vec2& probe : fishSensors) probe.x *= -1;
	}

	if (startGame && !displayShop) { // home screen

		textDisplay(); // shows capacity and money
		fish.Display();
		shopButton.Display();
		foodButton.Display();

		displayBoughtStuff(); // shows bought items in home screen
		

		if (feedingTime) {
			for (Sprite &s : pelletsVec) {
				s.Display();
			}
		}

		for (Sprite& messes : messVec) { // algae displaying on screen
			messes.Display();
		}
	}

	if (displayShop) {
		textDisplay(); // shows capacity and money
		displayShopStuff();
	}


	glDisable(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_TEST); // need z-buffer for mouse hit-test
	glFlush();
}

bool fishEating() {
	for (int i = 0; i < nFishSensors; i++) {
		if (fishProbes[i].z < -0.01f) {
			return true;
		}
	}
	return false;
}

float snailX = 0.001f; //snail move speed

void snailMove() { // snail moves horizontally
	snail.SetPosition(snail.position + vec2(snailX, 0.0f));

	if (snail.position.x >= 1.45f || snail.position.x <= -1.45f) { // flip sprite when reaching ends of tank
		snailX *= -1;
		snail.uvTransform = snail.uvTransform * Scale(-1, 1, 1);
		snail.ptTransform = snail.ptTransform * Scale(-1, 1, 1);
	}

}

void setup() {
	vector<string> b{ "C:/Assets/Images/titlescreen.png", "C:/Assets/Images/fishbackground.png", "C:/Assets/Images/shopmenu.png" };
	background.Initialize(b, "", 0, false);
	background.SetScale(vec2(2.f, 1.f));
	background.autoAnimate = false;
	background.SetFrame(0);




	playButton.Initialize("C:/Assets/Images/playbutton.png", -0.9f, false);
	playButton.SetScale(vec2(.1f, .1f));
	playButton.SetPosition(vec2(0.0f, -0.7f));

	

}

float redfishDx = 0.003f;

void redfishMove() {
	redfish.SetPosition(redfish.position + vec2(redfishDx, 0.0f));

	if (redfish.position.x >= 1.45f || redfish.position.x <= -1.45f) { // flip sprite when reaching ends of tank
		redfishDx *= -1;
		redfish.uvTransform = redfish.uvTransform * Scale(-1, 1, 1);
		redfish.ptTransform = redfish.ptTransform * Scale(-1, 1, 1);
	}
}

float goldfishDx = -0.002f;

void goldfishMove() {
	goldfish.SetPosition(goldfish.position + vec2(goldfishDx, 0.0f));

	if (goldfish.position.x >= 1.45f || goldfish.position.x <= -1.45f) { // flip sprite when reaching ends of tank
		goldfishDx *= -1;
		goldfish.uvTransform = goldfish.uvTransform * Scale(-1, 1, 1);
		goldfish.ptTransform = goldfish.ptTransform * Scale(-1, 1, 1);
	}
}


float dx = 0.005f, dy = 0.005f;

// variables for fish finding pellets
bool locateFood = true;
float hypotenuse = 0.0f;
vec2 unitVectors = {};
const float swimSpeed = 0.005f;
vec2 swimToFood = {};
vec2 distanceToPellet = {};

void fishMove() {
	if (feedingTime) { // check if food button has been pressed
		if (!pelletsVec.empty()) {

			vec2 pelletPosition = pelletsVec[0].GetScreenPosition(); // get the lru pellet 



			if (locateFood) { // a pellet has been detected
				distanceToPellet = pelletPosition - fish.GetScreenPosition();

				// flips for direction changes from previous movement
				if (distanceToPellet.x < 0 && direction) {
					needToFlip = true;
				}

				if (distanceToPellet.x > 0 && !direction) {
					needToFlip = true;
				}

				// calculate the unit vector for straight line traversal
				hypotenuse = sqrt((distanceToPellet.x * distanceToPellet.x) + (distanceToPellet.y * distanceToPellet.y));

				unitVectors = { distanceToPellet.x / hypotenuse, distanceToPellet.y / hypotenuse };

				// multiply unit vector by fish swim speed to get straight line movement at desired pace
				swimToFood = { unitVectors.x * swimSpeed, unitVectors.y * swimSpeed };

				// set boolean to false so previous values do not update until pellet found
				locateFood = false;
			}

			if (fishEating() || fish.Intersect(pelletsVec[0])) {

				pelletsVec.erase(pelletsVec.begin()); // clear the eaten pellet
				locateFood = true;
				money += 0.1;

				if (dx < 0.0f && direction) needToFlip = true;
				if (dx > 0.0f && !direction) needToFlip = true;

			}

			fish.SetPosition(fish.position + swimToFood);
		}
	}
	else {
		if (dx < 0 && direction) {
			needToFlip = true;
		}

		if (dx > 0 && !direction) {
			needToFlip = true;
		}

		fish.SetPosition(fish.position + vec2(dx, dy)); // fish moves in dvd pattern

		if (fish.position.y >= 0.8f || fish.position.y <= -0.8f) // if fish reaches top or bottom of tank
			dy *= -1;

		// if fish reaches left or right edge of tank
		if (fish.position.x >= 1.45f || fish.position.x <= -1.45f) {
			dx *= -1;
			hitWall = true;
		}
	}
}

// Mouse



void MouseButton(float x, float y, bool left, bool down) {
	if (left && down) {
		selected = NULL;
		if (playButton.Hit(x, y)) { // start game, change background and initialize sprites
			background.SetFrame(1);
			startGame = true;
			gameInitialize();
			playButton.Release();

			wav.Play(volume); // play music!
			wav.Loop(volume, -1);
		}
		else if (startGame) { // if game has started check for the rest
			if (foodButton.Hit(x, y)) { // time to feed the fishies
				if (!feedingTime) {
					feedingTime = true;
					foodButton.SetFrame(1);
				}
				else {
					feedingTime = false;
					foodButton.SetFrame(0);
					pelletsVec.clear(); // if done feeding, clear pellets from screen
				}
			}
			if (shopButton.Hit(x, y) || xButton.Hit(x, y)) { // whether pulling up shop or exiting, change background
				displayShop = !displayShop;
				displayShop ? background.SetFrame(2) : background.SetFrame(1);
			}

			if (background.Hit(x, y) && feedingTime) { // clicking on open space to spawn pellet
				spawnPellet(x, y);
			}


			for (int i = 0; i < buyButtonsVec.size(); i++) { // check in vector to see if a buy button has been clicked
				if (buyButtonsVec[i].Hit(x, y)) {
					bool transactionApproved = false; // for console message after
					
					ItemType item = (ItemType)i; // index matching with enum
					switch (item) { // for each case, if player has enough money, pay up for item
						case BOAT:
							if (money >= 30 && buyBoat == false) {
								money -= 30;
								buyBoat = true;
								transactionApproved = true;
							}
							break;
						case CHEST:
							if (money >= 40 && buyChest == false) {
								money -= 40;
								transactionApproved = true;
								buyChest = true;
							}
							break;
						case VOLCANO:
							if (money >= 50 && buyVolcano == false) {
								money -= 50;
								transactionApproved = true;
								buyVolcano = true;
							}
							break;

						case SNAIL:
							if (numFish < capacity && money >= 20 && buySnail == false) { // for fishes, need to buy capacity upgrade to be able to buy fish as well
								money -= 20;
								transactionApproved = true;
								numFish++;
								buySnail = true;
							}
							break;
						case UPGRADE:
							if (numUpgrades < 3 && money >= 10 && buyUpgrade == false) { // only 3 upgrades available, same button can be clicked 3 times
								transactionApproved = true;
								money -= 10;
								numUpgrades++;
								capacity++;
								buyButtonsVec[i].SetFrame(0);
								if (numUpgrades >= 3) {
									buyButtonsVec[i].SetFrame(1);
								}
							} else {
								buyUpgrade = true;
							}
							
							break;
						case REDFISH:
							if (numFish < capacity && money >= 15 && buyRedfish == false) {
								transactionApproved = true;
								money -= 15;
								numFish++;
								buyRedfish = true;
							}
							break;
						case GOLDFISH:
							if (numFish < capacity && money >= 5 && buyGoldfish == false) {
								transactionApproved = true;
								money -= 5;
								numFish++;
								buyGoldfish = true;
							}
							break;
						default:
							break;
					}

					if (transactionApproved) { // print message for player
						if (item != UPGRADE)
							buyButtonsVec[i].SetFrame(1);
						cout << endl << "Purchase approved!" << endl;
					}
					else {
						cout << endl << "Insufficient funds or capacity full." << endl;
					}
				}
			}

			for (vector<Sprite>::iterator it = messVec.begin(); it != messVec.end();) { // check if player is cleaning up mess
				if (it->Hit(x, y)) {
					it = messVec.erase(it);
					money += 0.5; // they get money for it!
				}
				else {
					++it;
				}
			}
		}
	}
}

void Keyboard(int key, bool press, bool shift, bool control) {
	if (press) {
		if (key == 'F') { // dev cheats to speed up demo/tests
			money += 50;
		}
	}
}

// Application


void Resize(int w, int h) {
	glViewport(0, 0, w, h);
	for (Sprite* s : actors)
		s->UpdateTransform();
}

const char* usage = R"(Usage:
	left click mouse only, and f key for cheats
)";



int main(int ac, char** av) {
	clock_t startTime = clock(); // keep track of time to generate money
	clock_t messTime = clock(); // keep track of time to generate algae sprites

	GLFWwindow* w = InitGLFW(100, 100, 1000, 600, "Eddie's Fish Tank");

	// read background, foreground sprites for title screen
	setup();

	// callbacks
	RegisterMouseButton(MouseButton);
	RegisterResize(Resize);
	RegisterKeyboard(Keyboard);
	
	wav.OpenDevice(); // connect to audio device

	// event loop
	printf(usage);
	while (!glfwWindowShouldClose(w)) {

		if (startGame) {
			clock_t currentTime = clock();

			if ((float)(currentTime - startTime) / CLOCKS_PER_SEC >= 1.0) { // every second, generate money based on number of fish in tank
				money += numFish * 0.05;
				startTime = clock();
			}

			if ((float)(currentTime - messTime) / CLOCKS_PER_SEC >= 15.0) { // every 15 seconds, generate a mess somewhere on screen
				spawnMess();
				messTime = clock();
			}

			if (buySnail) {
				snailMove();
			}
			if (buyRedfish)
				redfishMove();

			if (buyGoldfish)
				goldfishMove();

			fishMove();
		}


		Display();
		glfwSwapBuffers(w);
		glfwPollEvents();
	}
}