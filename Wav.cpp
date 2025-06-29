// Wav.cpp
// Copyright (c) 2024 Jules Bloomenthal, all rights reserved. Commercial use requires license.

#include <glad.h>
#include "Draw.h"
#include "Text.h"
#include "Wav.h"
#include "Widgets.h"
 
// read

Wav::Wav(string filename, bool verbose) {
	if (filename.length())
		Read(filename, verbose);
}

void PrintWAV(const char *name, int format) {
	char *c = (char *) &format;
	printf("%s: %c%c%c%c\n", name, c[0], c[1], c[2], c[3]);
}

bool Wav::Read(string filename, bool verbose) {
	// **** see https://docs.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex
	//		fmt and riff do not indicate possible "chunks" (additional cuts) following audio
	//		so, nSamples can be mistakenly high
	//		**** for accurate nSamples, advanced parse of headers needed
	//		**** or: import to Audacity, select all, export to .wav; then, reduce = 70 (see below) seems to work
	//      only uncompressed audio supported here
	FILE *in = fopen(filename.c_str(), "rb");
	if (!in) {
		printf("  ** no such file: %s\n", filename.c_str());
		return false;
	}
	struct RiffHeader {
		int fileID;			// should be "RIFF" (resource interchange file format)
		int fileSize;		// size of file in bytes less 8
		int typeID;			// should be "WAVE" (waveform audio file format)
	} r;
	struct FmtHeader {
		int waveID;			// should be "fmt"
		int headerSize;		// in bytes (less 4), should be 16 unless "extra" bytes
		short compression;
		short nChannels;
		int samplingRate;
		int aveBytesPerSec;
		short blockAlign;
		short sigBitsPerSamp;
	} fmt;
	int nRiffRead = fread(&r, sizeof(RiffHeader), 1, in);
	int nFmtRead = fread(&fmt, sizeof(FmtHeader), 1, in);
	if (nRiffRead != 1 || nFmtRead != 1) {
			printf("  ** can't read header (%s)\n", filename.c_str());
			fclose(in);
			return false;
	}
	if (verbose) {
		printf("%s:\n", filename.c_str());
		PrintWAV("  fileID", r.fileID);
		PrintWAV("  typeID", r.typeID);
		PrintWAV("  waveID", fmt.waveID);
		printf("  file size: %i\n", r.fileSize);
		printf("  header size: %i\n", fmt.headerSize);
		printf("  compression: %i\n", fmt.compression);
		printf("  # channels: %i\n", fmt.nChannels);
		printf("  sampling rate: %i\n", fmt.samplingRate);
		printf("  aveBytes/sec: %i\n", fmt.aveBytesPerSec);
		printf("  block align: %i\n", fmt.blockAlign);
		printf("  sigBits/samp: %i\n", fmt.sigBitsPerSamp);
		printf("  RiffHeader %i bytes, FmtHeader %i bytes\n", sizeof(RiffHeader), sizeof(FmtHeader));
	}
	// needed by Wav
	nChannels = fmt.nChannels;
	samplingRate = fmt.samplingRate;
	aveBytesPerSec = fmt.aveBytesPerSec;
	blockAlign = fmt.blockAlign;
	sigBitsPerSamp = fmt.sigBitsPerSamp;
	// validity checks
	if (nChannels < 1 || nChannels > 2)
		printf("  ** %i channels!\n", fmt.nChannels);
	if (sigBitsPerSamp != 16)
		printf("  ** %i bits per sample!\n", fmt.sigBitsPerSamp);
	if (blockAlign/nChannels != 2)
		printf("  ** blockAlign = %i!\n", fmt.blockAlign);
	if (fmt.headerSize > 16) {
		short nExtraFormatBytes;
		if (fread(&nExtraFormatBytes, 2, 1, in) == 1) {
			char buf[100];
			fread(buf, nExtraFormatBytes, 1, in);
			if (verbose) 
				printf("  ** %i extra bytes in Wave header\n", nExtraFormatBytes);
		}
	}
	// read samples
	int reduce = 70;
		// *** determined empirically to compensate for possible
		// *** "chunks" following audio data not indicated by fmt or riff
	int nSampleBytes = r.fileSize-sizeof(RiffHeader)-sizeof(FmtHeader);
	nSamples = nSampleBytes/(nChannels*sizeof(short))-reduce;
	size_t nShorts = nChannels*nSamples, bytesPerSample = nChannels*sizeof(short);
	samples.reserve(nShorts);
	samples.resize(nShorts);
	int nSamplesRead = fread((void *) samples.data(), bytesPerSample, nSamples, in);
	if (verbose)
		perror("  status");
	fclose(in);
	if (nSamplesRead < nSamples) {
		printf("** only read %i samples\n", nSamplesRead);
		nSamples = nSamplesRead;
	}
	samples.resize(nChannels*nSamples);
	if (verbose)
		printf("  %i 16-bit stereo samples (%3.2f secs)\n", nSamples, (float) nSamples/samplingRate);
	duration = (float) nSamples/samplingRate;
	return true;
}

