#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libgen.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <SDL/SDL.h>
#include "shared.h"
#include "scaler.h"
#include "smsplus.h"
#include "text_gui.h"
#include "bigfontwhite.h"
#include "bigfontred.h"
#include "font.h"
#include "sound_output.h"

static gamedata_t gdata;

t_config option;
SDL_Surface* sdl_screen;
static SDL_Surface *img_background, *scale2x_buf;
static char home_path[256];
SDL_Surface *sms_bitmap;

static SDL_Joystick* joystick[2];

extern SDL_Surface *font, *bigfontred, *bigfontwhite;

static uint8_t selectpressed = 0;
static uint8_t save_slot = 0;
static uint8_t quit = 0;
static const int8_t upscalers_available = 2
#ifdef SCALE2X_UPSCALER
+2
#endif
;

static void video_update()
{
	SDL_Rect dst, dstT;
	SDL_LockSurface(sdl_screen);
	switch(option.fullscreen) 
	{
#ifdef SCALE2X_UPSCALER
		// Unscaled Scale2x
		case 4:
			if (sms.console == CONSOLE_GG) 
			{
				dst.w = 160*3;
				dst.h = 144*3;
				dstT.x = 96;
				dstT.y = 0;
				dstT.w = 320;
				dstT.h = 288;
			}
			else
			{
				dst.w = VIDEO_WIDTH_SMS * 2;
				dst.h = vdp.height * 2;
				dstT.x = (vdp.reg[0] & 0x20) ? 16 : 0;
				dstT.y = 0;
				dstT.w = dst.w - dstT.x;
				dstT.h = dst.h;
			}
			scale2x(sms_bitmap->pixels, scale2x_buf->pixels, 512, 1024, 256, 240);
			bitmap_scale(
			dstT.x,dstT.y,
			dstT.w,dstT.h,
			dst.w,dst.h,
			512,sdl_screen->w-dst.w,
			(uint16_t* restrict)scale2x_buf->pixels,
			(uint16_t* restrict)sdl_screen->pixels+(sdl_screen->w-dst.w)/2+(sdl_screen->h-dst.h)/2*sdl_screen->w);
		break;
		case 3: // Scale2x
			if (sms.console == CONSOLE_GG) 
			{
				dst.x = 96;
				dst.y = 0;
				dst.w = 320;
				dst.h = 288;
				dstT.x = 48;
				dstT.y = 0;
				dstT.w = 320;
				dstT.h = 288;
			}
			else
			{
				dst.x = (vdp.reg[0] & 0x20) ? 16 : 0;
				dst.y = 0;
				dst.w = (vdp.reg[0] & 0x20) ? 496 : 512;
				dst.h = vdp.height*2;
				dstT.x = 0;
				dstT.y = 0;
				dstT.w = (dst.w)-dstT.x;
				dstT.h = vdp.height*2;
			}
			scale2x(sms_bitmap->pixels, scale2x_buf->pixels, 512, 1024, 256, 240);
			bitmap_scale(
			dst.x,dst.y,
			dst.w,dst.h,
			sdl_screen->w,sdl_screen->h,
			512,0,
			(uint16_t* restrict)scale2x_buf->pixels,(uint16_t* restrict)sdl_screen->pixels);
		break;
#endif
		case 2: // Fullscreen (cropped)
			if (sms.console == CONSOLE_GG) 
			{
				dst.w = sdl_screen->w;
				dst.h = sdl_screen->h;
				dstT.x = 48;
				dstT.y = 0;
				dstT.w = 160;
				dstT.h = 144;
			}
			else
			{
				dst.w = sdl_screen->w;
				dst.h = sdl_screen->h;
				dstT.x = (vdp.reg[0] & 0x20) ? 8 : 0;
				dstT.y = 0;
				dstT.w = VIDEO_WIDTH_SMS-dstT.x;
				dstT.h = vdp.height;
			}
			bitmap_scale(
			dstT.x,dstT.y,
			dstT.w,dstT.h,
			dst.w,dst.h,
			VIDEO_WIDTH_SMS,0,
			(uint16_t* restrict)sms_bitmap->pixels,(uint16_t* restrict)sdl_screen->pixels);
		break;
		case 1: // Stretch (keep aspect ratio)
			if (sms.console == CONSOLE_GG) 
			{
				dst.w = 640;
				dst.h = 480;
				dstT.x = 48;
				dstT.y = 0;
				dstT.w = 160;
				dstT.h = 144;
			}
			else
			{
				dst.w = 640;
				dst.h = 480;
				dstT.x = (vdp.reg[0] & 0x20) ? 8 : 0;
				dstT.y = 0;
				dstT.w = VIDEO_WIDTH_SMS - dstT.x;
				dstT.h = vdp.height;
			}
			bitmap_scale(
			dstT.x,dstT.y,
			dstT.w,dstT.h,
			dst.w,dst.h,
			256,sdl_screen->w-dst.w,
			(uint16_t* restrict)sms_bitmap->pixels,
			(uint16_t* restrict)sdl_screen->pixels+(sdl_screen->w-dst.w)/2+(sdl_screen->h-dst.h)/2*sdl_screen->w);
		break;
        case 0: // native res
			if (sms.console == CONSOLE_GG) 
			{
				dst.w = 160*3;
				dst.h = 144*3;
				dstT.x = 48;
				dstT.y = 0;
				dstT.w = 160;
				dstT.h = 144;
			}
			else
			{
				dst.w = VIDEO_WIDTH_SMS * 2;
				dst.h = vdp.height * 2;
				dstT.x = (vdp.reg[0] & 0x20) ? 8 : 0;
				dstT.y = 0;
				dstT.w = VIDEO_WIDTH_SMS - dstT.x;
				dstT.h = vdp.height;
			}
			bitmap_scale(
			dstT.x,dstT.y,
			dstT.w,dstT.h,
			dst.w,dst.h,
			256,sdl_screen->w-dst.w,
			(uint16_t* restrict)sms_bitmap->pixels,
			(uint16_t* restrict)sdl_screen->pixels+(sdl_screen->w-dst.w)/2+(sdl_screen->h-dst.h)/2*sdl_screen->w);
		break;
	}
	//SDL_SoftStretch(sms_bitmap, &dstT, sdl_screen, &dst);
	SDL_UnlockSurface(sdl_screen);
	SDL_Flip(sdl_screen);
}

