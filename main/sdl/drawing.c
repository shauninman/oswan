#include "drawing.h"
#ifdef SCALING
#include "gfx/SDL_rotozoom.h"
#endif
#include "WS.h"

void Get_resolution(void)
{
#ifdef SCALING
	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	screen_scale.w_display = info->current_w;
	screen_scale.h_display = info->current_h;
#else
	screen_scale.w_display = 320;
	screen_scale.h_display = 240;
#endif
}

void Set_resolution(uint16_t w, uint16_t h)
{
#ifdef SCALING
	screen_scale.w_scale = screen_scale.w_display / w;
	screen_scale.h_scale = screen_scale.h_display / h;
  
	screen_scale.w_scale_size = screen_scale.w_scale * w;
	screen_scale.h_scale_size = screen_scale.h_scale * h;
	
	screen_position.x = (screen_scale.w_display - screen_scale.w_scale_size)/2;
	screen_position.y = (screen_scale.h_display - screen_scale.h_scale_size)/2;	
#else
	screen_scale.w_display = w;
	screen_scale.h_display = h;
#endif

}

void SetVideo(uint8_t mode)
{
#ifdef SCALING
	int32_t flags = FLAG_VIDEO | SDL_NOFRAME | SDL_FULLSCREEN;
#else
	int32_t flags = FLAG_VIDEO;
#endif
	uint16_t w = 320, h = 240;
	
	if (mode == 1) 
	{
		w = 224;
		h = 144;
	}
	
	if (!SDL_WasInit(SDL_INIT_VIDEO)) 
	{	
		SDL_Init(SDL_INIT_VIDEO);
		SDL_ShowCursor(SDL_DISABLE);
	}
	
	#if defined(SCALING)
		if (real_screen) SDL_FreeSurface(real_screen);
		if (actualScreen) SDL_FreeSurface(actualScreen);
	#else
		if (actualScreen) SDL_FreeSurface(actualScreen);
	#endif
	
	#if !defined(NOSCREENSHOTS)
		if (screenshots) SDL_FreeSurface(screenshots);
	#endif
	
	Set_resolution(w, h);

	#if defined(SCALING)
		real_screen = SDL_SetVideoMode(screen_scale.w_display, screen_scale.h_display, BITDEPTH_OSWAN, flags);
		actualScreen = SDL_CreateRGBSurface(FLAG_VIDEO, w, h, BITDEPTH_OSWAN, 0,0,0,0);
	#else
		actualScreen = SDL_SetVideoMode(screen_scale.w_display, screen_scale.h_display, BITDEPTH_OSWAN, flags);
	#endif
	
	#if !defined(NOSCREENSHOTS)
		screenshots = SDL_CreateRGBSurface(0, w, h, BITDEPTH_OSWAN, 0,0,0,0);
	#endif
}

void Set_DrawRegion(void)
{
	/* Clear screen too to avoid graphical glitches */
	SDL_FillRect(actualScreen, NULL, 0);

	if ( (GameConf.m_Rotate == 0) || ((GameConf.m_Rotate == 2)&&(HVMode == 0)) ) {
		switch (GameConf.m_ScreenRatioH) {
			case 0:		// Native
				screen_to_draw_region.w	= 224;
				screen_to_draw_region.h	= 144;
#ifdef NATIVE_RESOLUTION
				screen_to_draw_region.offset_x = 0;
				screen_to_draw_region.offset_y = 0; 
#else
				screen_to_draw_region.offset_x = ((actualScreen->w - SYSVID_WIDTH)/2);
				screen_to_draw_region.offset_y = ((actualScreen->h - SYSVID_HEIGHT)/2); 
#endif
				break;
			case 1:		// Fullscreen
				screen_to_draw_region.w	= 320;
				screen_to_draw_region.h	= 240;
				screen_to_draw_region.offset_x = 0;
				screen_to_draw_region.offset_y = 0; 
				break;
			case 2:		// Aspect
				screen_to_draw_region.w	= 320;
				screen_to_draw_region.h	= 204;
				screen_to_draw_region.offset_x = 0;
				screen_to_draw_region.offset_y = 18; 
				break;
			case 3:		// 1.5x Sharp
				screen_to_draw_region.w	= 320;
				screen_to_draw_region.h	= 216;
				screen_to_draw_region.offset_x = 0;
				screen_to_draw_region.offset_y = 12;
				break;
		}
	} else {
		switch (GameConf.m_ScreenRatioV) {
			case 0:		// Rotate
				screen_to_draw_region.w	= 144;
				screen_to_draw_region.h	= 224;
#ifdef NATIVE_RESOLUTION
				screen_to_draw_region.offset_x = 0;
				screen_to_draw_region.offset_y = 0; 
#else
				screen_to_draw_region.offset_x = ((actualScreen->w - SYSVID_HEIGHT)/2);
				screen_to_draw_region.offset_y = ((actualScreen->h - SYSVID_WIDTH)/2); 
#endif
				break;
			case 1:		// RotateFull
				screen_to_draw_region.w	= 320;
				screen_to_draw_region.h	= 240;
				screen_to_draw_region.offset_x = 0;
				screen_to_draw_region.offset_y = 0;
				break;
			case 2:		// RotateWide
				screen_to_draw_region.w	= 288;
				screen_to_draw_region.h	= 224;
				screen_to_draw_region.offset_x = 16;
				screen_to_draw_region.offset_y = 8;
				break;
		}
	}
}

