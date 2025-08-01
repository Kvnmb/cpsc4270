// Draw.cpp - various draw operations
// Copyright (c) 2024 Jules Bloomenthal, all rights reserved. Commercial use requires license.

#include <glad.h>
#include "Draw.h"
#include "GLXtras.h"
#include "Text.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using std::string;

// viewport operations

void VPsize(int &width, int &height) {
	int vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	width = vp[2];
	height = vp[3];
}

vec4 VP() {
	vec4 vp;
	glGetFloatv(GL_VIEWPORT, (float *) &vp);
	return vp;
}

int4 VPi() {
	int4 vp;
	glGetIntegerv(GL_VIEWPORT, (int *) &vp);
	return vp;
}

int VPw() { return VPi()[2]; }

int VPh() { return VPi()[3]; }

mat4 Viewport() {
	// map +/-1 space to screen space viewport
	float vp[4];
	glGetFloatv(GL_VIEWPORT, vp);
	float x = vp[0], y = vp[1], w = vp[2], h = vp[3];
	return mat4(vec4(w/2,0,0,x+w/2), vec4(0,h/2,0,y+h/2), vec4(0,0,1,0), vec4(0,0,0,1));
		// **** something wrong here?
}

// support

bool UnProject(float xscreen, float yscreen, float zscreen, mat4 &fullview, vec3 &p, int4 vp) {
	// from https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/gluUnProject.xml
	// to compute the coordinates (objX objY objZ), gluUnProject multiplies
	// the normalized device coordinates by the inverse of model*proj:
	//	  [ objX ]               [ (2*winx-vp[0])/vp[2]-1 ]
	//	  | objY |  =  [PM]-1    | (2*winy-vp[1])/vp[3]-1 |
	//	  | objZ |               |             (2*winz)-1 |
	//	  [ objW ]               [                      1 ]
	mat4 inv;
	if (!InvertMatrix4x4(&fullview[0][0], &inv[0][0])) {
		printf("can't unproject\n");
		return false;
	}
	vec4 ndc(2.f*(xscreen-vp[0])/vp[2]-1.f, 2.f*(yscreen-vp[1])/vp[3]-1.f, 2*zscreen-1.f, 1.f);
	vec4 q = inv*ndc;
	p = vec3(q.x/q.w, q.y/q.w, q.z/q.w);
	return true;
}

void UnProjectInv(float xscreen, float yscreen, float zscreen, mat4 &inv, vec3 &p, int4 vp) {
	vec4 ndc(2.f*(xscreen-vp[0])/vp[2]-1.f, 2.f*(yscreen-vp[1])/vp[3]-1.f, 2*zscreen-1.f, 1.f);
	vec4 q = inv*ndc;
	p = vec3(q.x, q.y, q.z)/q.w;
}

// misc operations

bool DepthXY(int x, int y, float &depth) {
	if (glIsEnabled(GL_DEPTH_TEST)) {
		float v, depthRange[2]; // depthRange maps to window coordinates +/-1
		glGetFloatv(GL_DEPTH_RANGE, depthRange);
		glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &v);
		depth = -1+2*(v-depthRange[0])/(depthRange[1]-depthRange[0]);
		return true;
	}
	depth = 0;
	return false;
}

bool FrontFacing(vec3 base, vec3 vec, mat4 view) {
	vec4 xbase = view*vec4(base, 1), xhead = view*vec4(base+.1f*vec, 1);
	vec3 xb(xbase.x, xbase.y, xbase.z), xh(xhead.x, xhead.y, xhead.z), xv(xh-xb);
	return dot(xb, xv) < 0;
}
// bool FrontFacing(vec3 base, vec3 vec, mat4 view) { return ScreenZ(base, view) >= ScreenZ(base+vec, view); }

mat4 ScreenMode() {
	// map pixel space (xorigin, yorigin)-(xorigin+width,yorigin+height) to NDC (clip) space (-1,-1)-(1,1)
	float vp[4];
	glGetFloatv(GL_VIEWPORT, vp);
	float x = vp[0], y = vp[1], w = vp[2], h = vp[3];
	return Translate(-1, -1, 0)*Scale(2/w, 2/h, 1)*Translate(-x, -y, 0);
}

// screen operation (default viewport int4(0,0,0,0) uses current

bool IsVisible(vec3 p, mat4 fullview, vec2 *screenA, float fudge, int4 vp) {
	if (vp == int4(0,0,0,0)) vp = VP();
	vec4 xp = fullview*vec4(p, 1);
	vec2 clip(xp.x/xp.w, xp.y/xp.w);    // clip space, +/-1
	vec2 screen(vp[0]+vp[2]*(1+clip.x)/2.f, vp[1]+vp[3]*(1+clip.y)/2.f);
	if (screenA)
		*screenA = screen;
	float z = xp.z/xp.w, zScreen;
	glReadPixels((int)screen.x, (int)screen.y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &zScreen);
		// **** slow when used during rendering!
	zScreen = 2*zScreen-1; // seems to work (clip range +/-1 but zbuffer range 0-1)
	return z < zScreen+fudge;
}