// play

void PrintError(const char *title, MMRESULT r) {
	struct Msg { int n; const char *c; } messages[] = {
		{0, "<no error>"}, {MMSYSERR_ALLOCATED, "already allocated"}, {MMSYSERR_BADDEVICEID, "bad device id"}, {MMSYSERR_NODRIVER, "no device driver"},
		{MMSYSERR_NOMEM, "can't allocate/lock memory"}, {WAVERR_BADFORMAT, "unsupported format"}, {WAVERR_SYNC, "synch device, no WAVE_ALLOWSYNC"},
		{MMSYSERR_INVALHANDLE, "invalid device handle"}, {WAVERR_UNPREPARED, "data block unprepared"}
	};
	for (Msg m : messages)
		if (m.n == r) { printf("%s: %s\n", title, m.c); return; }
	printf("unknown error %i\n", r);
}

// int deviceState = WOM_CLOSE;	// WOM_CLOSE, WOM_OPEN, WOM_DONE
 
// void CALLBACK waveOutCallback(HWAVEOUT wo, UINT msg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
//	deviceState = msg;
// }

bool Wav::OpenDevice() {
	WAVEFORMATEX f;
	f.wFormatTag = WAVE_FORMAT_PCM;
	f.nChannels = nChannels;
	f.nSamplesPerSec = samplingRate;
	f.nAvgBytesPerSec = aveBytesPerSec;
	f.nBlockAlign = blockAlign;
	f.wBitsPerSample = sigBitsPerSamp;
	f.cbSize = 0;
	MMRESULT r1 = waveOutOpen(&waveOut, WAVE_MAPPER, &f, NULL, NULL, WAVE_FORMAT_DIRECT);
//	MMRESULT r1 = waveOutOpen(&waveOut, WAVE_MAPPER, &f, (DWORD_PTR) &waveOutCallback, NULL, WAVE_FORMAT_DIRECT | CALLBACK_FUNCTION);
	if (r1) PrintError("waveOutOpen", r1);
	waveOutSetVolume(waveOut, 0xFFFF<<16 | 0xFFFF); // reset volume to full
	return r1 == 0;
}

bool Wav::Play(int offset, int nsamps, float volume, bool reuseDevice) {
	// see https://docs.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex
	if (paused && accumulatedPlaytime > 0)
		return Resume();
	if (!waveOut || !reuseDevice) {
		if (!OpenDevice())
			return false;
	}
	paused = false;
	startPlay = clock();
	// deviceState = WOM_OPEN;
	wh.lpData = (char *) samples.data();
	wh.dwBufferLength = nsamps*(sigBitsPerSamp/8)*nChannels; // bytes
	wh.dwBytesRecorded = 0;
	wh.dwUser = NULL;
	wh.dwFlags = 0;
	wh.dwLoops = 1;
	wh.lpNext = 0;
	wh.reserved = 0;
	MMRESULT r2 = waveOutPrepareHeader(waveOut, &wh, sizeof(WAVEHDR));
	if (r2) PrintError("waveOutPrepareHeader", r2);
	// set volume, play
	SetVolume(volume);
	startPlay = clock();
	MMRESULT r3 = waveOutWrite(waveOut, &wh, sizeof(WAVEHDR));
	if (r3) PrintError("waveOutWrite", r3);
	return r2 == 0 && r3 == 0;
}

bool Wav::Play(float volume) {
	return Play(0, nSamples, volume, false);
}

bool Wav::Stop() {
	accumulatedPlaytime = 0;
	paused = true;
	return waveOutReset(waveOut) == 0;
}

bool Wav::Pause() {
	if (!paused)
		accumulatedPlaytime += TimeDif(clock(), startPlay);
	paused = true;
	return waveOutPause(waveOut) == 0;
}

bool Wav::Resume() {
	paused = false;
	startPlay = clock();
	return waveOutRestart(waveOut) == 0;
}

bool Wav::Loop(float volume, int nLoops) {
	// see https://docs.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex
	if (paused && accumulatedPlaytime > 0)
		return Resume();
	if (!waveOut) {
		if (!OpenDevice())
			return false;
	}
	paused = false;
	startPlay = clock();
	// deviceState = WOM_OPEN;
	wh.lpData = (char *) samples.data();
	wh.dwBufferLength = nSamples*(sigBitsPerSamp/8)*nChannels; // bytes
	wh.dwBytesRecorded = 0;
	wh.dwUser = NULL;
	wh.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
	wh.dwLoops = nLoops;
	wh.lpNext = 0;
	wh.reserved = 0;
	MMRESULT r2 = waveOutPrepareHeader(waveOut, &wh, sizeof(WAVEHDR));
	if (r2) PrintError("waveOutPrepareHeader", r2);
	// set volume, play
	SetVolume(volume);
	startPlay = clock();
	MMRESULT r3 = waveOutWrite(waveOut, &wh, sizeof(WAVEHDR));
	if (r3) PrintError("waveOutWrite", r3);
	return r2 == 0 && r3 == 0;
}