void screen_draw(void)
{
	uint16_t *src = (uint16_t *) FrameBuffer +8;	// +8 offset
	uint16_t *dst = (uint16_t *) actualScreen->pixels + screen_to_draw_region.offset_x + screen_to_draw_region.offset_y * 320;
	uint32_t x , y;
	SDL_LockSurface(actualScreen);

	if ( (GameConf.m_Rotate == 0) || ((GameConf.m_Rotate == 2)&&(HVMode == 0)) ) {
		// Normal Scalers
		switch (GameConf.m_ScreenRatioH) {
			case 0:		// Native	224x144
				for(y = 0; y < 144; y++, src += 320, dst += 320) memcpy(dst, src, 224*2);
				break;
			case 1:		// Fullscreen	224x144 > 320x240	7x3 > 10x5
				upscale_224x144_to_320xXXX(dst, src, 240);
				break;
			case 2:		// Aspect	224x144 > 320x204
				upscale_224x144_to_320xXXX(dst, src, 204);
				break;
			case 3: // 1.5x Sharp 336x216 (-8,12)
				upscale_15x_sharp(actualScreen->pixels, src);
				break;
		}
	} else {
		// Rotate Scalers
		src += (224-1);		// draw from Right-Top
		switch (GameConf.m_ScreenRatioV) {
			case 0:		// Rotate	224x144 > 144x224
				for( y = 0; y < 224; y++) {
					for ( x = 0; x < 144/2; x++) {
						*(uint32_t*)dst = *src|(*(src+320)<<16);
						dst += 2; src += 320*2;
					}
					dst += (320 - 144);
					src -= (320 * 144) + 1;
				}
				break;
			case 1:		// RotateFull	224x144 > 320x240
				upscale_144x224_to_320x240_rotate(dst, src);
				break;
			case 2:		// RotateWide	224x144 > 288x224
				for( y = 0; y < 224; y++) {
					for ( x = 0; x < 144; x++) {
						*(uint32_t*)dst = *src|(*src<<16);
						dst += 2; src += 320;
					}
					dst += (320 - 288);
					src -= (320 * 144) + 1;
				}
				break;
		}
	}

	static char buffer[4];
	if (GameConf.m_DisplayFPS) 
	{
		if (screen_to_draw_region.offset_y)
		{
			SDL_Rect rect;
			rect.x = 0;
			rect.y = 0;
			rect.w = 16;
			rect.h = 8;
			SDL_FillRect(actualScreen, &rect, 0);
		}
		sprintf(buffer,"%d",FPS);
		print_string_video(1,1,buffer);
	}
	
	SDL_UnlockSurface(actualScreen);
	flip_screen(actualScreen);
}

#if defined(SCALING)
void flip_screen(SDL_Surface* screen)
{
	SDL_SoftStretch(actualScreen, NULL, real_screen, &screen_position);
	SDL_Flip(real_screen);
}
#endif

void take_screenshot(void)
{
#if !defined(NOSCREENSHOTS)
	/* Save current screen in screenshots's layer */
	SDL_BlitSurface(actualScreen, NULL, screenshots, NULL);
#endif
}

#define AVERAGE16(c1, c2) (((c1) + (c2) + (((c1) ^ (c2)) & 0x0821))>>1)  //More accurate

