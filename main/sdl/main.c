#ifdef DREAMCAST
#include <kos.h>
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#ifdef HOME_SUPPORT
#include <unistd.h>
#endif

#include "shared.h"
#include "drawing.h"
#include "game_input.h"

#include <dlfcn.h>
#include <mmenu.h>
static void* mmenu = NULL;
char rom_path[512];
char save_path[512];

#ifndef NO_WAIT
void msleep(uint8_t milisec);
#endif
void exit_oswan();

extern void mixaudioCallback(void *userdata, uint8_t *stream, int32_t len);

uint32_t m_Flag;
uint32_t interval;

gamecfg GameConf;
int8_t gameName[512];
int8_t current_conf_app[MAX__PATH];

uint32_t nextTick, lastTick = 0, newTick, currentTick, wait;
uint8_t FPS = 60; 
uint8_t pastFPS = 0; 

SDL_Surface *actualScreen;
#if !defined(NOSCREENSHOTS)
SDL_Surface *screenshots;
#endif

#ifdef FRAMESKIP
extern int32_t FrameSkip;
#endif

uint32_t SDL_UXTimerRead(void) 
{
	struct timeval tval; /* timing	*/
  	gettimeofday(&tval, 0);
	return (((tval.tv_sec*1000000) + (tval.tv_usec )));
}

void graphics_paint(void) 
{
	screen_draw();
	pastFPS++;
	newTick = SDL_UXTimerRead();
	if ((newTick-lastTick)>1000000) 
	{
		FPS = pastFPS;
		pastFPS = 0;
		lastTick = newTick;
	}
//	#ifdef FRAMESKIP
//	FrameSkip = 80 - FPS;
//	if (FrameSkip < 0) FrameSkip = 0;
//	else if (FrameSkip > 4) FrameSkip=4;
//	#endif
}

void initSDL(void) 
{
	/* Get current resolution, does nothing on Windowed or bare metal platroms */
	Get_resolution();
	SetVideo(0);

#ifdef SOUND_ON
	SDL_Init(SDL_INIT_AUDIO);
	SDL_AudioSpec fmt, retFmt;
	
	/*	Set up SDL sound */
	fmt.freq = 24000;		// 24kHz
	fmt.samples = 512;		// Shorter to prevent sound delay
	fmt.format = AUDIO_S16SYS;
	fmt.channels = 1;		// Mono
	fmt.callback = mixaudioCallback;
	fmt.userdata = NULL;

    /* Open the audio device and start playing sound! */
    if ( SDL_OpenAudio(&fmt, &retFmt) < 0 )
	{
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        printf("Exiting Oswan...\n");
        exit(1);
    }
#endif
}


