//cube_audio.c AUDIO output via libOGC

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version. See also the license.txt file for *
 *   additional informations.                                              *
 *                                                                         *
 ***************************************************************************/
#include "out.h"
#include "../psxcommon.h"

////////////////////////////////////////////////////////////////////////
// cube audio globals
////////////////////////////////////////////////////////////////////////
#include "../Gamecube/DEBUG.h"
#include <aesndlib.h>

#define MAX_OUT_DRIVERS 1

static struct out_driver out_drivers[MAX_OUT_DRIVERS];
struct out_driver *out_current;
static int driver_count;

#define REGISTER_DRIVER(d) { \
	extern void out_register_##d(struct out_driver *drv); \
	out_register_##d(&out_drivers[driver_count++]); \
}

char audioEnabled;

static const u32 freq = 44100;
unsigned int    iVolume = 3;
static AESNDPB* voice = NULL;
int	iDisStereo=0;

#define NUM_BUFFERS 4
#define BUFFERS_SIZE 32768
static struct { u8 buffer[BUFFERS_SIZE]; u32 len; } buffers[NUM_BUFFERS];
static u32 fill_buffer, play_buffer;

static void aesnd_callback(AESNDPB* voice, u32 state);


void SetVolume(void)
{
	// iVolume goes 1 (loudest) - 4 (lowest); volume goes 255-64
	u16 volume = (4 - iVolume + 1) * 64 - 1;
	if (voice) AESND_SetVoiceVolume(voice, volume, volume);
}

////////////////////////////////////////////////////////////////////////
// SETUP SOUND
////////////////////////////////////////////////////////////////////////

void SetupSound(void)
{
	int i;
	if (driver_count == 0) {
        REGISTER_DRIVER(cube);
	}

	out_drivers[0].init();

	//if (i < 0 || i >= driver_count) {
	//	printf("the impossible happened\n");
	//	abort();
	//}

	out_current = &out_drivers[0];
	//printf("selected sound output driver: %s\n", out_current->name);
}

void CubeSoundInit(void)
{
	voice = AESND_AllocateVoice(aesnd_callback);
	AESND_SetVoiceFormat(voice, iDisStereo ? VOICE_MONO16 : VOICE_STEREO16);
	AESND_SetVoiceFrequency(voice, freq);
	SetVolume();
	AESND_SetVoiceStream(voice, true);
	fill_buffer = play_buffer = 0;
}

////////////////////////////////////////////////////////////////////////
// REMOVE SOUND
////////////////////////////////////////////////////////////////////////

void RemoveSound(void)
{
	AESND_SetVoiceStop(voice, true);
}

////////////////////////////////////////////////////////////////////////
// GET BYTES BUFFERED
////////////////////////////////////////////////////////////////////////

unsigned long SoundGetBytesBuffered(void)
{
	unsigned long bytes_buffered = 0, i = fill_buffer;
	while(1) {
		bytes_buffered += buffers[i].len;

		if(i == play_buffer) break;

        // upd xjsxjs197 start
		//i = (i + NUM_BUFFERS - 1) % NUM_BUFFERS;
		i = (i + NUM_BUFFERS - 1) & 3;
		// upd xjsxjs197 end
	}

	return bytes_buffered;
}

static int SoundBusy(void) {
	if (SoundGetBytesBuffered() > 8*1024)
    {
        return 1;
    }

	return 0;
}

static void aesnd_callback(AESNDPB* voice, u32 state){
	if(state == VOICE_STATE_STREAM) {
		if(play_buffer != fill_buffer) {
			AESND_SetVoiceBuffer(voice,
					buffers[play_buffer].buffer, buffers[play_buffer].len);
            // upd xjsxjs197 start
			//play_buffer = (play_buffer + 1) % NUM_BUFFERS;
			play_buffer = (play_buffer + 1) & 3;
			// upd xjsxjs197 end
		}
	}
}

////////////////////////////////////////////////////////////////////////
// FEED SOUND DATA
////////////////////////////////////////////////////////////////////////
void SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
	if(!audioEnabled) return;

	//buffers[fill_buffer].buffer = pSound;
	memcpy(buffers[fill_buffer].buffer, pSound, lBytes);
	buffers[fill_buffer].len = lBytes;
	// upd xjsxjs197 start
	//fill_buffer = (fill_buffer + 1) % NUM_BUFFERS;
	fill_buffer = (fill_buffer + 1) & 3;
	// upd xjsxjs197 end

	AESND_SetVoiceStop(voice, false);
}

void pauseAudio(void){
	AESND_Pause(true);
}

void resumeAudio(void){
	AESND_Pause(false);
}

void out_register_cube(struct out_driver *drv)
{
	drv->name = "cube";
	drv->init = CubeSoundInit;
	drv->finish = RemoveSound;
	drv->busy = SoundBusy;
	drv->feed = SoundFeedStreamData;
}
