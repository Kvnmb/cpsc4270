// GLXtras.cpp - GLSL support
// Copyright (c) 2024 Jules Bloomenthal, all rights reserved. Commercial use requires license.

#include <glad/glad.h>
#include "GLXtras.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <unordered_map>

namespace {

GLFWwindow *w = NULL;
int4 origWin;

bool GetKey(int button) { return w? glfwGetKey(w, button) == GLFW_PRESS : false; }

MouseMoveCallback mmcb = NULL;
MouseButtonCallback mbcb = NULL;
MouseWheelCallback mwcb = NULL;
ResizeCallback rcb = NULL;
KeyboardCallback kcb = NULL;

double InvertY(GLFWwindow *w, double y) {
	// given y wrt upper left, return wrt lower left
	int h;
	glfwGetFramebufferSize(w, NULL, &h);
	return h-y;
}

void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
	vec2 v = MouseCoords();
	if (mbcb)
		mbcb(v.x, v.y, butn == GLFW_MOUSE_BUTTON_LEFT, action == GLFW_PRESS);
}

void MouseMove(GLFWwindow *w, double x, double y) {
	bool leftDown = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	bool rightDown = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
#ifdef __APPLE__
	// compensate for "retina coordinates"
	x *= 2;
	y *= 2;
#endif
	if (mmcb)
		mmcb((float) x, (float) InvertY(w, y), leftDown, rightDown);
}

void MouseWheel(GLFWwindow *w, double ignore, double spin) {
	if (mwcb)
		mwcb((float) spin);
}

void Resize(GLFWwindow *w, int width, int height) {
	// width, height in pixels (whether Apple or not)
	if (rcb)
		rcb(width, height);
}

std::unordered_map<int, time_t> keydownTime;

void Keyboard(GLFWwindow *w, int key, int scancode, int action, int mods) {
	keydownTime[key] = action == GLFW_PRESS? clock() : 0;
	if (action != GLFW_REPEAT)
		// repeat-mode is unreliable; better for app to test a held key within the event loop
		kcb(key, action == GLFW_PRESS, mods & GLFW_MOD_SHIFT, mods & GLFW_MOD_CONTROL);
	// kcb(key, action == GLFW_PRESS || action == GLFW_REPEAT, mods & GLFW_MOD_SHIFT, mods & GLFW_MOD_CONTROL);
}

} // end namespace

void SetMonitor(int x, int y, int width, int height) {
	GLFWmonitor *m = glfwGetWindowMonitor(w);
	glfwSetWindowMonitor(w, m, x, y, width, height, 60);
}

void SetFullScreen() {
	// window hints have no effect once window created
	glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);							// app always on top
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);							// user can't resize
	glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);							// no title bar
	const GLFWvidmode *vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	SetMonitor(0, 0, vidMode->width, vidMode->height);
}

void RestoreFromFullScreen() {
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);							// no effect
	glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);							// no effect
	SetMonitor(origWin[0], origWin[1], origWin[2], origWin[3]);
}

// #define PRINT_ERROR
#if defined PRINT_ERROR && !defined __APPLE__
int nErrors = 0;

void APIENTRY GlError(GLenum source, GLenum type, GLuint id, GLenum severity,
						GLsizei len, const GLchar *msg, const void *data) {
	nErrors++;
	if (nErrors == 10) {
		printf("skipping further errors\n");
		glDisable(GL_DEBUG_OUTPUT);
	}
	if (nErrors < 10)
		printf("GLSL Error: %s\n", msg);
}

void GlfwError(int id, const char *reason) {
	printf("GFLW error %i: %s\n", id, reason);
}
#endif

#define WIN_GL_VERSION_MAJOR 4
#define WIN_GL_VERSION_MINOR 5
#define APPLE_GL_VERSION_MAJOR 4 // default is 2
#define APPLE_GL_VERSION_MINOR 1

