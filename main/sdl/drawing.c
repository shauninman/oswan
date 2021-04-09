#include "drawing.h"
#ifdef SCALING
#include "gfx/SDL_rotozoom.h"
#endif

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
//		screenshots = SDL_CreateRGBSurface(FLAG_VIDEO, w, h, BITDEPTH_OSWAN, 0,0,0,0);
		screenshots = SDL_CreateRGBSurface(0, w, h, BITDEPTH_OSWAN, 0,0,0,0);
	#endif
}

void Set_DrawRegion(void)
{
	/* Clear screen too to avoid graphical glitches */
	SDL_FillRect(actualScreen, NULL, 0);

	switch (GameConf.m_ScreenRatio) {
		case SCREEN_RATIO_NATIVE:		// Native
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
		case SCREEN_RATIO_FULLSCREEN:		// Fullscreen
			screen_to_draw_region.w	= 320;
			screen_to_draw_region.h	= 240;
			screen_to_draw_region.offset_x = 0;
			screen_to_draw_region.offset_y = 0; 
			break;
		case SCREEN_RATIO_ASPECT:		// Aspect
			screen_to_draw_region.w	= 320;
			screen_to_draw_region.h	= 204;
			screen_to_draw_region.offset_x = 0;
			screen_to_draw_region.offset_y = 18; 
			break;
		case SCREEN_RATIO_ROTATE:		// Rotate
			screen_to_draw_region.w	= 144;
			screen_to_draw_region.h	= 224;
			screen_to_draw_region.offset_x = 88;
			screen_to_draw_region.offset_y = 8;
			break;
		case SCREEN_RATIO_ROTATE_WIDE:		// RotateWide
			screen_to_draw_region.w	= 288;
			screen_to_draw_region.h	= 224;
			screen_to_draw_region.offset_x = 16;
			screen_to_draw_region.offset_y = 8;
			break;
		case SCREEN_RATIO_15X_SHARP:		
			screen_to_draw_region.w	= 320;
			screen_to_draw_region.h	= 216;
			screen_to_draw_region.offset_x = 0;
			screen_to_draw_region.offset_y = 12;
			break;
	}
}