void upscale_224x144_to_320xXXX(uint16_t *dst, uint16_t *src, uint32_t height)
{
    register uint_fast16_t a, b, c, d, e, f, g;
    int Eh = 0;
    int vf = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < 320/10; x++)
        {
            a = *(src+0);
            b = *(src+1);
            c = *(src+2);
            d = *(src+3);
            e = *(src+4);
            f = *(src+5);
            g = *(src+6);

            if(vf == 1){
                a = AVERAGE16(a, *(src+320));
                b = AVERAGE16(b, *(src+321));
                c = AVERAGE16(c, *(src+322));
                d = AVERAGE16(d, *(src+323));
                e = AVERAGE16(e, *(src+324));
                f = AVERAGE16(f, *(src+325));
                g = AVERAGE16(g, *(src+326));
            }

            *(uint32_t*)(dst+0) = a|(b<<16);
            *(uint32_t*)(dst+2) = AVERAGE16(b,c)|(c<<16);
            *(uint32_t*)(dst+4) = d|(AVERAGE16(d,e)<<16);
            *(uint32_t*)(dst+6) = e|(f<<16);
            *(uint32_t*)(dst+8) = AVERAGE16(f,g)|(g<<16);

            src+=7;
            dst+=10;

        }

        Eh += 144;
        if(Eh >= height) {
            Eh -= height;
            vf = 0;
	    src += (320 - 224);
        }
        else {
            vf = 1;
	    src -= 224;
		}
	}
}

void upscale_144x224_to_320x240_rotate(uint16_t *dst, uint16_t *src)	//9:14 > 20:15
{
    register uint_fast16_t a, b, c, d, e;
    int Eh = 0;
    int vf = 0;

    for (int y = 0; y < 240; y++)
    {
        for (int x = 0; x < 144/9; x++)
        {
            a = *(src+0);
            b = *(src+320);
            c = *(src+320*2);
            d = *(src+320*3);
            e = *(src+320*4);

            if(vf == 1){
                a = AVERAGE16(a, *(src-1));
                b = AVERAGE16(b, *(src+320-1));
                c = AVERAGE16(c, *(src+320*2-1));
                d = AVERAGE16(d, *(src+320*3-1));
                e = AVERAGE16(e, *(src+320*4-1));
            }

            *(uint32_t*)(dst+0) = a|(a<<16);
            *(uint32_t*)(dst+2) = b|(b<<16);
            *(uint32_t*)(dst+4) = c|(c<<16);
            *(uint32_t*)(dst+6) = d|(d<<16);
            *(uint32_t*)(dst+8) = AVERAGE16(d,e)|(e<<16);

            a = *(src+320*5);
            b = *(src+320*6);
            c = *(src+320*7);
            d = *(src+320*8);

            if(vf == 1){
                a = AVERAGE16(a, *(src+320*5-1));
                b = AVERAGE16(b, *(src+320*6-1));
                c = AVERAGE16(c, *(src+320*7-1));
                d = AVERAGE16(d, *(src+320*8-1));
            }

            *(uint32_t*)(dst+10) = e|(a<<16);
            *(uint32_t*)(dst+12) = a|(b<<16);
            *(uint32_t*)(dst+14) = b|(c<<16);
            *(uint32_t*)(dst+16) = c|(AVERAGE16(c,d)<<16);
            *(uint32_t*)(dst+18) = d|(d<<16);

            src+=320*9;
            dst+=20;

        }

        Eh += 224;
        if(Eh >= 240) {
            Eh -= 240;
            vf = 0;
	    src -= (320*144)+1;
        }
        else {
            vf = 1;
	    src -= (320*144);
		}
    }
}

#define DARKER(c1, c2) (c1 > c2 ? c2 : c1)

// WS 224x144 to 336x216 (-8,12) or 318x216 (1,12)
void upscale_15x_sharp(uint16_t *dst, uint16_t *src) {
	register uint_fast16_t a,b,c,d,e,f;
	uint32_t x,y;

	// centering
	dst += (320*((240-216)/2)) + (320-318)/2;
	// clipping
	src += 6;
	
	for (y=(144/2); y>0 ; y--, src+=320+(320-212), dst+=320*2+(320-318))
	{	
		for (x=(212/4); x>0; x--, src+=4, dst+=6)
		{
			a = *(src+0);
			b = *(src+1);
			c = *(src+320);
			d = *(src+320+1);
			e = DARKER(a,c);
			f = DARKER(b,d);

			*(uint32_t*)(dst+  0) = a|(DARKER(a,b)<<16);
			*(uint32_t*)(dst+320) = e|(DARKER(e,f)<<16);
			*(uint32_t*)(dst+640) = c|(DARKER(c,d)<<16);

			c = *(src+320+2);
			a = *(src+2);
			e = DARKER(a,c);

			*(uint32_t*)(dst+  2) = b|(a<<16);
			*(uint32_t*)(dst+322) = f|(e<<16);
			*(uint32_t*)(dst+642) = d|(c<<16);

			b = *(src+3);
			d = *(src+320+3);
			f = DARKER(b,d);

			*(uint32_t*)(dst+  4) = DARKER(a,b)|(b<<16);
			*(uint32_t*)(dst+324) = DARKER(e,f)|(f<<16);
			*(uint32_t*)(dst+644) = DARKER(c,d)|(d<<16);
		}
	}
}