GLFWwindow *InitGLFW(int x, int y, int width, int height, const char *title, bool aa, bool fullScreen) {
	// initialization
	origWin = int4(x, y, width, height);
	glfwInit();
	// hints
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	if (aa)
		glfwWindowHint(GLFW_SAMPLES, 4);
		// this may fail unless NVidia Control Panel:
		// 3D Settings --> Adjust Image Settings with Preview
		//		Set Use my preference emphasizing to: Quality
#ifdef __APPLE__
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, APPLE_GL_VERSION_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, APPLE_GL_VERSION_MINOR);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, WIN_GL_VERSION_MAJOR);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, WIN_GL_VERSION_MINOR);
#endif
	if (fullScreen) {
		glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);					// app always on top
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);					// user can't resize
		glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);					// no title bar
		x = y = 0;
		const GLFWvidmode *vidMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
		w = glfwCreateWindow(vidMode->width, vidMode->height, title, NULL, NULL);
		SetMonitor(0, 0, vidMode->width, vidMode->height);
	} else 
		w = glfwCreateWindow(width, height, title, NULL, NULL);
	glfwSetWindowPos(w, x, y);

	if (!w)
		glfwTerminate();
	else {
		glfwMakeContextCurrent(w);
		glfwSwapInterval(1);
		gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	}
#if defined PRINT_ERROR && !defined __APPLE__
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(GlError, NULL);
	glfwSetErrorCallback(GlfwError);
#endif
#ifdef __APPLE__
	int fbW, fbH, winW, winH;
	glfwGetFramebufferSize(w, &fbW, &fbH);
	glfwGetWindowSize(w, &winW, &winH); 
	printf("window size: %i x %i, framebuffer size: %i x %i\n", winW, winH, fbW, fbH);
#endif
	return w;
}

vec2 MouseCoords() {
	// return mouse coords wrt lower left
	double x, y;
	glfwGetCursorPos(w, &x, &y);
#ifdef __APPLE__
	// compensate for "retina coordinates"
	// *** or? new GLFW window hint GLFW_COCOA_RETINA_FRAMEBUFFER?
	x *= 2;
	y *= 2;
#endif
	return vec2((float) x, (float) InvertY(w, y));
}

float KeydownElapsed(int key) {
	return (float)(clock()-keydownTime[key])/CLOCKS_PER_SEC;
}

bool KeyDown(int key) {
	return GetKey(key);
}

bool Shift() {
	return GetKey(GLFW_KEY_LEFT_SHIFT) || GetKey(GLFW_KEY_RIGHT_SHIFT);
}

bool Control() {
	return GetKey(GLFW_KEY_LEFT_CONTROL) || GetKey(GLFW_KEY_RIGHT_CONTROL);
}

void RegisterMouseButton(MouseButtonCallback cb) {
	mbcb = cb;
	glfwSetMouseButtonCallback(w, MouseButton);
}

void RegisterMouseMove(MouseMoveCallback cb) {
	mmcb = cb;
	glfwSetCursorPosCallback(w, MouseMove);
}

void RegisterMouseWheel(MouseWheelCallback cb) {
	mwcb = cb;
	glfwSetScrollCallback(w, MouseWheel);
}

void RegisterResize(ResizeCallback cb) {
	rcb = cb;
	glfwSetFramebufferSizeCallback(w, Resize);
}

void RegisterKeyboard(KeyboardCallback cb) {
	kcb = cb;
	glfwSetKeyCallback(w, Keyboard);
}

// Print OpenGL, GLSL Details

#ifndef __APPLE__
const char *ErrorString(GLenum n) {
	if (n == GL_NO_ERROR) return NULL;
	if (n == GL_INVALID_ENUM) return "invalid enum";
	if (n == GL_INVALID_VALUE) return "invalid value";
	if (n == GL_INVALID_OPERATION) return "invalid operation";
	if (n == GL_INVALID_FRAMEBUFFER_OPERATION) return "invalid framebuffer operation";
	if (n == GL_OUT_OF_MEMORY) return "out of memory";
	if (n == GL_STACK_UNDERFLOW) return "stack underflow";
	if (n == GL_STACK_OVERFLOW) return "stack overflow";
	return "<unknown error>";
}

int PrintGLErrors(const char *title) {
	char buf[1000];
	int nErrors = 0;
	buf[0] = 0;
	for (;;) {
		GLenum n = glGetError();
		if (n == GL_NO_ERROR)
			break;
	//	sprintf(buf+strlen(buf), "%s%s", !nErrors++? "" : ", ", ErrorString(n));
		int len = strlen(buf);
		snprintf(buf+len, 999-len, "%s%s", !nErrors++? "" : ", ", ErrorString(n));
			// do not call Debug() while looping through errors, so accumulate in buf
	}
	if (nErrors) {
		if (title) printf("%s (GL errors): %s\n", title, buf);
		else printf("GL errors: %s\n", buf);
	}
	return nErrors;
}
#endif