vec2 ScreenPoint(vec3 p, mat4 m, float *zscreen, int4 vp) {
	if (vp == int4(0,0,0,0)) vp = VP();
	vec4 xp = m*vec4(p, 1);
	if (zscreen)
		*zscreen = xp.z; // /xp.w;
	return vec2(vp[0]+((xp.x/xp.w)+1)*.5f*(float)vp[2], vp[1]+((xp.y/xp.w)+1)*.5f*(float)vp[3]);
}

void ScreenLine(float xscreen, float yscreen, mat4 modelview, mat4 persp, vec3 &p1, vec3 &p2, int4 vp) {
	// compute 3D world space line, given by p1 and p2, that transforms
	// to a line perpendicular to the screen at (xscreen, yscreen)
	mat4 fullview = persp*modelview, inv;
	if (vp == int4(0,0,0,0)) vp = VP();
	if (!InvertMatrix4x4(&fullview[0][0], &inv[0][0]))
		printf("ScreenLine: can't unProject\n");
	else {
		UnProjectInv(xscreen, yscreen, .25f, inv, p1, vp);
		UnProjectInv(xscreen, yscreen, .50f, inv, p2, vp);
	}
	// double tpersp[4][4], tmodelview[4][4], a[3], b[3];
	// get viewport
	// int vp[4];
	// glGetIntegerv(GL_VIEWPORT, vp);
	// create transposes
	// for (int i = 0; i < 4; i++)
	// 	for (int j = 0; j < 4; j++) {
	// 		tmodelview[i][j] = modelview[j][i];
	// 		tpersp[i][j] = persp[j][i];
	// 	}
	// if (gluUnProject(xscreen, yscreen, .25, (const double*) tmodelview, (const double*) tpersp, vp, &a[0], &a[1], &a[2]) == GL_FALSE)
	// 	printf("UnProject false\n");
	// if (gluUnProject(xscreen, yscreen, .50, (const double*) tmodelview, (const double*) tpersp, vp, &b[0], &b[1], &b[2]) == GL_FALSE)
	// 	printf("UnProject false\n");
	// 	alternatively, a second point can be determined by transforming the origin by the inverse of modelview
	// 	this would yield in world space the camera location, through which all view lines pass
	// p1 = vec3(static_cast<float>(a[0]), static_cast<float>(a[1]), static_cast<float>(a[2]));
	// p2 = vec3(static_cast<float>(b[0]), static_cast<float>(b[1]), static_cast<float>(b[2]));
}

void ScreenRay(float xscreen, float yscreen, mat4 modelview, mat4 persp, vec3 &p, vec3 &v, int4 vp) {
	if (vp == int4(0,0,0,0)) vp = VP();
	// compute ray from p in direction v; p is transformed eyepoint, xscreen, yscreen determine v
	// origin of ray is always eye (translated origin)
	p = vec3(modelview[0][3], modelview[1][3], modelview[2][3]);
	mat4 fullview = persp*modelview;
	vec3 p1, p2;
	bool ok = UnProject(xscreen, yscreen, .25f, fullview, p1, vp) && // *** should be able to use p, no?
			  UnProject(xscreen, yscreen, .50f, fullview, p2, vp);
	v = normalize(p2-p1);
	// create transposes for gluUnproject
	//	double tpersp[4][4], tmodelview[4][4], a[3], b[3];
	//	for (int i = 0; i < 4; i++)
	//		for (int j = 0; j < 4; j++) {
	//			tmodelview[i][j] = modelview[j][i];
	//			tpersp[i][j] = persp[j][i];
	//		}
	// un-project two screen points of differing depth to determine v
	//	if (gluUnProject(xscreen, yscreen, .25, (const double*) tmodelview, (const double*) tpersp, vp, &a[0], &a[1], &a[2]) == GL_FALSE)
	//		printf("UnProject false\n");
	//	if (gluUnProject(xscreen, yscreen, .50, (const double*) tmodelview, (const double*) tpersp, vp, &b[0], &b[1], &b[2]) == GL_FALSE)
	//		printf("UnProject false\n");
	//	v = normalize(vec3((float) (b[0]-a[0]), (float) (b[1]-a[1]), (float) (b[2]-a[2])));
}

float ScreenDSq(double x, double y, vec3 p, mat4 m, float *zscreen, int4 vp) {
	// y presumed in original mouse/screen space (increasing y is downwards)
	if (vp == int4(0,0,0,0)) vp = VP();
	vec2 screen = ScreenPoint(p, m, zscreen, vp);
	double dx = x-screen.x, dy = y-screen.y;
	return (float) (dx*dx+dy*dy);
}

float ScreenDSq(int x, int y, vec3 p, mat4 m, float *zscreen, int4 vp) {
	if (vp == int4(0,0,0,0)) vp = VP();
	return ScreenDSq((double) x, (double) y, p, m, zscreen, vp);
}

