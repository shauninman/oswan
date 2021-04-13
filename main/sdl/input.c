#include "shared.h"

/*
0: PAD_LEFT;	
1: PAD_RIGHT;	
2: PAD_UP;
3: PAD_DOWN;
4: PAD_A;	
5: PAD_B;
6: PAD_X;	
7: PAD_Y;
8: PAD_L;	
9: PAD_R;
10: PAD_START;	
11: PAD_SELECT;
12: PAD_QUIT;
13: PAD_SLIDER;
14: PAD_XLEFT;
15: PAD_XRIGHT;
16: PAD_XUP;
17: PAD_XDOWN;
*/

uint32_t	button_state[18];
uint32_t	button_avoid[18];

#ifdef JOYSTICK
	SDL_Joystick* joystick_sdl[2];
	int16_t joystick_axies[4] = {0, 0, 0, 0};
	#define JOYSTICK_UP (joystick_axies[1] < -2048 ? 1 : 0)
	#define JOYSTICK_RIGHT	(joystick_axies[0] > 2048 ? 1 : 0)
	#define JOYSTICK_LEFT	(joystick_axies[0] < -2048 ? 1 : 0)
	#define JOYSTICK_DOWN (joystick_axies[1] > 2048 ? 1 : 0)

const	uint32_t paddata[18][3] = {
	{PAD_LEFT,	16,	JOYSTICK_LEFT},
	{PAD_RIGHT,	16,	JOYSTICK_RIGHT},
	{PAD_UP,	16,	JOYSTICK_UP},
	{PAD_DOWN,	16,	JOYSTICK_DOWN},
	{PAD_A,		2,	0},
	{PAD_B,		1,	0},
	{PAD_X,		0,	0},
	{PAD_Y,		3,	0},
	{PAD_L,		4,	0},
	{PAD_R,		5,	0},
	{PAD_START,	9,	0},
	{PAD_SELECT,	8,	0},
	{PAD_QUIT,	16,	0},
	{PAD_SLIDER,	16,	0},
	{PAD_XLEFT,	16,	0},
	{PAD_XRIGHT,	16,	0},
	{PAD_XUP,	16,	0},
	{PAD_XDOWN	16,	0}
	};
#else
const	uint32_t paddata[18] = {
	PAD_LEFT,PAD_RIGHT,PAD_UP,PAD_DOWN,
	PAD_A,PAD_B,PAD_X,PAD_Y,
	PAD_L,PAD_R,PAD_START,PAD_SELECT,
	PAD_QUIT,PAD_SLIDER,
	PAD_XLEFT,PAD_XRIGHT,PAD_XUP,PAD_XDOWN
	};
#endif

/* Uses button_state global */
void Buttons(void)
{
	uint8_t i = 0;
#if defined(_TINSPIRE)
	t_key pad;
	#define CHECK_PAD isKeyPressed(pad)
#else
#ifdef JOYSTICK
	int32_t pad, pad2, pad3;
	if (SDL_NumJoysticks() > 0)
		joystick_sdl[0] = SDL_JoystickOpen(0);

	for(i=0;i<4;i++)
		joystick_axies[i] = SDL_JoystickGetAxis(joystick_sdl[0], i);

	SDL_JoystickUpdate();
	#define CHECK_PAD (keys[pad] || SDL_JoystickGetButton(joystick_sdl[0], pad2) || (pad3))
#else
	int32_t pad;
	#define CHECK_PAD keys[pad]
#endif
	SDL_Event event;
	SDL_PollEvent(&event);
	uint8_t *keys = SDL_GetKeyState(NULL);
#endif

	for(i=0;i<18;i++)
	{
#ifdef	JOYSTICK
		pad = paddata[i][0];
		pad2 = paddata[i][1];
		pad3 = paddata[i][2];
#else
		pad = paddata[i];
#endif
		switch (button_state[i])
		{
			case 0:
				if (CHECK_PAD) {
					/* To avoid for the button for being pressed again */
					if (!(button_avoid[i])) button_state[i] = 1;
				} else	button_avoid[i] = 0;
			break;
			
			case 1:
				if (CHECK_PAD) button_state[i] = 2; else button_state[i] = 0;
			break;
			
			case 2:
				if (!(CHECK_PAD)) button_state[i] = 0;
			break;
		}
	
	}

}