void smsp_state(uint8_t slot_number, uint8_t mode)
{
	// Save and Load States
	char stpath[PATH_MAX];
	snprintf(stpath, sizeof(stpath), "%s%s.st%d", gdata.stdir, gdata.gamename, slot_number);
	FILE *fd;
	
	switch(mode) {
		case 0:
			fd = fopen(stpath, "wb");
			if (fd) {
				system_save_state(fd);
				fclose(fd);
			}
			break;
		
		case 1:
			fd = fopen(stpath, "rb");
			if (fd) {
				system_load_state(fd);
				fclose(fd);
			}
			break;
	}
}

void system_manage_sram(uint8_t *sram, uint8_t slot_number, uint8_t mode) 
{
	// Set up save file name
	FILE *fd;
	switch(mode) 
	{
		case SRAM_SAVE:
			if(sms.save) 
			{
				fd = fopen(gdata.sramfile, "wb");
				if (fd) 
				{
					fwrite(sram, 0x8000, 1, fd);
					fclose(fd);
				}
			}
			break;
		
		case SRAM_LOAD:
			fd = fopen(gdata.sramfile, "rb");
			if (fd) 
			{
				sms.save = 1;
				fread(sram, 0x8000, 1, fd);
				fclose(fd);
			}
			else
				memset(sram, 0x00, 0x8000);
			break;
	}
}