float ScreenD(double x, double y, vec3 p, mat4 m, float *zscreen, int4 vp) {
	if (vp == int4(0,0,0,0)) vp = VP();
	return sqrt(ScreenDSq(x, y, p, m, zscreen, vp));
}

float ScreenD(int x, int y, vec3 p, mat4 m, float *zscreen, int4 vp) {
	if (vp == int4(0,0,0,0)) vp = VP();
	return sqrt(ScreenDSq(x, y, p, m, zscreen, vp));
}

// double ScreenZ(vec3 p, mat4 m) { return (m*vec4(p, 1)).z; }

// Draw Shader

int drawShader = 0;
mat4 drawView;

#ifdef __APPLE__
const char *drawVShader = R"(
	#version 410 core
	in vec3 position;
	in vec3 color;
	out vec3 vColor;
	out vec2 vUv;
	uniform mat4 view;
	void main() {
		vec2 uvs[] = vec2[4](vec2(0,0), vec2(0,1), vec2(1,1), vec2(1,0));
		vUv = uvs[gl_VertexID];
		gl_Position = view*vec4(position, 1);
		vColor = color;
	}
)";
#else
const char *drawVShader = R"(
	#version 130
	in vec3 position;
	in vec3 color;
	out vec3 vColor;
	out vec2 vUv;
	uniform mat4 view;
	void main() {
		vec2 uvs[] = vec2[4](vec2(0,0), vec2(0,1), vec2(1,1), vec2(1,0));
		vUv = uvs[gl_VertexID];
		gl_Position = view*vec4(position, 1);
		vColor = color;
	}
)";
#endif

#ifdef __APPLE__
const char *drawPShader = R"(
	#version 410 core
	in vec3 vColor;
	in vec2 vUv;
	out vec4 pColor;
	uniform float opacity = 1;
	uniform bool fadeToCenter = false;
	uniform bool ring = false;
	uniform bool useTexture = false;
	uniform sampler2D textureImage;
	float Fade(float t) {
		if (t < .95) return 1.;
		if (t > 1.05) return 0.;
		float a = (t-.95)/(1.05-.95);
		return 1-smoothstep(0, 1, a); // does smoothstep help?
	}
	float Ring(float t) {
		if (t < .7) return 0.;
		if (t > .9) return 1.;
		float a = (t-.7)/(.9-.7);
		return smoothstep(0, 1, a);
	}
	float DistanceToCenter() {
		// gl_PointCoord wrt point primitive size
		float dx = 1-2*gl_PointCoord.x;
		float dy = 1-2*gl_PointCoord.y;
		return sqrt(dx*dx+dy*dy);
	  }
	void main() {
		// GL_POINT_SMOOTH deprecated, so calc here
		// needs GL_POINT_SPRITE or 0x8861 enabled
		float o = opacity;
		if (fadeToCenter)
			o *= Fade(DistanceToCenter());
		if (ring)
			o *= Ring(DistanceToCenter());
		pColor = vec4(useTexture? texture(textureImage, vUv).rgb : vColor, o);		
	}
)";
#else
const char *drawPShader = R"(
	#version 130
	in vec3 vColor;
	in vec2 vUv;
	out vec4 pColor;
	uniform float opacity = 1;
	uniform bool fadeToCenter = false;
	uniform bool ring = false;
	uniform bool useTexture = false;
	uniform sampler2D textureImage;
	uniform int nTexChannels = 3;
	float Fade(float t) {
		if (t < .95) return 1.;
		if (t > 1.05) return 0.;
		float a = (t-.95)/(1.05-.95);
		return 1-smoothstep(0, 1, a); // does smoothstep help?
	}
	float Ring(float t) {
		if (t < .7) return 0.;
		if (t > .9) return 1.;
		float a = (t-.7)/(.9-.7);
		return smoothstep(0, 1, a);
	}
	float DistanceToCenter() {
		// gl_PointCoord wrt point primitive size
		float dx = 1-2*gl_PointCoord.x;
		float dy = 1-2*gl_PointCoord.y;
		return sqrt(dx*dx+dy*dy);
	  }
	void main() {
		// GL_POINT_SMOOTH deprecated, so calc here
		// needs GL_POINT_SPRITE or 0x8861 enabled
		if (opacity < 1 && gl_Color.a < 1)
			discard;									// in effect, transparent
		float o = opacity;
		if (fadeToCenter)
			o *= Fade(DistanceToCenter());
		if (ring)
			o *= Ring(DistanceToCenter());
		pColor = vec4(vColor, o);
		if (useTexture) {
			if (nTexChannels == 4) {
				pColor = texture(textureImage, vUv);
				if (pColor.a < .02)						// if nearly transparent,
					discard;							// don't tag z-buffer
			}
			else
				pColor = vec4(texture(textureImage, vUv).rgb, o);
		}		
	}
)";
#endif

mat4 GetDrawView() { return drawView; }

void SetDrawView(mat4 m) { SetUniform(drawShader, "view", drawView = m); }

