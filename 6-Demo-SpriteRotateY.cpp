// Sprite.cpp - 2D display and manipulate texture-mapped quad
//              options: full-screen, compensate for aspect ratio
// Copyright (c) 2024 Jules Bloomenthal, all rights reserved. Commercial use requires license.

#include <glad.h>
#include <GLFW/glfw3.h>
#include <time.h>
#include "Draw.h"
#include "GLXtras.h"
#include "Sprite.h"

Sprite background, actor;
bool hit = false;			// sprite selected by user?

// Flipping

bool flipping = false, flippingBack = true;
time_t flipTime = 0;		// start of flip
float flipDuration = 1;		// in seconds

void Flip(float degrees) {
	vec3 s(actor.scale, 1);
	// compensate for aspect ratio
	vec4 vp = VP();
	float w = vp[2], h = vp[3];
	w > h? s.x *= h/w : s.y *= w/h;
	actor.ptTransform = Translate(actor.position.x, actor.position.y, actor.z)*Scale(s)*RotateY(degrees)*Translate(0, 0, -actor.z);
}

// Display

void Display() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_CLAMP); // disable clipping in z
	if (flipping) {
		float dt = (float)(clock()-flipTime)/CLOCKS_PER_SEC, a = dt/flipDuration;
		float degrees = flippingBack? 180+180*a : 180*a;	
		cout << degrees << endl; // amount of rotation
		Flip(degrees);											// rotate about Y-axis
		actor.SetFrame(degrees > 90 && degrees < 270? 1 : 0);	// change frame when sprite on edge
		flipping = dt < flipDuration;							// done?
	}
	background.Display();
	actor.Display();
	glFlush();
}

// Mouse

void MouseButton(float x, float y, bool left, bool down) {
	hit = false;
	if (left && down && actor.Hit(x, y)) {
		// with z-buffer enabled, this test is on per-pixel basis
		//   transparent portions of the sprite are not hit-able
		// otherwise, simple bounding-box test
		hit = true;
		actor.Down(x, y);
	}
}

void MouseMove(float x, float y, bool leftDown, bool rightDown) {
	if (leftDown && hit)
		actor.Drag(x, y);
}

// Application

void MoveActor(float dx, float dy) {
	actor.SetPosition(actor.position+vec2(dx, dy));
}

void Keyboard(int key, bool press, bool shift, bool control) {
	if (press) {
		float d = .05f;
		if (key == GLFW_KEY_LEFT) MoveActor(-d, 0);
		if (key == GLFW_KEY_RIGHT) MoveActor(d, 0);
		if (key == GLFW_KEY_DOWN) MoveActor(0, -d);
		if (key == GLFW_KEY_UP) MoveActor(0, d);
		if (key == 'F' && !flipping) {
			flipping = true;
			flipTime = clock();
			flippingBack = !flippingBack;
		}
	}
}

void Resize(int width, int height) {
	glViewport(0, 0, width, height);
	actor.UpdateTransform();
}

const char *usage = R"(Usage:
	mouse drag: move sprite
	arrow keys: move sprite
	F: flip sprite
)";

int main(int ac, char **av) {
	GLFWwindow *w = InitGLFW(100, 100, 600, 600, "Sprite Rotate Y");
	// read background, foreground
	string dir("C:/Assets/Images/"), front(dir+"fishleft.png"), back(dir+"fishright.png"), mat(dir+"Space_Background_yellow.png");
	vector<string> frames{ front, back };
	background.Initialize(dir+"fishbackground.png", 0);
	actor.Initialize(frames, mat, -1);
	actor.autoAnimate = false;
	actor.SetFrame(0);
	actor.SetScale(vec2(.4f, .4f));
	// callbacks
	RegisterMouseButton(MouseButton);
	RegisterMouseMove(MouseMove);
	RegisterResize(Resize);
	RegisterKeyboard(Keyboard);
	// event loop
	printf(usage);
	while (!glfwWindowShouldClose(w)) {
		Display();
		glfwSwapBuffers(w);
		glfwPollEvents();
	}
}