static void bios_init()
{
	FILE *fd;
	char bios_path[256];
	
	bios.rom = malloc(0x100000);
	bios.enabled = 0;
	
	snprintf(bios_path, sizeof(bios_path), "%s%s", gdata.biosdir, "BIOS.sms");

	fd = fopen(bios_path, "rb");
	if(fd)
	{
		/* Seek to end of file, and get size */
		fseek(fd, 0, SEEK_END);
		uint32_t size = ftell(fd);
		fseek(fd, 0, SEEK_SET);
		if (size < 0x4000) size = 0x4000;
		fread(bios.rom, size, 1, fd);
		bios.enabled = 2;  
		bios.pages = size / 0x4000;
		fclose(fd);
	}

	snprintf(bios_path, sizeof(bios_path), "%s%s", gdata.biosdir, "BIOS.col");
	
	fd = fopen(bios_path, "rb");
	if(fd)
	{
		/* Seek to end of file, and get size */
		fread(coleco.rom, 0x2000, 1, fd);
		fclose(fd);
	}
}

static void smsp_gamedata_set(char *filename) 
{
	// Set paths, create directories
	snprintf(home_path, sizeof(home_path), "%s/.smsplus/", getenv("HOME"));
	
	if (mkdir(home_path, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", home_path, errno);
	}
	
	// Set the game name
	snprintf(gdata.gamename, sizeof(gdata.gamename), "%s", basename(filename));
	
	// Strip the file extension off
	for (unsigned long i = strlen(gdata.gamename) - 1; i > 0; i--) {
		if (gdata.gamename[i] == '.') {
			gdata.gamename[i] = '\0';
			break;
		}
	}
	
	// Set up the sram directory
	snprintf(gdata.sramdir, sizeof(gdata.sramdir), "%ssram/", home_path);
	if (mkdir(gdata.sramdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.sramdir, errno);
	}
	
	// Set up the sram file
	snprintf(gdata.sramfile, sizeof(gdata.sramfile), "%s%s.sav", gdata.sramdir, gdata.gamename);
	
	// Set up the state directory
	snprintf(gdata.stdir, sizeof(gdata.stdir), "%sstate/", home_path);
	if (mkdir(gdata.stdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.stdir, errno);
	}
	
	// Set up the screenshot directory
	snprintf(gdata.scrdir, sizeof(gdata.scrdir), "%sscreenshots/", home_path);
	if (mkdir(gdata.scrdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.scrdir, errno);
	}
	
	// Set up the sram directory
	snprintf(gdata.biosdir, sizeof(gdata.biosdir), "%sbios/", home_path);
	if (mkdir(gdata.biosdir, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", gdata.sramdir, errno);
	}
	
}

static void Menu()
{
    int16_t pressed = 0;
    int16_t currentselection = 1;
    uint16_t miniscreenwidth = 140;
    uint16_t miniscreenheight = 135;
    SDL_Rect dstRect;
    SDL_Event Event;
    /* Seriously, we need that mess due to crappy framebuffer drivers on the RS-07... */
    SDL_Surface* miniscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, miniscreenwidth, miniscreenheight, 16, sdl_screen->format->Rmask, sdl_screen->format->Gmask, sdl_screen->format->Bmask, sdl_screen->format->Amask);
    SDL_Surface* black_screen = SDL_CreateRGBSurface(SDL_SWSURFACE, sdl_screen->w, sdl_screen->h, 16, sdl_screen->format->Rmask, sdl_screen->format->Gmask, sdl_screen->format->Bmask, sdl_screen->format->Amask);
    SDL_Surface* final_screen = SDL_CreateRGBSurface(SDL_SWSURFACE, sdl_screen->w, sdl_screen->h, 16, sdl_screen->format->Rmask, sdl_screen->format->Gmask, sdl_screen->format->Bmask, sdl_screen->format->Amask);
	SDL_FillRect( black_screen, NULL, 0 );

    SDL_LockSurface(miniscreen);
    if(IS_GG)
        bitmap_scale(48,0,160,144,miniscreenwidth,miniscreenheight,256,0,(uint16_t*)sms_bitmap->pixels,(uint16_t*)miniscreen->pixels);
    else
        bitmap_scale(0,0,256,192,miniscreenwidth,miniscreenheight,256,0,(uint16_t*)sms_bitmap->pixels,(uint16_t*)miniscreen->pixels);
        
    SDL_UnlockSurface(miniscreen);
    char text[50];
    
    Sound_Pause();

    SDL_PollEvent(&Event);
    while (((currentselection != 1) && (currentselection != 5)) || (!pressed))
    {
        pressed = 0;
		
        dstRect.x = sdl_screen->w-5-miniscreenwidth;
        dstRect.y = 30;
		if (!img_background) SDL_BlitSurface(black_screen,NULL,final_screen,NULL);
		else SDL_BlitSurface(img_background,NULL,final_screen,NULL);
        SDL_BlitSurface(miniscreen,NULL,final_screen,&dstRect);

        gfx_font_print_center(final_screen,22,bigfontwhite,"SMS Plus GX");

        if (currentselection == 1)
            gfx_font_print(final_screen,5,25,bigfontred,"Continue");
        else
            gfx_font_print(final_screen,5,25,bigfontwhite,"Continue");

        sprintf(text,"Save State %d",save_slot);

        if (currentselection == 2)
            gfx_font_print(final_screen,5,65,bigfontred,text);
        else
            gfx_font_print(final_screen,5,65,bigfontwhite,text);

        sprintf(text,"Load State %d",save_slot);

        if (currentselection == 3)
            gfx_font_print(final_screen,5,85,bigfontred,text);
        else
            gfx_font_print(final_screen,5,85,bigfontwhite,text);

        if (currentselection == 4)
        {
			switch (option.fullscreen)
			{
				case 0:
					gfx_font_print(final_screen,5,105,bigfontred,"Native resolution");
				break;
				case 1:
					gfx_font_print(final_screen,5,105,bigfontred,"Keep Aspect Ratio");
				break;
				case 2:
					gfx_font_print(final_screen,5,105,bigfontred,"Fullscreen");
				break;
				case 3:
					gfx_font_print(final_screen,5,105,bigfontred,"EPX/Scale2x Full");
				break;
				case 4:
					gfx_font_print(final_screen,5,105,bigfontred,"EPX/Scale2x Unscaled");
				break;
			}
        }
        else
        {
			switch (option.fullscreen)
			{
				case 0:
					gfx_font_print(final_screen,5,105,bigfontwhite,"Native resolution");
				break;
				case 1:
					gfx_font_print(final_screen,5,105,bigfontwhite,"Keep Aspect Ratio");
				break;
				case 2:
					gfx_font_print(final_screen,5,105,bigfontwhite,"Fullscreen");
				break;
				case 3:
					gfx_font_print(final_screen,5,105,bigfontwhite,"EPX/Scale2x Full");
				break;
				case 4:
					gfx_font_print(final_screen,5,105,bigfontwhite,"EPX/Scale2x Unscaled");
				break;
			}
        }

        if (currentselection == 5)
            gfx_font_print(final_screen,5,125,bigfontred,"Quit");
        else
            gfx_font_print(final_screen,5,125,bigfontwhite,"Quit");

        gfx_font_print_center(final_screen,sdl_screen->h-80-gfx_font_height(font),font,"SMS_SDL for the RS-97");
        gfx_font_print_center(final_screen,sdl_screen->h-70-gfx_font_height(font),font,"PAP K3S port by gameblabla");
        gfx_font_print_center(final_screen,sdl_screen->h-60-gfx_font_height(font),font,"See full credits on github:");
        gfx_font_print_center(final_screen,sdl_screen->h-50-gfx_font_height(font),font,"https://github.com/gameblabla/sms_sdl");

		for(uint8_t i = 0; i < 2; i++)
		{
			if (joystick[i] == NULL) joystick[i] = SDL_JoystickOpen(i);
		}

        while (SDL_PollEvent(&Event))
        {
			switch(Event.type)
			{
				case SDL_KEYDOWN:
				{
					switch(Event.key.keysym.sym)
					{
						case SDLK_UP:
							currentselection--;
							if (currentselection == 0)
								currentselection = 5;
							break;
						case SDLK_DOWN:
							currentselection++;
							if (currentselection == 6)
								currentselection = 1;
							break;
						case SDLK_LCTRL:
						case SDLK_LALT:
						case SDLK_RETURN:
							pressed = 1;
							break;
						case SDLK_LEFT:
							switch(currentselection)
							{
								case 2:
								case 3:
									if (save_slot > 0) save_slot--;
									break;
								case 4:
									option.fullscreen--;
										if (option.fullscreen < 0)
											option.fullscreen = upscalers_available;
									break;

							}
							break;
						case SDLK_RIGHT:
							switch(currentselection)
							{
								case 2:
								case 3:
									save_slot++;
									if (save_slot == 10)
										save_slot = 9;
									break;
								case 4:
									option.fullscreen++;
									if (option.fullscreen > upscalers_available)
										option.fullscreen = 0;
									break;
							}
							break;
					}
					break;
				}
				case SDL_JOYAXISMOTION:
					if (Event.jaxis.axis == 0)
					{
						if (Event.jaxis.value < -500)
						{
							switch(currentselection)
							{
								case 2:
								case 3:
									if (save_slot > 0) save_slot--;
									break;
								case 4:
									option.fullscreen--;
										if (option.fullscreen < 0)
											option.fullscreen = upscalers_available;
									break;

							}
						}
						else if (Event.jaxis.value > 500)
						{
							switch(currentselection)
							{
								case 2:
								case 3:
									save_slot++;
									if (save_slot == 10)
										save_slot = 9;
									break;
								case 4:
									option.fullscreen++;
									if (option.fullscreen > upscalers_available)
										option.fullscreen = 0;
									break;
							}
						}
					}
					else if (Event.jaxis.axis == 1)
					{
						if (Event.jaxis.value < -500)
						{
							currentselection--;
							if (currentselection == 0)
								currentselection = 5;
						}
						else if (Event.jaxis.value > 500)
						{
							currentselection++;
							if (currentselection == 6)
								currentselection = 1;
						}
					}
				break;
				case SDL_JOYBUTTONDOWN:
				switch(Event.jbutton.button)
				{
					case 1:
					case 2:
						pressed = 1;
					break;
				}
				break;
			}
        }
        
		SDL_JoystickUpdate();
		
        if (pressed)
        {
            switch(currentselection)
            {
                case 4 :
                    option.fullscreen++;
                    if (option.fullscreen > upscalers_available)
                        option.fullscreen = 0;
                    break;
                case 2 :
                    smsp_state(save_slot, 0);
					currentselection = 1;
                    break;
                case 3 :
					smsp_state(save_slot, 1);
					currentselection = 1;
				break;
            }
        }

		SDL_BlitSurface(final_screen, NULL, sdl_screen, NULL);
		SDL_Flip(sdl_screen);
    }
    
    if (currentselection == 5)
        quit = 1;
    else
		Sound_Unpause();
        
	if (quit)
	{
		if (miniscreen) SDL_FreeSurface(miniscreen);
		if (black_screen) SDL_FreeSurface(black_screen);
		if (final_screen) SDL_FreeSurface(final_screen);
	}
        
}

static void Cleanup(void)
{
#ifdef SCALE2X_UPSCALER
	if (scale2x_buf) SDL_FreeSurface(scale2x_buf);
#endif
	if (sdl_screen) SDL_FreeSurface(sdl_screen);
	if (sms_bitmap) SDL_FreeSurface(sms_bitmap);
	if (font) SDL_FreeSurface(font);
	if (bigfontwhite) SDL_FreeSurface(bigfontwhite);
	if (bigfontred) SDL_FreeSurface(bigfontred);
	
	for(uint8_t i=0;i<2;i++)
	{
		if (joystick[i]) SDL_JoystickClose(joystick[i]);
	}
	
	if (bios.rom) free(bios.rom);
	
	// Deinitialize audio and video output
	Sound_Close();
	
	SDL_Quit();

	// Shut down
	system_poweroff();
	system_shutdown();	
}

static void Controls_papk3s()
{
	int16_t x_move = 0, y_move = 0;
	uint8_t *keystate;
	keystate = SDL_GetKeyState(NULL);
	
	if (sms.console == CONSOLE_COLECO)
    {
		coleco.keypad[0] = 0xff;
		coleco.keypad[1] = 0xff;
	}
	
	if (keystate[SDLK_TAB])
		coleco.keypad[0] = 1;
		
	if (keystate[SDLK_BACKSPACE])
		coleco.keypad[0] = 2;
		
	for(uint8_t i=0;i<2;i++)
	{
		if (joystick[i] == NULL) joystick[i] = SDL_JoystickOpen(i);
				
		x_move = SDL_JoystickGetAxis(joystick[i], 0);
		y_move = SDL_JoystickGetAxis(joystick[i], 1);

		if (x_move > 500 || (i == 0 && keystate[SDLK_RIGHT]))
			input.pad[i] |= INPUT_RIGHT;
		else
			input.pad[i] &= ~INPUT_RIGHT;
					
		if (x_move < -500 || (i == 0 && keystate[SDLK_LEFT]))
			input.pad[i] |= INPUT_LEFT;
		else
			input.pad[i] &= ~INPUT_LEFT;

		if (y_move > 500 || (i == 0 && keystate[SDLK_DOWN]))
			input.pad[i] |= INPUT_DOWN;
		else
			input.pad[i] &= ~INPUT_DOWN;

		if(y_move < -500 || (i == 0 && keystate[SDLK_UP]))
			input.pad[i] |= INPUT_UP;
		else
			input.pad[i] &= ~INPUT_UP;

		if((SDL_JoystickGetButton(joystick[i], 2) == SDL_PRESSED) || (i == 0 && keystate[SDLK_LCTRL]))
			input.pad[i] |= INPUT_BUTTON2;
		else
			input.pad[i] &= ~INPUT_BUTTON2;
				
		if((SDL_JoystickGetButton(joystick[i], 1) == SDL_PRESSED) || (i == 0 && keystate[SDLK_LALT]))
			input.pad[i] |= INPUT_BUTTON1;
		else
			input.pad[i] &= ~INPUT_BUTTON1;

		if(((i == 0) && (SDL_JoystickGetButton(joystick[i], 9) == SDL_PRESSED)) || (keystate[SDLK_RETURN]))
			input.system |= (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;
		else
			input.system &= (sms.console == CONSOLE_GG) ? ~INPUT_START : ~INPUT_PAUSE;
					
		if((SDL_JoystickGetButton(joystick[i], 8) == SDL_PRESSED) || keystate[SDLK_ESCAPE])
			selectpressed = 1;
	}
	
	if (sms.console == CONSOLE_COLECO) input.system = 0;

	SDL_JoystickUpdate();	
}

static void config_load()
{
	char config_path[256];
	snprintf(config_path, sizeof(config_path), "%s/config.cfg", home_path);
	FILE* fp;
	
	fp = fopen(config_path, "rb");
	if (fp)
	{
		fread(&option, sizeof(option), sizeof(int8_t), fp);
		fclose(fp);
	}
}

static void config_save()
{
	char config_path[256];
	snprintf(config_path, sizeof(config_path), "%s/config.cfg", home_path);
	FILE* fp;
	
	fp = fopen(config_path, "wb");
	if (fp)
	{
		fwrite(&option, sizeof(option), sizeof(int8_t), fp);
		fclose(fp);
	}
}

int main (int argc, char *argv[]) 
{
	SDL_Event event;
	// Print Header
	fprintf(stdout, "%s %s\n", APP_NAME, APP_VERSION);
	
	if(argc < 2) 
	{
		fprintf(stderr, "Usage: ./smsplus [FILE]\n");
		exit(1);
	}
	
	smsp_gamedata_set(argv[1]);
	
	memset(&option, 0, sizeof(option));
	
	option.fullscreen = 1;
	option.fm = 1;
	option.spritelimit = 1;
	option.tms_pal = 2;
	option.nosound = 0;
	option.soundlevel = 2;
	
	config_load();
	
	option.country = 0;
	option.console = 0;
	
	strcpy(option.game_name, argv[1]);
	
	// Force Colecovision mode if extension is .col
	if (strcmp(strrchr(argv[1], '.'), ".col") == 0) option.console = 6;
	// Sometimes Game Gear games are not properly detected, force them accordingly
	else if (strcmp(strrchr(argv[1], '.'), ".gg") == 0) option.console = 3;

	if (sms.console == CONSOLE_SMS || sms.console == CONSOLE_SMS2)
	{ 
		sms.use_fm = 1; 
	}

	// Load ROM
	if(!load_rom(argv[1])) {
		fprintf(stderr, "Error: Failed to load %s.\n", argv[1]);
		Cleanup();
		return 0;
	}

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_ENABLE);
	sdl_screen = SDL_SetVideoMode(800, 480, 16, SDL_HWSURFACE);
	SDL_SetCursor(0);
	
	// Needed in order to clear the cursor
	SDL_FillRect( sdl_screen, NULL, 0 );
	SDL_Flip(sdl_screen);
	
	sms_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, VIDEO_WIDTH_SMS, 267, 16, 0, 0, 0, 0);
#ifdef SCALE2X_UPSCALER
	scale2x_buf = SDL_CreateRGBSurface(SDL_SWSURFACE, VIDEO_WIDTH_SMS*2, 267*2, 16, 0, 0, 0, 0);
#endif
	img_background = SDL_LoadBMP("background.bmp");
	
    font = gfx_tex_load_tga_from_array(fontarray);
    bigfontwhite = gfx_tex_load_tga_from_array(bigfontwhitearray);
    bigfontred = gfx_tex_load_tga_from_array(bigfontredarray);
	
	// Load ROM
	if(!load_rom(argv[1])) {
		fprintf(stderr, "Error: Failed to load %s.\n", argv[1]);
		exit(1);
	}
	
	fprintf(stdout, "CRC : %08X\n", cart.crc);
	
	// Set parameters for internal bitmap
	bitmap.width = VIDEO_WIDTH_SMS;
	bitmap.height = VIDEO_HEIGHT_SMS;
	bitmap.depth = 16;
	bitmap.data = (uint8_t *)sms_bitmap->pixels;
	bitmap.pitch = sms_bitmap->pitch;
	bitmap.viewport.w = VIDEO_WIDTH_SMS;
	bitmap.viewport.h = VIDEO_HEIGHT_SMS;
	bitmap.viewport.x = 0x00;
	bitmap.viewport.y = 0x00;
	
	//sms.territory = settings.misc_region;

	bios_init();

	// Initialize all systems and power on
	system_poweron();
	
	Sound_Init();
	
	// Loop until the user closes the window
	while (!quit) 
	{
		// Execute frame(s)
		system_frame(0);
		
		// Refresh video data
		video_update();
		
		// Refresh sound data
		Sound_Update(snd.output, snd.sample_count);
		
		if (sms.console == CONSOLE_COLECO)
		{
			input.system = 0;
			coleco.keypad[0] = 0xff;
			coleco.keypad[1] = 0xff;
		}

		if (selectpressed == 1)
		{
            Menu();
            SDL_FillRect(sdl_screen, NULL, 0);
            input.system &= (IS_GG) ? ~INPUT_START : ~INPUT_PAUSE;
            selectpressed = 0;
		}
		
		Controls_papk3s();

		if(SDL_PollEvent(&event)) 
		{
			switch(event.type) 
			{
				case SDL_QUIT:
					quit = 1;
				return 0;
			}
		}
	}
	
	config_save();
	Cleanup();
	
	return 0;
}
