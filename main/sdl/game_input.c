#include <string.h>

#include "game_input.h"
#include "drawing.h"
#include "shared.h"

#include "WS.h"
#include "WSFileio.h"

void exit_button(void)
{
	/* ESC -> MENU UI */
	if (button_state[12])
	{
		m_Flag = GF_MAINUI;
		take_screenshot();
		/* HACK - FIX ME*/
#ifdef GCW
		if (GameConf.m_ScreenRatio == 2) SetVideo(0);
#endif
	}
}

int32_t WsInputGetState(void)
{
	/*
	 * 0 = Up  (Y1)
	 * 1 = Right (Y1)
	 * 2 = Down (Y1)
	 * 3 = Left (Y1)
	 * 4 = Up  (X1)
	 * 5 = Right (X1)
	 * 6 = Down (X1)
	 * 7 = Left (X1)
	 * 8 = START
	 * 9 = OPTION
	 * 10 = A 
	 * 11 = B
	*/
	int8_t szFile[256];
	int32_t button = 0;
	
	Buttons();

	/* If Quick Saves are enabled */
	if (GameConf.quicksave)
	{
		/* Save (L button)	*/
		if (button_state[8] == 1)
		{
			strcpy(szFile, gameName);
			#ifdef _TINSPIRE
			strcpy(strrchr(szFile, '.'), ".sta.tns");
			#else
			strcpy(strrchr(szFile, '.'), ".sta");
			#endif
			WsSaveState(szFile, GameConf.save_slot);
		}
		/* Load (R button)	*/
		else if (button_state[9] == 1)
		{
			strcpy(szFile, gameName);
			#ifdef _TINSPIRE
			strcpy(strrchr(szFile, '.'), ".sta.tns");
			#else
			strcpy(strrchr(szFile, '.'), ".sta");
			#endif
			WsLoadState(szFile, GameConf.load_slot);
		}
	}
	
	if ( (GameConf.input_layout == 0) || ((GameConf.input_layout == 2)&&(HVMode == 0)&&(GameConf.m_Rotate != 1)) ) {
		// H-Mode
		//	PAD_RIGHT(1)	-> XR(5)
		button |= button_state[1] ? (1<<5) : 0; 
		//	PAD_LEFT(0)	-> XL(7)
		button |= button_state[0] ? (1<<7) : 0; 
		//	PAD_DOWN(3)	-> XD(6)
		button |= button_state[3] ? (1<<6) : 0; 
		//	PAD_UP(2)	-> XU(4)
		button |= button_state[2] ? (1<<4) : 0; 
		//	PAD_R(9)	-> YR(1)
		button |= button_state[9] ? (1<<1) : 0; 
		//	PAD_L(8)	-> YL(3)
		button |= button_state[8] ? (1<<3) : 0; 
		//	PAD_Y(7)	-> YD(2)
		button |= button_state[7] ? (1<<2) : 0; 
		//	PAD_X(6)	-> YU(0)
		button |= button_state[6] ? (1<<0) : 0; 
		
		//	PAD_A(4)	-> A(10)
		button |= button_state[4] ? (1<<10) : 0; 
		//	PAD_B(5)	-> B(11)
		button |= button_state[5] ? (1<<11) : 0; 
	} else {
		// V-Mode
		if ( (GameConf.m_Rotate == 0) || ((GameConf.m_Rotate == 2)&&(HVMode == 0)) ) {
			// Normal Scaler
			//	PAD_RIGHT(1)	-> YR(1)
			button |= button_state[1] ? (1<<1) : 0; 
			//	PAD_LEFT(0)	-> YL(3)
			button |= button_state[0] ? (1<<3) : 0; 
			//	PAD_DOWN(3)	-> YD(2)
			button |= button_state[3] ? (1<<2) : 0; 
			//	PAD_UP(2)	-> YU(0)
			button |= button_state[2] ? (1<<0) : 0; 
			//	PAD_A(4)	-> XR(5)
			button |= button_state[4] ? (1<<5) : 0; 
			//	PAD_Y(7)	-> XL(7)
			button |= button_state[7] ? (1<<7) : 0; 
			//	PAD_B(5)	-> XD(6)
			button |= button_state[5] ? (1<<6) : 0; 
			//	PAD_X(6)	-> XU(4)
			button |= button_state[6] ? (1<<4) : 0; 
		} else {
			// Rotate Scaler
			//	PAD_UP(2)	-> YR(1)
			button |= button_state[2] ? (1<<1) : 0; 
			//	PAD_DOWN(3)	-> YL(3)
			button |= button_state[3] ? (1<<3) : 0; 
			//	PAD_RIGHT(1)	-> YD(2)
			button |= button_state[1] ? (1<<2) : 0; 
			//	PAD_LEFT(0)	-> YU(0)
			button |= button_state[0] ? (1<<0) : 0; 
			//	PAD_X(6)	-> XR(5)
			button |= button_state[6] ? (1<<5) : 0; 
			//	PAD_B(5)	-> XL(7)
			button |= button_state[5] ? (1<<7) : 0; 
			//	PAD_A(4)	-> XD(6)
			button |= button_state[4] ? (1<<6) : 0; 
			//	PAD_Y(7)	-> XU(4)
			button |= button_state[7] ? (1<<4) : 0; 
		}
		//	PAD_R(9)	-> A(10)
		button |= button_state[9] ? (1<<10) : 0; 
		//	PAD_L(8)	-> B(11)
		button |= button_state[8] ? (1<<11) : 0; 
	}
	// START BUTTON				PAD_START
	button |= button_state[10] ? (1<<9) : 0; 

	return button;
}