GLuint UseDrawShader() {
	int was = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &was);
	bool init = !drawShader;
	if (init) drawShader = LinkProgramViaCode(&drawVShader, &drawPShader);
	glUseProgram(drawShader);
	if (init) drawView = mat4();
	SetUniform(drawShader, "view", drawView);
	return was;
}

GLuint UseDrawShader(mat4 viewMatrix) {
	int was = UseDrawShader();
	SetUniform(drawShader, "view", viewMatrix);
	drawView = viewMatrix;
	return was;
}

// Disks

GLuint diskVBO = 0, diskVAO = 0;

void Disk(vec2 p, float diameter, vec3 color, float opacity, bool ring) {
	Disk(vec3(p), diameter, color, opacity, ring);
}

void Disk(vec3 p, float diameter, vec3 color, float opacity, bool ring) {
	// diameter should be >= 0, <= 20
	UseDrawShader();
	// create buffer for single vertex (x,y,z,r,g,b)
	if (!diskVBO) {
		glGenVertexArrays(1, &diskVAO);
		glGenBuffers(1, &diskVBO);
		glBindVertexArray(diskVAO);
		glBindBuffer(GL_ARRAY_BUFFER, diskVBO);
		glBufferData(GL_ARRAY_BUFFER, 2*sizeof(vec3), NULL, GL_STATIC_DRAW);
	}
	glBindVertexArray(diskVAO);
	glBindBuffer(GL_ARRAY_BUFFER, diskVBO); // set active buffer
	// allocate buffer memory and load location and color data
	glBufferSubData(GL_ARRAY_BUFFER, 0, 3*sizeof(float), &p.x);
	glBufferSubData(GL_ARRAY_BUFFER, 3*sizeof(float), 3*sizeof(float), &color.x);
	// connect shader inputs
	VertexAttribPointer(drawShader, "position", 3, 0, (void *) 0);
	VertexAttribPointer(drawShader, "color", 3, 0, (void *) sizeof(vec3));
	// draw
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	SetUniform(drawShader, "opacity", opacity);
	SetUniform(drawShader, "ring", ring);
	glPointSize(diameter);
/* #ifdef GL_POINT_SMOOTH
	glEnable(GL_POINT_SMOOTH);
#endif
#if !defined(GL_POINT_SMOOTH) && defined(GL_POINT_SPRITE)
	glEnable(GL_POINT_SPRITE);
#endif
#if !defined(GL_POINT_SMOOTH) && !defined(GL_POINT_SPRITE) */
	glEnable(0x8861); // same as GL_POINT_SMOOTH [this is a 4.5 core bug]
	SetUniform(drawShader, "fadeToCenter", true); // needed if GL_POINT_SMOOTH and GL_POINT_SPRITE fail
// #endif
	glDrawArrays(GL_POINTS, 0, 1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Lines

GLuint lineVBO = 0, lineVAO = 0;

void Line(vec3 p1, vec3 p2, float width, vec3 col1, vec3 col2, float opacity) {
	UseDrawShader();
	// create a vertex buffer for the array
	vec3 data[] = {p1, p2, col1, col2};
	if (!lineVBO) {
		glGenVertexArrays(1, &lineVAO);
		glGenBuffers(1, &lineVBO);
		glBindVertexArray(lineVAO);
		glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(data), NULL, GL_STATIC_DRAW);
	}
	// set active vertex buffer, load location and color data
	glBindVertexArray(lineVAO);
	glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), (void *) data);
	// connect shader inputs, set uniforms
	VertexAttribPointer(drawShader, "position", 3, 0, (void *) 0);
	VertexAttribPointer(drawShader, "color", 3, 0, (void *) (2*sizeof(vec3)));
	SetUniform(drawShader, "fadeToCenter", false);  // gl_PointCoord fails for lines (instead, use GL_LINE_SMOOTH)
	SetUniform(drawShader, "opacity", opacity);
	// draw
	glLineWidth(width);
	glDrawArrays(GL_LINES, 0, 2);
	// cleanup
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Line(vec3 p1, vec3 p2, float width, vec3 col, float opacity) {
	Line(p1, p2, width, col, col, opacity);
}

void Line(vec2 p1, vec2 p2, float width, vec3 col1, vec3 col2, float opacity) {
	vec3 pp1(p1, 0), pp2(p2, 0);
	Line(pp1, pp2, width, col1, col2, opacity);
}

void Line(vec2 p1, vec2 p2, float width, vec3 col, float opacity) {
	Line(p1, p2, width, col, col, opacity);
}

void Line(int x1, int y1, int x2, int y2, float width, vec3 col, float opacity) {
	vec3 p1((float) x1, (float) y1, 0), p2((float) x2, (float) y2, 0);
	Line(p1, p2, width, col, col, opacity);
}

// LineDash and LineDot