void PrintVersionInfo() {
	const GLubyte *renderer    = glGetString(GL_RENDERER);
	const GLubyte *vendor      = glGetString(GL_VENDOR);
	const GLubyte *version     = glGetString(GL_VERSION);
	const GLubyte *glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
	printf("GL vendor: %s\n", vendor);
	printf("GL renderer: %s\n", renderer);
	printf("GL version: %s\n", version);
	printf("GLSL version: %s\n", glslVersion);
	// GLint major, minor;
	// glGetIntegerv(GL_MAJOR_VERSION, &major);
	// glGetIntegerv(GL_MINOR_VERSION, &minor);
	// printf("GL version (integer): %d.%d\n", major, minor);
}

void PrintExtensions() {
	const GLubyte *extensions = glGetString(GL_EXTENSIONS);
	const char *skip = "(, \t\n";
	char buf[100];
	printf("\nGL extensions:\n");
	if (extensions)
		for (char *c = (char *) extensions; *c; ) {
				c += strspn(c, skip);
				size_t nchars = strcspn(c, skip);
				strncpy(buf, c, nchars);
				buf[nchars] = 0;
				printf("  %s\n", buf);
				c += nchars;
		}
}

void PrintProgramLog(GLuint programID) {
	GLint logLen;
	glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &logLen);
	if (logLen > 0) {
		char *log = new char[logLen];
		GLsizei written;
		glGetProgramInfoLog(programID, logLen, &written, log);
		printf("Program log:\n%s", log);
		delete [] log;
	}
}

void PrintProgramAttributes(GLuint programID) {
	GLint maxLength, nAttribs;
	glGetProgramiv(programID, GL_ACTIVE_ATTRIBUTES, &nAttribs);
	glGetProgramiv(programID, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
	char *name = new char[maxLength];
	GLint written, size;
	GLenum type;
	for (int i = 0; i < nAttribs; i++) {
		glGetActiveAttrib(programID, i, maxLength, &written, &size, &type, name);
		GLint location = glGetAttribLocation(programID, name);
		printf("    %-5i  |  %s\n", location, name);
	}
	delete [] name;
}

void PrintProgramUniforms(GLuint programID) {
	GLenum type;
	GLchar name[201];
	GLint nUniforms, length, size;
	glGetProgramiv(programID, GL_ACTIVE_UNIFORMS, &nUniforms);
	printf("  uniforms\n");
	for (int i = 0; i < nUniforms; i++) {
		glGetActiveUniform(programID, i,  200,  &length, &size, &type, name);
		printf("    %s\n", name);
	}
}

// Compilation

GLuint CompileShaderViaFile(const char *filename, GLint type) {
		FILE* fp = fopen(filename, "r");
		if (fp == NULL) {
			printf("can't open %s\n", filename);
			return 0;
		}
		fseek(fp, 0L, SEEK_END);
		long maxSize = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		char *buf = new char[maxSize+1], c;
		int nchars = 0;
		while ((c = fgetc(fp)) != EOF && nchars < maxSize)
			buf[nchars++] = c;
		buf[nchars] = 0;
		fclose(fp);
		GLuint s = CompileShaderViaCode((const char **) &buf, type);
		delete [] buf;
		return s;
}

GLuint CompileShaderViaCode(const char **code, GLint type) {
	GLuint shader = glCreateShader(type);
	if (!shader) {
#ifndef __APPLE__
		PrintGLErrors();
#endif
		return 0;
	}
	glShaderSource(shader, 1, code, NULL);
	glCompileShader(shader);
	// check compile status
	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		// squawk logged errors
		GLint logLen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0) {
			GLsizei written;
			char *log = new char[logLen];
			glGetShaderInfoLog(shader, logLen, &written, log);
			printf("compilation failed: %s", log);
			delete [] log;
		}
		else printf("shader compilation failed\n");
		return 0;
	}
	return shader;
}

// Linking

GLuint LinkProgramViaCode(const char **vertexCode, const char **pixelCode) {
	GLuint vshader = CompileShaderViaCode(vertexCode, GL_VERTEX_SHADER);
	GLuint pshader = CompileShaderViaCode(pixelCode, GL_FRAGMENT_SHADER);
	GLuint p = LinkProgram(vshader, pshader);
	glDetachShader(p, vshader);
	glDetachShader(p, pshader);
	glDeleteShader(vshader);
	glDeleteShader(pshader);
	return p;
}

