#include "shared.h"

typedef struct {
	int8_t itemName[16];
	int16_t *itemPar;
	int16_t itemParMaxValue;
	int8_t *itemParName;
	void (*itemOnA)();
} MENUITEM;

typedef struct {
	int32_t itemNum; /* number of items	*/
	int32_t itemCur; /* current item	*/
	MENUITEM *m; /* array of items	*/
} MENU;


void clear_screen_menu(void);
void draw_bluerect_menu(uint8_t i);
void draw_bluerect_file(uint8_t i);

void menuReset(void);
void menuQuit(void);
void menuContinue(void);
void menuFileBrowse(void);
void menuSaveState(void);
void menuLoadState(void);
void screen_showkeymenu(void);
uint8_t ifactive(void);
#if !defined(NOSCREENSHOTS)
void menuSaveBmp(void);
#endif

int8_t mnuABXY[2][16] = {"H-Mode", "V-Mode"};
int8_t mnuYesNo[2][16] = {"No", "Yes"};
int8_t mnuSaves[10][16] = { "1","2","3","4","5","6","7","8","9"};
int8_t mnuRatio[5][16] = { "Native","Fullscreen", "Aspect","Rotate","RotateWide"};

#ifdef _TINSPIRE
const int8_t *file_ext[] = { 
	(const int8_t *) ".tns",
	NULL };
#else
const int8_t *file_ext[] = { 
	(const int8_t *) ".ws",  (const int8_t *) ".wsc", (const int8_t *) ".bin", 
#ifdef ZIP_SUPPORT  
	(const int8_t *) ".zip",
#endif
	NULL };
#endif