void LineDash(vec3 p1, vec3 p2, float width, vec3 col1, vec3 col2, float opacity, float dashLen, float percentDash) {
	float totalLen = length(ScreenPoint(p2, drawView)-ScreenPoint(p1, drawView));
	int nDashes = (int)(.5f+totalLen/dashLen);
	vec3 seg = (p2-p1)/(float)nDashes, dash = percentDash*seg;
	for (int i = 0; i < nDashes; i++) {
		vec3 start = p1+(float)i*seg;
		Line(start, start+dash, width, col1, col2, opacity);
	}
}

void LineDot(vec3 p1, vec3 p2, float width, vec3 col, float opacity, int pixelSpacing) {
	float totalLen = length(ScreenPoint(p2, drawView)-ScreenPoint(p1, drawView));
	int nDots = (int) (totalLen/(float)pixelSpacing);
	vec3 d = (p2-p1)/(float)nDots;
	for (int i = 0; i < nDots; i++)
		Disk(p1+(float)i*d, width, col);
}
/*
void LineDot(vec3 p1, vec3 p2, mat4 view, float width, vec3 col, float opacity, int pixelSpacing) {
	UseDrawShader(view);
	float totalLen = length(ScreenPoint(p2, view)-ScreenPoint(p1, view));
	int nDots = (int) (totalLen/(float)pixelSpacing);
	vec3 d = (p2-p1)/(float)nDots;
	for (int i = 0; i < nDots; i++)
		Disk(p1+(float)i*d, width, col);
}
*/
GLuint lineStripVBO = 0, lineStripVAO = 0;

void LineStrip(int nPoints, vec3 *points, vec3 &color, float opacity, float width) {
	size_t pSize = nPoints*sizeof(vec3);
	if (!lineStripVBO) {
		glGenVertexArrays(1, &lineStripVAO);
		glGenBuffers(1, &lineStripVBO);
		glBindVertexArray(lineStripVAO);
		glBindBuffer(GL_ARRAY_BUFFER, lineStripVBO);
		glBufferData(GL_ARRAY_BUFFER, 2*pSize, NULL, GL_STATIC_DRAW);
	}
	glBindVertexArray(lineStripVAO);
	glBindBuffer(GL_ARRAY_BUFFER, lineStripVBO);
	std::vector<vec3> colors(nPoints, color);
	glBufferSubData(GL_ARRAY_BUFFER, 0, pSize, points);
	glBufferSubData(GL_ARRAY_BUFFER, pSize, pSize, colors.data());
	VertexAttribPointer(drawShader, "position", 3, 0, (void *) 0);
	VertexAttribPointer(drawShader, "color", 3, 0, (void *) pSize);
	SetUniform(drawShader, "fadeToCenter", false);
	SetUniform(drawShader, "opacity", opacity);
	glLineWidth(width);
	glDrawArrays(GL_LINE_STRIP, 0, nPoints);
}

// Quads

GLuint quadVBO = 0, quadVAO = 0;

void QuadInner(vec3 p1, vec3 p2, vec3 p3, vec3 p4, bool solid, vec3 col, float opacity, float lineWidth,
			   bool texture = false, GLuint textureName = 0, int textureUnit = 0, int nTexChannels = 3)
{
#ifdef __APPLE__
	Triangle(p1, p2, p3, col, col, col, opacity, !solid, col, lineWidth);
	Triangle(p1, p3, p4, col, col, col, opacity, !solid, col, lineWidth);
#else
	vec3 data[] = { p1, p2, p3, p4, col, col, col, col };
	UseDrawShader();
	if (quadVBO == 0) {
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(data), NULL, GL_STATIC_DRAW);
	}
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
	VertexAttribPointer(drawShader, "position", 3, 0, (void *) 0);
	VertexAttribPointer(drawShader, "color", 3, 0, (void *) (4*sizeof(vec3)));
	SetUniform(drawShader, "opacity", opacity);
	SetUniform(drawShader, "fadeToCenter", false);
	SetUniform(drawShader, "useTexture", texture);
	if (texture) {
		glActiveTexture(GL_TEXTURE0+textureUnit);
		glBindTexture(GL_TEXTURE_2D, textureName);
		SetUniform(drawShader, "textureImage", textureUnit);
		SetUniform(drawShader, "nTexChannels", nTexChannels);
	}
	glLineWidth(lineWidth);
	glDrawArrays(solid? GL_QUADS : GL_LINE_LOOP, 0, 4);
	SetUniform(drawShader, "useTexture", false);
#endif
}

void Quad(vec3 p1, vec3 p2, vec3 p3, vec3 p4, bool solid, vec3 col, float opacity, float lineWidth) {
	QuadInner(p1, p2, p3, p4, solid, col, opacity, lineWidth, false);
}

void Quad(vec3 p1, vec3 p2, vec3 p3, vec3 p4, GLuint textureName, int textureUnit, float opacity, int nChannels) {
	QuadInner(p1, p2, p3, p4, true, vec3(0,0,0), opacity, 1, true, textureName, textureUnit, nChannels);
}

void Quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, bool solid, vec3 color, float opacity, float lineWidth) {
	QuadInner(vec3((float)x1,(float)y1,0), vec3((float)x2,(float)y2,0), vec3((float)x3,(float)y3,0), vec3((float)x4,(float)y4,0), solid, color, opacity, lineWidth);
}

void Quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, bool solid, vec3 color, float opacity, float lineWidth) {
	QuadInner(vec3(x1,y1,0), vec3(x2,y2,0), vec3(x3,y3,0), vec3(x4,y4,0), solid, color, opacity, lineWidth);
}

// Star

void Star(vec3 p, float size, vec3 color) {
	mat4 mSave = drawView;
	vec2 s = ScreenPoint(p, drawView);
	UseDrawShader(ScreenMode());
	Disk(s, size, color);
	for (int i = 0, nRays = 8; i < nRays; i++) {
		float a = 3.1415f*(float)i/nRays;
		float r1 = 1.02f*size, r2 = size*(i%2? 1.7f : 2.1f), w = i%2? 1 : 1.75f;
		vec3 c = color; // i%2? vec3(0, 0, 1) : color;
		vec2 d(cos(a), sin(a));
		Line(s+r1*d, s+r2*d, w, c);
		Line(s-r1*d, s-r2*d, w, c);
	}
	UseDrawShader(mSave);
}

void Star(vec3 p, float size, vec3 colorVisible, vec3 colorHidden) {
	Star(p, size, IsVisible(p, drawView)? colorVisible : colorHidden);
}

// Arrow

void Arrow(vec2 base, vec2 head, vec3 col, float lineWidth, double headSize) {
	Line(base, head, lineWidth, col);
	if (headSize > 0) {
		vec2 v1 = (float)headSize*normalize(head-base), v2(v1.y/2.f, -v1.x/2.f);
		vec2 head1(head-v1+v2), head2(head-v1-v2);
		Line(head, head1, lineWidth, col);
		Line(head, head2, lineWidth, col);
	}
}

vec3 ProjectToLine(vec3 p, vec3 p1, vec3 p2) {
	// project p to line p1p2
	vec3 delta(p2-p1);
	float magSq = dot(delta, delta);
	float alpha = magSq > FLT_EPSILON? dot(delta, p-p1)/magSq : 0;
	return p1+alpha*delta;
}

void PointScreen(vec3 p, vec2 s, mat4 modelview, mat4 persp, float lineWidth, vec3 col) {
	// draw between p and the nearest point on a 3D screen line through s
	vec3 p1, p2;
	ScreenLine(s.x, s.y, modelview, persp, p1, p2); //, false);
	vec3 pp = ProjectToLine(p, vec3(p1), vec3(p2));
	Line(p, pp, lineWidth, col);
}

void ArrowV(vec3 base, vec3 v, mat4 modelview, mat4 persp, vec3 col, float lineWidth, double headSize) {
	mat4 m = persp*modelview;
	vec3 head(base+v);
	float zbase, zhead;
	vec2 base2 = ScreenPoint(base, m, &zbase), head2 = ScreenPoint(head, m, &zhead);
	vec2 v1 = (float)headSize*normalize(head2-base2), v2(v1.y/2.f, -v1.x/2.f);
	vec2 h1(head2-v1+v2), h2(head2-v1-v2);
	// col = zbase > zhead? vec3(0,1,0) : vec3(0,0,1);
	// could draw in screen mode, using base2, head2, h1, & h2, but prefer draw in 3D (allows for depth test)
	UseDrawShader(m);
	Line(base, head, lineWidth, col);
	PointScreen(head, h1, modelview, persp, lineWidth, col);
	PointScreen(head, h2, modelview, persp, lineWidth, col);
}

// Frame

namespace {

vec3 Col(mat4 m, int i) { return vec3(m[0][i], m[1][i], m[2][i]); }

void DrawArrow(vec3 p, vec3 v, mat4 modelview, mat4 persp, const char *s, vec3 color, float scale, vec3 textColor) {
	v = scale*normalize(v);
	ArrowV(p, v, modelview, persp, color, 2, 12);
	Text(p+v, persp*modelview, textColor, 12, s);
}

}

void Frame(mat4 m, mat4 modelview, mat4 persp, float scale, vec3 textColor) {
	vec3 o = Col(m, 3), x = Col(m, 0), y = Col(m, 1), z = Col(m, 2);
	DrawArrow(o, x, modelview, persp, "X", vec3(1, 0, 0), scale, textColor);
	DrawArrow(o, y, modelview, persp, "Y", vec3(0, 1, 0), scale, textColor);
	DrawArrow(o, z, modelview, persp, "Z", vec3(0, 0, 1), scale, textColor);
	Disk(o, 8, textColor);
}

// Cylinder

const char *cylVShader = R"(
	#version 410 core
	void main() { gl_Position = vec4(0); }
)";

