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
			case 4:		// 1.5x Sharp2
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
	uint16_t *src = (uint16_t *) FrameBuffer +8;	// +8 offset , width = 240
	uint16_t *dst = (uint16_t *) actualScreen->pixels + screen_to_draw_region.offset_x + screen_to_draw_region.offset_y * 320;
	uint32_t x , y;
	SDL_LockSurface(actualScreen);

	if ( (GameConf.m_Rotate == 0) || ((GameConf.m_Rotate == 2)&&(HVMode == 0)) ) {
		// Normal Scalers
		switch (GameConf.m_ScreenRatioH) {
			case 0:		// Native	224x144
				for(y = 0; y < 144; y++, src += 240, dst += 320) memcpy(dst, src, 224*2);
				break;
			case 1:		// Fullscreen	224x144 > 320x240	7x3 > 10x5
				upscale_224x144_to_320xXXX(dst, src, 240);
				break;
			case 2:		// Aspect	224x144 > 320x204
				upscale_224x144_to_320xXXX(dst, src, 204);
				break;
			case 3:		// 1.5x Sharp	213.5x144 > 320x216 (crop 10.5px side)
				src += 5;
				upscale_15x_sharp(dst,src);
				break;
			case 4:		// 1.5x Sharp2	213.5x144 > 320x216 (crop 10.5px side)
				src += 5;
				upscale_15x_sharp2(dst,src);
				break;
		}
	} else {
		// Rotate Scalers
		switch (GameConf.m_ScreenRatioV) {
			case 0:		// Rotate	224x144 > 144x224
				dst += (320*(224-1));	// draw from Left-Down
				for ( x = 0; x < 144/2; x++) {
					for( y = 0; y < 224; y++) {
						*(uint32_t*)dst = *src|(*(src+240)<<16);
						src++; dst -= 320;
					}
					src += (240-224)+240; dst += (320*224)+2;
				}
				break;
			case 1:		// RotateFull	224x144 > 320x240
				dst += (320*(240-1));
				upscale_144x224_to_320x240_rotate(dst, src);
				break;
			case 2:		// RotateWide	224x144 > 288x224
				dst += (320*(224-1));
				for ( x = 0; x < 144; x++) {
					for( y = 0; y < 224; y++) {
						*(uint32_t*)dst = *src|(*src<<16);
						src++; dst -= 320;
					}
					src += (240-224); dst += (320*224)+2;
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

            if (vf) {
                a = AVERAGE16(a, *(src+240));
                b = AVERAGE16(b, *(src+241));
                c = AVERAGE16(c, *(src+242));
                d = AVERAGE16(d, *(src+243));
                e = AVERAGE16(e, *(src+244));
                f = AVERAGE16(f, *(src+245));
                g = AVERAGE16(g, *(src+246));
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
	    src += (240 - 224);
        }
        else {
            vf = 1;
	    src -= 224;
	}
    }
}

void upscale_144x224_to_320x240_rotate(uint16_t *dst, uint16_t *src)	//9:14 > 20:15
{
    register uint_fast16_t a, b;
    int Eh = 0;
    int vf = 0;
    int x,y;

#define	RAB		a = *src++; b = *src++
#define RWAB		RAB; WA; WB
#define	R14_W15		RWAB; RWAB; RWAB; RWAB; RWAB; RWAB; RAB; WA; a = AVERAGE16(a,b); WA; WB
#define	R14_W15b	RWAB; RWAB; RWAB; RWAB; RWAB; RWAB; RAB; WA; a = AVERAGE16(a,b); WAb; WB

    for (x = 0; x < 144; x++)
    {
	if (vf) {	// 1(.5) to 3 lines
	    if (((uintptr_t)dst & 3) == 0) {	// alignment check
		for (y = 0; y < 224/14; y++)
		{
#define	WA	*(uint32_t*)dst = a|a<<16; *(dst+2) = AVERAGE16(a,*(src+238)); dst-=320
#define	WAb	*(uint32_t*)dst = a|a<<16; *(dst+2) = AVERAGE16(a,AVERAGE16(*(src+238),*(src+239))); dst-=320
#define	WB	*(uint32_t*)dst = b|b<<16; *(dst+2) = AVERAGE16(b,*(src+239)); dst-=320
		R14_W15b;
		}
	    } else {
		for (y = 0; y < 224/14; y++)
		{
#undef	WA
#undef	WAb
#undef	WB
#define	WA	*dst = a; *(uint32_t*)(dst+1) = a|AVERAGE16(a,*(src+238))<<16; dst-=320
#define	WAb	*dst = a; *(uint32_t*)(dst+1) = a|AVERAGE16(a,AVERAGE16(*(src+238),*(src+239)))<<16; dst-=320
#define	WB	*dst = b; *(uint32_t*)(dst+1) = b|AVERAGE16(b,*(src+239))<<16; dst-=320
		R14_W15b;
		}
	    }
	    dst += (320*240)+3;
	} else {	// 1 to 2 lines
	    if (((uintptr_t)dst & 3) == 0) {	// alignment check
		for (y = 0; y < 224/14; y++)
		{
#undef	WA
#undef	WAb
#undef	WB
#define	WA	*(uint32_t*)dst = a|a<<16; dst-=320
#define	WB	*(uint32_t*)dst = b|b<<16; dst-=320
		R14_W15;
		}
	    } else {
	        for (y = 0; y < 224/14; y++)
		{
#undef	WA
#undef	WB
#define	WA	*dst = a; *(dst+1) = a; dst-=320
#define	WB	*dst = b; *(dst+1) = b; dst-=320
		R14_W15;
		}
	    }
	    dst += (320*240)+2;
	}
	src += (240 - 224);
	Eh += 125;	// 144;
	if (Eh >= (320/2)) { Eh -= (320/2); vf = 0; } else vf = 1;
    }
#undef	WA
#undef	WB
#undef	RAB
#undef	RWAB
#undef	R14_W15
}

#define DARKER(c1, c2) (c1 > c2 ? c2 : c1)
#define LIGHTER(c1, c2) (c1 > c2 ? c1 : c2)

void upscale_15x_sharp(uint16_t *dst, uint16_t *src)
{	//	213.5x144 > 320x216 (crop 10.5px side)
    register uint_fast16_t a, b, c, d;
    int Eh = 0;
    int vf = 0;

    for (int y = 0; y < 216; y++)
    {
	for (int x = 0; x < 318/6; x++)
	{
	    a = *(src+0);
	    b = *(src+1);
	    c = *(src+2);
	    d = *(src+3);
	
	    if (vf) {
		a = DARKER(a, *(src+240));
		b = DARKER(b, *(src+241));
		c = DARKER(c, *(src+242));
		d = DARKER(d, *(src+243));
	    }
	
	    *(uint32_t*)(dst+0) = a|(DARKER(a,b)<<16);
	    *(uint32_t*)(dst+2) = b|(c<<16);
	    *(uint32_t*)(dst+4) = (DARKER(c,d))|(d<<16);
	
	    src+=4;
	    dst+=6;
	}

	// last 2px
	a = *(src+0);
	b = *(src+1);
	    if (vf) {
		a = DARKER(a, *(src+240));
		b = DARKER(b, *(src+241));
	    }
	*(uint32_t*)(dst+0) = a|(DARKER(a,b)<<16);
	dst += 2;

        Eh += 144;
        if(Eh >= 216) {
            Eh -= 216;
            vf = 0;
	    src += (240 - 212);
        }
        else {
            vf = 1;
	    src -= 212;
	}
    }
}

void upscale_15x_sharp2(uint16_t *dst, uint16_t *src)
{	//	213.5x144 > 320x216 (crop 10.5px side)
    register uint_fast16_t a, b, c, d;
    int Eh = 0;
    int vf = 0;

    for (int y = 0; y < 216; y++)
    {
	for (int x = 0; x < 318/6; x++)
	{
	    a = *(src+0);
	    b = *(src+1);
	    c = *(src+2);
	    d = *(src+3);
	
	    if (vf) {
		a = DARKER(a, *(src+240));
		b = DARKER(b, *(src+241));
		c = DARKER(c, *(src+242));
		d = DARKER(d, *(src+243));
	    }
	
	    *(uint32_t*)(dst+0) = a|(LIGHTER(a,b)<<16);
	    *(uint32_t*)(dst+2) = b|(c<<16);
	    *(uint32_t*)(dst+4) = (LIGHTER(c,d))|(d<<16);
	
	    src+=4;
	    dst+=6;
	}

	// last 2px
	a = *(src+0);
	b = *(src+1);
	    if (vf) {
		a = DARKER(a, *(src+240));
		b = DARKER(b, *(src+241));
	    }
	*(uint32_t*)(dst+0) = a|(LIGHTER(a,b)<<16);
	dst += 2;

        Eh += 144;
        if(Eh >= 216) {
            Eh -= 216;
            vf = 0;
	    src += (240 - 212);
        }
        else {
            vf = 1;
	    src -= 212;
	}
    }
}
/*
void upscale_15x_scanline(uint16_t *dst, uint16_t *src)
{	//	213.5x144 > 320x216 (crop 10.5px side)
    register uint_fast16_t a, b, c, d;
    int Eh = 0;
    int vf = 0;

    for (int y = 0; y < 216; y++)
    {
	for (int x = 0; x < 318/6; x++)
	{
	    a = *(src+0);
	    b = *(src+1);
	    c = *(src+2);
	    d = *(src+3);
	
	    if (vf) {
		a = ((a & 0xF7DE)+((*(src+240) & 0xE79C)>>1))>>1;
		b = ((b & 0xF7DE)+((*(src+241) & 0xE79C)>>1))>>1;
		c = ((c & 0xF7DE)+((*(src+242) & 0xE79C)>>1))>>1;
		d = ((d & 0xF7DE)+((*(src+243) & 0xE79C)>>1))>>1;
	    }
	
	    *(uint32_t*)(dst+0) = a|(((a & 0xF7DE)+((b & 0xE79C)>>1))<<15);
	    *(uint32_t*)(dst+2) = b|(c<<16);
	    *(uint32_t*)(dst+4) = (((c & 0xF7DE)+((d & 0xE79C)>>1))>>1)|(d<<16);
	
	    src+=4;
	    dst+=6;
	}

	// last 2px
	a = *(src+0);
	b = *(src+1);
	    if (vf) {
		a = ((a & 0xF7DE)+((*(src+240) & 0xE79C)>>1))>>1;
		b = ((b & 0xF7DE)+((*(src+241) & 0xE79C)>>1))>>1;
	    }
	*(uint32_t*)(dst+0) = a|(((a & 0xF7DE)+((b & 0xE79C)>>1))<<15);
	dst += 2;

        Eh += 144;
        if(Eh >= 216) {
            Eh -= 216;
            vf = 0;
	    src += (240 - 212);
        }
        else {
            vf = 1;
	    src -= 212;
	}
    }
}
*/