GLuint LinkProgramViaCode(const char **vertexCode,
						  const char **tessellationControlCode,
						  const char **tessellationEvalCode,
						  const char **geometryCode,
						  const char **pixelCode) {
	GLuint vshader = CompileShaderViaCode(vertexCode, GL_VERTEX_SHADER);
	GLuint tcshader = 0;
	GLuint teshader = 0;
#ifdef GL_TESS_EVALUATION_SHADER
	if (tessellationControlCode)
		tcshader = CompileShaderViaCode(tessellationControlCode, GL_TESS_CONTROL_SHADER);
	if (tessellationEvalCode)
		teshader = CompileShaderViaCode(tessellationEvalCode, GL_TESS_EVALUATION_SHADER);
#endif
	GLuint gshader = geometryCode? CompileShaderViaCode(geometryCode, GL_GEOMETRY_SHADER) : 0;
	GLuint pshader = CompileShaderViaCode(pixelCode, GL_FRAGMENT_SHADER);
	return LinkProgram(vshader, tcshader, teshader, gshader, pshader);
}

#ifndef __APPLE__
// **** Apple supports OpenGLv4.1, but compute shader not supported until v4.3

void LinkProgramViaCode(GLuint computeProgram, const char **computeCode) {
	GLuint computeShader = CompileShaderViaCode(computeCode, GL_COMPUTE_SHADER);
	glAttachShader(computeProgram, computeShader);
	glLinkProgram(computeProgram);
	glDetachShader(computeProgram, computeShader);
	glDeleteShader(computeShader);
	GLint status;
	glGetProgramiv(computeProgram, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) PrintProgramLog(computeProgram);
}

GLuint LinkProgramViaCode(const char **computeCode) {
	GLuint computeProgram = glCreateProgram();
	LinkProgramViaCode(computeProgram, computeCode);
	return computeProgram;
}

GLuint LinkProgramViaFile(const char *computeShaderFile) {
	GLuint cshader = CompileShaderViaFile(computeShaderFile, GL_COMPUTE_SHADER);
	if (!cshader)
		return 0;
	GLuint program = glCreateProgram();
	glAttachShader(program, cshader);
	glLinkProgram(program);
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) PrintProgramLog(program);
	return program;
}

// Binary Read/Write **** NOT SUPPORTED BY OPENGL3.x

void WriteProgramBinary(GLuint program, const char *filename) {
	GLenum binaryFormat = 0;
	GLsizei sizeBinary = 0, sizeEnum = sizeof(GLenum);
	glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &sizeBinary);
	std::vector<unsigned char> data(sizeBinary); //	std::vector<std::byte>
	glGetProgramBinary(program, sizeBinary, NULL, &binaryFormat, &data[0]);
	FILE *out = fopen(filename, "wb");
	fwrite(&binaryFormat, sizeEnum, 1, out);
	fwrite(&data[0], 1, sizeBinary, out);
	fclose(out);
}

bool ReadProgramBinary(GLuint program, const char *filename) {
	FILE *in = fopen(filename, "rb");
	if (in != NULL) {
		fseek(in, 0, SEEK_END);
		long filesize = ftell(in);
		size_t sizeEnum = sizeof(GLenum), sizeBinary = filesize-sizeEnum;
		std::vector<unsigned char> data(sizeBinary);
		GLenum binaryFormat;
		fseek(in, 0, 0);
		fread((char *) &binaryFormat, sizeEnum, 1, in);
		fread((char *) &data[0], 1, sizeBinary, in);
		fclose(in);
		glProgramBinary(program, binaryFormat, &data[0], sizeBinary);
		return true;
	}
	return false;
}

GLuint ReadProgramBinary(const char *filename) {
	GLuint program = glCreateProgram();
	if (!ReadProgramBinary(program, filename)) {
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

#endif // #ifndef __APPLE__

GLuint LinkProgram(GLuint vshader, GLuint pshader) {
	return LinkProgram(vshader, 0, 0, 0, pshader);
}

GLuint LinkProgram(GLuint vshader, GLuint tcshader, GLuint teshader, GLuint gshader, GLuint pshader) {
	GLuint program = 0;
	// create shader program
	if (vshader && pshader)
		program = glCreateProgram();
	if (program > 0) {
		// attach shaders to program
		glAttachShader(program, vshader);
		if (tcshader > 0) glAttachShader(program, tcshader);
		if (teshader > 0) glAttachShader(program, teshader);
		if (gshader > 0) glAttachShader(program, gshader);
		glAttachShader(program, pshader);
		// link and verify
		glLinkProgram(program);
		GLint status;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) PrintProgramLog(program);
	}
	return program;
}

GLuint LinkProgramViaFile(const char *vertexShaderFile, const char *pixelShaderFile) {
	GLuint vshader = CompileShaderViaFile(vertexShaderFile, GL_VERTEX_SHADER);
	GLuint fshader = CompileShaderViaFile(pixelShaderFile, GL_FRAGMENT_SHADER);
	return LinkProgram(vshader, fshader);
}

// Miscellany

int CurrentProgram() {
	int program = 0;
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	return program;
}

void DeleteProgram(GLuint program) {
	GLint nShaders = 0;
	GLuint shaderNames[10];
	glGetProgramiv(program, GL_ATTACHED_SHADERS, &nShaders);
	glGetAttachedShaders(program, nShaders, NULL, shaderNames);
	for (int i = 0; i < nShaders; i++)
		glDeleteShader(shaderNames[i]);
	glDeleteProgram(program);
}

// Uniform Access

bool squawk = false;

void SetReport(bool report) {
	squawk = report;
}

bool Bad(const char *name) {
	if (squawk)
		printf("can't find named uniform: %s\n", name);
	return false;
}

bool SetUniform(GLuint program, const char *name, bool val) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform1ui(id, val? 1 : 0);
	return true;
}