const char *cylTCShader = R"(
	#version 410 core
	layout (vertices = 4) out;
	void main() {
		if (gl_InvocationID == 0) {
			gl_TessLevelOuter[0] = gl_TessLevelOuter[2] = 1;
			gl_TessLevelOuter[1] = gl_TessLevelOuter[3] = 24;
			gl_TessLevelInner[0] = gl_TessLevelInner[1] = 24;
		}
	}
)";

const char *cylTEShader = R"(
	#version 410 core
	layout (quads, equal_spacing, ccw) in;
	uniform vec3 p1;
	uniform vec3 p2;
	uniform float r1;
	uniform float r2;
	uniform mat4 modelview;
	uniform mat4 persp;
	out vec3 tePoint;
	out vec3 teNormal;
	void main() {
		vec2 uv = gl_TessCoord.st;
		float c = cos(2*3.1415*uv.s), s = sin(2*3.1415*uv.s);
		vec3 dp = p2-p1;
		vec3 crosser = dp.x < dp.y? (dp.x < dp.z? vec3(1,0,0) : vec3(0,0,1)) : (dp.y < dp.z? vec3(0,1,0) : vec3(0,0,1));
		vec3 xcross = normalize(cross(crosser, dp));
		vec3 ycross = normalize(cross(xcross, dp));
		vec3 n = c*xcross+s*ycross;
		vec3 p = mix(p1, p2, uv.t)+mix(r1, r2, uv.t)*n;
		tePoint = (modelview*vec4(p, 1)).xyz;
		teNormal = (modelview*vec4(n, 0)).xyz;
		gl_Position = persp*vec4(tePoint, 1);
	}
)";

const char *cylPShader = R"(
	#version 410 core //130
	in vec3 tePoint;
	in vec3 teNormal;
	out vec4 pColor;
	uniform vec4 color;
	uniform vec3 light;
	void main() {
		vec3 N = normalize(teNormal);      // surface normal
		vec3 L = normalize(light-tePoint); // light vector
		vec3 E = normalize(tePoint);       // eye vector
		vec3 R = reflect(L, N);            // highlight vector
		float d = abs(dot(N, L));          // two-sided diffuse
		float s = abs(dot(R, E));          // two-sided specular
		float intensity = clamp(d+pow(s, 50), 0, 1);
		pColor = intensity*color;
	}
)";

GLuint cylinderShader = 0;

void Cylinder(vec3 p1, vec3 p2, float r1, float r2, mat4 modelview, mat4 persp, vec4 color) {
	if (!cylinderShader)
		cylinderShader = LinkProgramViaCode(&cylVShader, &cylTCShader, &cylTEShader, NULL, &cylPShader);
	//	cylinderShader = LinkProgramViaCode(&vShader, NULL, &teShader, NULL, &pShader);
	glUseProgram(cylinderShader);
	SetUniform(cylinderShader, "modelview", modelview);
	SetUniform(cylinderShader, "persp", persp);
	SetUniform(cylinderShader, "color", color);
	SetUniform(cylinderShader, "p1", p1);
	SetUniform(cylinderShader, "p2", p2);
	SetUniform(cylinderShader, "r1", r1);
	SetUniform(cylinderShader, "r2", r2);
	// float r = 24, outerLevels[] = {1, r, 1, r}, innerLevels[] = {r, r};
	// glPatchParameteri(GL_PATCH_VERTICES, 4);
	// glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerLevels);
	// glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerLevels);
#ifdef GL_PATCHES
	glDrawArrays(GL_PATCHES, 0, 4);
#endif
}

GLuint GetCylinderShader() {
	if (!cylinderShader)
		cylinderShader = LinkProgramViaCode(&cylVShader, &cylTCShader, &cylTEShader, NULL, &cylPShader);
	return cylinderShader;
}

GLuint GetDrawShader() {
	if (!drawShader)
		drawShader = LinkProgramViaCode(&drawVShader, &drawPShader);
	return drawShader;
}

// Triangles with optional outline

GLuint triShader = 0, triVBO = 0, triVAO = 0;

// vertex shader
const char *triVShaderCode = R"(
	#version 330 core
	in vec3 point;
	in vec3 color;
	out vec3 vColor;
	uniform mat4 view;
	void main() {
		gl_Position = view*vec4(point, 1);
		vColor = color;
	}
)";

// geometry shader with line-drawing
const char *triGShaderCode = R"(
	#version 330 core
	layout (triangles) in;
	layout (triangle_strip, max_vertices = 3) out;
	in vec3 vPoint[];
	in vec3 vColor[];
	out vec3 gColor;
	noperspective out vec3 gEdgeDistance;
	uniform mat4 viewptM;
	vec3 ViewPoint(int i) {
		return vec3(viewptM*(gl_in[i].gl_Position/gl_in[i].gl_Position.w));
	}
	void main() {
		float ha = 0, hb = 0, hc = 0;
		// transform each vertex into viewport space
		vec3 p0 = ViewPoint(0), p1 = ViewPoint(1), p2 = ViewPoint(2);
		// find altitudes ha, hb, hc
		float a = length(p2-p1), b = length(p2-p0), c = length(p1-p0);
		float alpha = acos((b*b+c*c-a*a)/(2.*b*c));
		float beta = acos((a*a+c*c-b*b)/(2.*a*c));
		ha = abs(c*sin(beta));
		hb = abs(c*sin(alpha));
		hc = abs(b*sin(alpha));
		// send triangle vertices and edge distances
		for (int i = 0; i < 3; i++) {
			gEdgeDistance = i==0? vec3(ha, 0, 0) : i==1? vec3(0, hb, 0) : vec3(0, 0, hc);
			gColor = vColor[i];
			gl_Position = gl_in[i].gl_Position;
			EmitVertex();
		}
		EndPrimitive();
	}
)";

