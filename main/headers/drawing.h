#include "shared.h"

void screen_draw(void);
void Get_resolution(void);
void Set_resolution(uint16_t w, uint16_t h);
void SetVideo(uint8_t mode);
void Set_DrawRegion(void);
void take_screenshot(void);
void upscale_224x144_to_320xXXX(uint16_t *dst, uint16_t *src, uint32_t height);
void upscale_144x224_to_320x240_rotate(uint16_t *dst, uint16_t *src);
void upscale_15x_sharp(uint16_t *dst, uint16_t *src);
void upscale_15x_sharp2(uint16_t *dst, uint16_t *src);
//void upscale_15x_scanline(uint16_t *dst, uint16_t *src);

extern uint8_t FPS;

struct wheretodraw
{
	uint16_t w;
	uint16_t h;
	uint16_t offset_x;
	uint16_t offset_y;
} screen_to_draw_region;

#if !defined(SCALING) && !defined(_TINSPIRE)
	#define flip_screen SDL_Flip
#elif defined(_TINSPIRE)
	void flip_screen(SDL_Surface* screen);
#else
	void flip_screen(SDL_Surface* screen);
	SDL_Surface *real_screen;
#endif

#if !defined(_TINSPIRE)
SDL_Rect screen_position;
#endif

struct scaling
{
	uint16_t w_display;
	uint16_t h_display;
	float w_scale;
	float h_scale;
	float w_scale_size;
	float h_scale_size;
} screen_scale;