void Wav::SetVolume(float volume) {
	short w = (short)(volume*(float)0xFFFF);
	unsigned long dwVolume = w << 16 | w;
	waveOutSetVolume(waveOut, dwVolume);
}

float Wav::FractionPlayed() {
	// if (deviceState == WOM_DONE) return 1;
	float elapsed = accumulatedPlaytime+(paused? 0 : TimeDif(clock(), startPlay));
	if (elapsed < duration)
		return elapsed/duration;
//	paused = true;
//	accumulatedPlaytime = 0;
	return 1;
}

// display

void WavView::Set(int ax, int ay, int aw, int ah) {
	x = ax; y = ay; w = aw; h = ah;
	// vmin, vmax for display when w < nSamples
	vmin.assign(w, FLT_MAX);
	vmax.assign(w, -FLT_MAX);
	float xscale = (float)w/nSamples;
	float vvvmin = FLT_MAX, vvvmax = -FLT_MAX;
	for (int i = 0; i < nSamples; i++) {
		int k = channel == C_Mono? i : channel == C_Left? 2*i : 2*i+1;
		float v = (float) wav->samples[k]/32767.f;
		if (v < vvvmin) vvvmin = v;
		if (v > vvvmax) vvvmax = v;
		int id = (int) (xscale*i);
		if (v < vmin[id]) vmin[id] = v;
		if (v > vmax[id]) vmax[id] = v;
	}
}

WavView::WavView(int x, int y, int w, int h, Wav *wav, Channel ch) :
	wav(wav), channel(ch), nSamples(wav->nSamples) {
	Set(x, y, w, h);
}

void WavView::ChangeView(int x, int y, int w, int h) {
	Set(x, y, w, h);
}

void WavView::Display() {
	int4 vp = VPi();
	vec3 grn(0,.7f,0), cyn(0,.7f,.7f), brn(.5f, 0, 0), prp(1, 0, .5f), blu(0, 0, 1);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glViewport(x, y, w, h);
	UseDrawShader(mat4());
	if (w < nSamples)
		for (int i = 0; i < w; i++) {
			float t = (float) i/(w-1), xx = 2*t-1;
			Line(vec2(xx, vmin[i]), vec2(xx, vmax[i]), 1, grn);
		}
	else
		for (int i = 1; i < nSamples; i++) {
			int k1 = channel == C_Mono? i : channel == C_Left? 2*i : 2*i+1;
			int k0 = channel == C_Mono? k1-1 : k1-2;
			float x0 = (float)(i-1)/(nSamples-1), x1 = (float)i/(nSamples-1);
			float v0 = (float)wav->samples[k0]/32767.f, v1 = (float)wav->samples[k1]/32767.f;
			Line(vec2(2*x0-1, v0), vec2(2*x1-1, v1), 1, cyn);
		}
	glViewport(vp[0], vp[1], vp[2], vp[3]);
	UseDrawShader(ScreenMode());
	Quad(x, y, x, y+h, x+w, y+h, x+w, y, false, brn, 1, 2);
	Line(x, y+h/2, x+w, y+h/2, 2, prp);
	float f = wav->FractionPlayed();
	int xx = x+int((float)w*f);
	Line(xx, y, xx, y+h, wav->duration < 2? 3.f : 1.f, blu);
	const char *names[] = { "left", "right", "mono" };
	Text(x+10, y+10, vec3(0, 0, 0), 18, "%s", names[channel]);
}

/*	mmeapi.h: waveform audio function prototypes
	waveOutGetNumDevs			waveInGetNumDevs
	waveOutGetDevCapsA			waveInGetDevCapsA
	waveOutGetVolume			waveInGetErrorTextA
	waveOutSetVolume			waveInOpen
	waveOutGetErrorTextA		waveInClose
	waveOutOpenwaveOutClose		waveInPrepareHeader
	waveOutPrepareHeader		waveInUnprepareHeader
	waveOutUnprepareHeader		waveInAddBuffer
	waveOutWritewaveOutPause	waveInStart
	waveOutRestart				waveInStop
	waveOutReset				waveInReset
	waveOutBreakLoop			waveInGetPosition
	waveOutGetPosition			waveInGetID
	waveOutGetPitch				waveInMessage
	waveOutSetPitch
	waveOutGetPlaybackRate
	waveOutSetPlaybackRate
	waveOutGetID
	waveOutMessage */