// pixel shader
const char *triPShaderCode = R"(
	#version 410 core
	in vec3 gColor;
	noperspective in vec3 gEdgeDistance;
	uniform vec4 outlineColor = vec4(0, 0, 0, 1);
	uniform float opacity = 1;
	uniform float outlineWidth = 1;
	uniform float transition = 1;
	uniform int outlineOn = 1;
	out vec4 pColor;
	void main() {
		pColor = vec4(gColor, opacity);
		if (outlineOn > 0) {
			float minDist = min(gEdgeDistance.x, min(gEdgeDistance.y, gEdgeDistance.z));
			float t = smoothstep(outlineWidth-transition, outlineWidth+transition, minDist);
			if (outlineOn == 2) pColor = vec4(1,1,1,1);
			pColor = mix(outlineColor, pColor, t);
		}
	}
)";

GLuint GetTriangleShader() {
	if (!triShader)
		triShader = LinkProgramViaCode(&triVShaderCode, NULL, NULL, &triGShaderCode, &triPShaderCode);
	return triShader;
}

GLuint UseTriangleShader() {
	bool init = triShader == 0;
	if (init)
		triShader = LinkProgramViaCode(&triVShaderCode, NULL, NULL, &triGShaderCode, &triPShaderCode);
	glUseProgram(triShader);
	if (init)
		SetUniform(triShader, "view", mat4());
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	return triShader;
}

GLuint UseTriangleShader(mat4 view) {
	GLuint s = UseTriangleShader();
	SetUniform(triShader, "view", view);
	return s;
}

void Triangle(vec3 p1, vec3 p2, vec3 p3, vec3 c1, vec3 c2, vec3 c3,
			  float opacity, bool outline, vec4 outlineCol, float outlineWidth, float transition) {
	vec3 data[] = { p1, p2, p3, c1, c2, c3 };
	UseTriangleShader();
	if (triVBO == 0) {
		glGenVertexArrays(1, &triVAO);
		glGenBuffers(1, &triVBO);
		glBindVertexArray(triVAO);
		glBindBuffer(GL_ARRAY_BUFFER, triVBO);
		glBufferData(GL_ARRAY_BUFFER, 3*sizeof(vec3), NULL, GL_STATIC_DRAW);
	}
	glBindVertexArray(triVAO);
	glBindBuffer(GL_ARRAY_BUFFER, triVBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, 3*sizeof(vec3), data);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
	VertexAttribPointer(triShader, "point", 3, 0, (void *) 0);
	VertexAttribPointer(triShader, "color", 3, 0, (void *) (3*sizeof(vec3)));
	SetUniform(triShader, "viewptM", Viewport()); // **** ????
	SetUniform(triShader, "opacity", opacity);
	SetUniform(triShader, "outlineOn", outline? 1 : 0);
	SetUniform(triShader, "outlineColor", outlineCol);
	SetUniform(triShader, "outlineWidth", outlineWidth);
	SetUniform(triShader, "transition", transition);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

// Boxes

void Box(vec3 a, vec3 b, float width, vec3 col) {
	float x1=a.x, x2=b.x, y1=a.y, y2=b.y, z1=a.z, z2=b.z;
	// left-right
	Line(vec3(x1,y1,z1), vec3(x2,y1,z1), width, col);
	Line(vec3(x1,y2,z1), vec3(x2,y2,z1), width, col);
	Line(vec3(x1,y1,z2), vec3(x2,y1,z2), width, col);
	Line(vec3(x1,y2,z2), vec3(x2,y2,z2), width, col);
	// bottom-top
	Line(vec3(x1,y1,z1), vec3(x1,y2,z1), width, col);
	Line(vec3(x1,y1,z2), vec3(x1,y2,z2), width, col);
	Line(vec3(x2,y1,z1), vec3(x2,y2,z1), width, col);
	Line(vec3(x2,y1,z2), vec3(x2,y2,z2), width, col);
	// near-far
	Line(vec3(x1,y1,z1), vec3(x1,y1,z2), width, col);
	Line(vec3(x1,y2,z1), vec3(x1,y2,z2), width, col);
	Line(vec3(x2,y1,z1), vec3(x2,y1,z2), width, col);
	Line(vec3(x2,y2,z1), vec3(x2,y2,z2), width, col);
}