void screen_draw(void)
{
#ifndef TRIMUI
	uint16_t *buffer_scr = (uint16_t *) actualScreen->pixels;
	uint32_t W,H,ix,iy,x,y;
	
	SDL_LockSurface(actualScreen);
	
	x=screen_to_draw_region.offset_x;
	y=screen_to_draw_region.offset_y; 
	W=screen_to_draw_region.w;
	H=screen_to_draw_region.h;
	ix=(SYSVID_WIDTH<<16)/W;
	iy=(SYSVID_HEIGHT<<16)/H;
	
	buffer_scr += (y)*320;
	buffer_scr += (x);
	do   
	{
		uint16_t *buffer_mem=(uint16_t *) (FrameBuffer+8+((y>>16)*SCREEN_WIDTH));	// +8 offset
		W=screen_to_draw_region.w; x=0;
		do 
		{
			*buffer_scr++=buffer_mem[x>>16];
#if BITDEPTH_OSWAN == 32
			*buffer_scr++=buffer_mem[x>>16];
#endif
			x+=ix;
		} while (--W);
		y+=iy;
#ifndef NATIVE_RESOLUTION
		if (screen_to_draw_region.w == 224) buffer_scr += actualScreen->pitch - 320 - SYSVID_WIDTH;
#endif
	} while (--H);
#else				// for TRIMUI scalers
	uint16_t *src = (uint16_t *) FrameBuffer +8;	// +8 offset
	uint16_t *dst = (uint16_t *) actualScreen->pixels + screen_to_draw_region.offset_x + screen_to_draw_region.offset_y * 320;
	uint32_t x , y;
	SDL_LockSurface(actualScreen);

	switch (GameConf.m_ScreenRatio) {
		case SCREEN_RATIO_NATIVE:		// Native	224x144
			for(y = 0; y < 144; y++, src += 320, dst += 320) memmove(dst, src, 224*2);
			upscale_15x_sharp(actualScreen->pixels, src);
			break;
		case SCREEN_RATIO_FULLSCREEN:		// Fullscreen	224x144 > 320x240	7x3 > 10x5
			upscale_224x144_to_320xXXX(dst, src, 240);
			break;
		case SCREEN_RATIO_ASPECT:		// Aspect	224x144 > 320x204
			upscale_224x144_to_320xXXX(dst, src, 204);
			break;
		case SCREEN_RATIO_ROTATE:		// Rotate	224x144 > 144x224
			src += 223;
			for( y = 0; y < 224; y++) {
				for ( x = 0; x < 144; x++) {
					*dst++ = *src;
					src += 320;
				}
				dst += (320 - 144);
				src -= (320 * 144) + 1;
			}
			break;
		case SCREEN_RATIO_ROTATE_WIDE:		// RotateWide	224x144 > 288x224
			src += 223;
			for( y = 0; y < 224; y++) {
				for ( x = 0; x < 144; x++) {
					*dst++ = *src; *dst++ = *src;
					src += 320;
				}
				dst += (320 - 288);
				src -= (320 * 144) + 1;
			}
			break;
		case SCREEN_RATIO_15X_SHARP: // 1.5x sharp 336x216 (-8,12)
			upscale_15x_sharp(actualScreen->pixels, src);
			break;
	}
#endif

	static char buffer[4];
	if (GameConf.m_DisplayFPS) 
	{
#ifndef NATIVE_RESOLUTION
		if (GameConf.m_ScreenRatio != SCREEN_RATIO_FULLSCREEN)
#else
		if ((GameConf.m_ScreenRatio != SCREEN_RATIO_NATIVE) && (GameConf.m_ScreenRatio != SCREEN_RATIO_FULLSCREEN))
#endif
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

#ifdef TRIMUI
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

            *(dst+0) = a;
            *(dst+1) = b;
            *(dst+2) = AVERAGE16(b,c);
            *(dst+3) = c;
            *(dst+4) = d;
            *(dst+5) = AVERAGE16(d,e);
            *(dst+6) = e;
            *(dst+7) = f;
            *(dst+8) = AVERAGE16(f,g);
            *(dst+9) = g;

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

void upscale_15x_sharp(uint16_t *dst_px, uint16_t *src_px) {
	// src might be 320px...
	uint16_t *next_row, *prev_row;
	
	dst_px += 320 * 12;
	
	unsigned int x,y,ox=0,oy=0,p,c,n,a,b,pa,pb,na,nb,skipped=0,s;
	for (y=0; y<144; y++) {
		s = 0;
		for (x=0; x<224; x++) {
			c = *src_px;
			
			if (s>=8 && s<328) {
				*dst_px = c;
				dst_px += 1;
			}
			s += 1;
			
			ox = !ox;
			if (ox) {
				n = *(src_px+1);
				if (s>=8 && s<328) {
					if (c>n) *dst_px = n; // always pick the darker
					else *dst_px = c;
					// *dst_px = 0xf800;
					dst_px += 1;
				}
				s += 1;
			}
			src_px += 1;
		}
		src_px += 96;
		
		if (skipped) {
			// NOTE: we hit the oy condition on the iteration before this
			// so we've just drawn the line after the one we skipped
			// so let's jump back to the beginning of the skipped line
			dst_px -= 320 * 2;
			for (x=0;x<320;x++) {
				n = *next_row;
				c = *prev_row;
				if (c>n) *dst_px = n; // always pick the darker
				else *dst_px = c;
				// *dst_px = 0xf800;
				prev_row += 1;
				dst_px += 1;
				next_row += 1;
			}
			dst_px += 320; // skip to the line after the one we had already drawn
			skipped = 0;
		}
		
		oy = !oy;
		if (oy) {
			// NOTE: we hit this before skipped condition
			// we are going to skip this interpolated line
			// and revisit it once we've drawn the next line
			skipped = 1;
			prev_row = dst_px - 320;
			dst_px += 320;
			next_row = dst_px;
		}
	}
}
#endif