int main(int argc, char *argv[]) 
{
#ifdef NOROMLOADER
	if (argc < 2) return 0;	
#endif

#ifdef GECKO
	fatInitDefault();
	WPAD_Init();
#endif	

	m_Flag = GF_MAINUI;
	
	mmenu = dlopen("libmmenu.so", RTLD_LAZY);
	
	/* Init graphics & sound */
	initSDL();
	
#ifdef JOYSTICK
	SDL_Init(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_ENABLE);
#endif

	getcwd(current_conf_app, MAX__PATH);
	
#if defined(HOME_SUPPORT)
	int8_t home_path[256];
	snprintf(home_path, sizeof(home_path), "%s/.oswan", PATH_DIRECTORY);
	/* 
	 * If folder does not exists then create it 
	 * This can speeds up startup if the folder already exists
	*/
	if(access( home_path, F_OK ) == -1) 
	{
		mkdir(home_path, 0755);	
	}
	snprintf(current_conf_app, sizeof(current_conf_app), "%s/.oswan/oswan.cfg", PATH_DIRECTORY);
#else
	snprintf(current_conf_app, sizeof(current_conf_app), "%soswan.cfg%s", PATH_DIRECTORY, EXTENSION);
#endif
	
	system_initcfg();
	system_loadcfg(current_conf_app);

    /*	load rom file via args if a rom path is supplied	*/
	strcpy(gameName,"");
	
	#ifdef _TINSPIRE
		clear_cache();
	#endif
	
	if(argc > 1) 
	{
#ifdef NATIVE_RESOLUTION
		SetVideo(1);
#endif
		flip_screen(actualScreen);
		snprintf(gameName, sizeof(gameName) ,"%s", argv[1]);
		m_Flag = GF_GAMEINIT;
		
		strcpy(rom_path, gameName);
		
		// based on code from menu.c:menuSaveState() and WSFileio.c:WsSaveState()
		int8_t savename[512];
		strcpy(savename, gameName);
		strcpy(strrchr(savename, '.'), ".sta");
		snprintf(save_path, 512, "%s%s%s.%%i.sta", PATH_DIRECTORY, SAVE_DIRECTORY, strrchr(savename,'/')+1);
	}

	while (m_Flag != GF_GAMEQUIT) 
	{
		switch (m_Flag) 
		{
			case GF_MAINUI:
				#ifdef SOUND_ON
				SDL_PauseAudio(1);
				#endif
				if (mmenu) {
					// prevents opening MinUI menu twice
					for (int i=0; i<18; i++) {
						button_state[i] = 0;
					}
					
					ShowMenu_t ShowMenu = (ShowMenu_t)dlsym(mmenu, "ShowMenu");
					MenuReturnStatus status = ShowMenu(rom_path, save_path, actualScreen, kMenuEventKeyDown);
					
					if (status==kStatusExitGame) {
						menuQuit();
					}
					else if (status==kStatusOpenMenu) {
						// prevents emulator menu from being dismissed immediately
						Reset_Controls(); 
						screen_showtopmenu();
					}
					else if (status>=kStatusLoadSlot) {
						int slot = status - kStatusLoadSlot;
						GameConf.save_slot = slot;
						GameConf.load_slot = slot;
						menuLoadState();
					}
					else if (status>=kStatusSaveSlot) {
						int slot = status - kStatusSaveSlot;
						GameConf.save_slot = slot;
						GameConf.load_slot = slot;	
						menuSaveState();
					}
					
					if (status!=kStatusExitGame && status!=kStatusOpenMenu) {
						menuContinue();
					}
					
					SDL_FillRect(actualScreen, NULL, 0);
					SDL_Flip(actualScreen);
				}
				else {
					screen_showtopmenu();
				}
				#ifdef SOUND_ON
				if (cartridge_IsLoaded()) SDL_PauseAudio(0);
				#endif
				#ifndef NO_WAIT
				nextTick = SDL_UXTimerRead() + interval;
				#endif
				break;

			case GF_GAMEINIT:
			
				Set_DrawRegion();
				
				if (WsCreate(gameName)) 
				{
					system_saveloadgamecfg(0);
					WsInit();
					m_Flag = GF_GAMERUNNING;
					#ifdef SOUND_ON
					SDL_PauseAudio(0);
					#endif
					#ifndef NO_WAIT
					/* Init timing */
					interval = (1.0 / 60) * 1000000;
					nextTick = SDL_UXTimerRead() + interval;
					#endif
				}
				else 
				{
					fprintf(stderr,"Can't load %s", gameName); 
					fflush(stderr);
					m_Flag = GF_GAMEQUIT;
				}
				break;
		
			case GF_GAMERUNNING:	
				#ifndef NO_WAIT
				currentTick = SDL_UXTimerRead(); 
				wait = (nextTick - currentTick);
				if (wait > 0) 
				{
					if (wait < 1000000) 
					{
						msleep(wait/1000);
					}
				}
				nextTick += interval;
				#endif
				exit_button();
				WsRun();
				break;
		}
	}
	
	exit_oswan();
	return 0;
}

void exit_oswan()
{
	#ifdef SOUND_ON
		SDL_PauseAudio(1);
	#endif

	/* Free memory	*/
	#ifdef _TINSPIRE
		deinitBuffering();
	#else
		
	#ifndef NOSCREENSHOTS
		if (screenshots != NULL) SDL_FreeSurface(screenshots);
	#endif
		if (actualScreen != NULL) SDL_FreeSurface(actualScreen);
	#if defined(SCALING)
		if (real_screen != NULL) SDL_FreeSurface(real_screen);
	#endif
		SDL_QuitSubSystem(SDL_INIT_VIDEO);	
	#ifdef SOUND_ON
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	#endif
		SDL_Quit();
	#endif
}

#ifndef NO_WAIT
void msleep(uint8_t milisec)
{
/* 
 * nanosleep is better in every way.
 * Only use SDL_Delay as a last resort. 
*/
#ifdef POSIX
	struct timespec req={0};
	time_t sec=(uint16_t)(milisec/1000);

	milisec=milisec-(sec*1000);
	req.tv_sec=sec;
	req.tv_nsec=milisec*1000000L;

	while(nanosleep(&req,&req)==-1)
	continue;
#else
	SDL_Delay(milisec);
#endif
}
#endif

