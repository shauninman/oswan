#include "shared.h"

MENUITEM MainMenuItems[] = {
#if !defined(NOROMLOADER)
	{"Load ROM", NULL, 0, NULL, &menuFileBrowse},
#endif
	{"Continue", NULL, 0, NULL, &menuContinue},
	{"Reset", NULL, 0, NULL, &menuReset},
	{"Load State: ", (int16_t *) &GameConf.load_slot,  8, (int8_t *) &mnuSaves, &menuLoadState},
	{"Save State: ", (int16_t *) &GameConf.save_slot,  8, (char *) &mnuSaves, &menuSaveState},
	{"Show FPS: ", (int16_t *) &GameConf.m_DisplayFPS, 1, (int8_t *) &mnuOnOff, NULL},
	{"Quick Saves: ", (int16_t *) &GameConf.quicksave, 1, (int8_t *) &mnuOnOff, NULL},
	{"Input Map: ", (int16_t *) &GameConf.input_layout, 2, (int8_t *) &mnuABXY, NULL},

#if !defined(NATIVE_RESOLUTION)
	{"Rotate: ", (int16_t *) &GameConf.m_Rotate, 2, (int8_t *) &mnuOnOff, NULL},
	{"HScale: ", (int16_t *) &GameConf.m_ScreenRatioH, 3, (int8_t *) &mnuRatioH, NULL},
	{"VScale: ", (int16_t *) &GameConf.m_ScreenRatioV, 2, (int8_t *) &mnuRatioV, NULL},
#endif

#if !defined(NOSCREENSHOTS)
	{"Take Screenshot", NULL, 0, NULL, &menuSaveBmp},
#endif

	{"Exit", NULL, 0, NULL, &menuQuit}
};


MENU mnuMainMenu = { 
	13
	#if defined(NOROMLOADER)
	-1
	#endif
	#if defined(NOSCREENSHOTS)
	-1
	#endif
	#if defined(NATIVE_RESOLUTION)
	-3
	#endif
	,
	0, (MENUITEM *) &MainMenuItems };