bool SetUniform(GLuint program, const char *name, int val) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform1i(id, val);
	return true;
}

// following might confuse some compilers
bool SetUniform(GLuint program, const char *name, GLuint val) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform1ui(id, val);
	return true;
}

bool SetUniformv(GLuint program, const char *name, int count, int *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform1iv(id, count, v);
	return true;
}

bool SetUniform(GLuint program, const char *name, float val) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform1f(id, val);
	return true;
}

bool SetUniformv(GLuint program, const char *name, int count, float *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform1fv(id, count, v);
	return true;
}

bool SetUniform(GLuint program, const char *name, vec2 v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform2f(id, v.x, v.y);
	return true;
}

bool SetUniform(GLuint program, const char *name, vec3 v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform3f(id, v.x, v.y, v.z);
	return true;
}

bool SetUniform(GLuint program, const char *name, vec4 v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform4f(id, v.x, v.y, v.z, v.w);
	return true;
}

bool SetUniform(GLuint program, const char *name, vec3 *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform3fv(id, 1, (float *) v);
	return true;
}

bool SetUniform(GLuint program, const char *name, vec4 *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform4fv(id, 1, (float *) v);
	return true;
}

bool SetUniform3(GLuint program, const char *name, float *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform3fv(id, 1, v);
	return true;
}

bool SetUniform2v(GLuint program, const char *name, int count, float *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform2fv(id, count, v);
	return true;
}

bool SetUniform3v(GLuint program, const char *name, int count, float *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform3fv(id, count, v);
	return true;
}

bool SetUniform4v(GLuint program, const char *name, int count, float *v) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniform4fv(id, count, v);
	return true;
}

bool SetUniform3v(GLuint program, const char *name, int count, float *v, mat4 m) {
	vec3 *v3 = (vec3 *) v;
	std::vector<vec3> xv(count);
	for (int i = 0; i < count; i++)
		xv[i] = Vec3(m*vec4(v3[i], 1));
	return SetUniform3v(program, name, count, (float *) xv.data());
}

bool SetUniform(GLuint program, const char *name, mat3 m) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniformMatrix3fv(id, 1, true, (float *) &m[0][0]);
	return true;
}

bool SetUniform(GLuint program, const char *name, mat4 m) {
	GLint id = glGetUniformLocation(program, name);
	if (id < 0)
		return Bad(name);
	glUniformMatrix4fv(id, 1, true, (float *) &m[0][0]);
	return true;
}

// Attribute Access

void DisableVertexAttribute(GLuint program, const char *name) {
	GLint id = glGetAttribLocation(program, name);
	if (id < 0 && squawk)
		printf("cant find attribute %s\n", name);
	if (id >= 0)
		glDisableVertexAttribArray(id);
}

int EnableVertexAttribute(GLuint program, const char *name) {
	GLint id = glGetAttribLocation(program, name);
	if (id < 0 && squawk)
		printf("cant find attribute %s\n", name);
	if (id >= 0)
		glEnableVertexAttribArray(id);
	return id;
}

void VertexAttribPointer(GLuint program, const char *name, GLint ncomponents, GLsizei stride, const GLvoid *offset) {
	GLint id = EnableVertexAttribute(program, name);
	if (id < 0 && squawk)
		printf("cant find attribute %s\n", name);
	if (id >= 0)
		glVertexAttribPointer(id, ncomponents, GL_FLOAT, GL_FALSE, stride, offset);
}
