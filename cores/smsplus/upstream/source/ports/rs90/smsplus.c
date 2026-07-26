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
#include "font_drawing.h"

#ifdef NOYUV
SDL_Color palette_8bpp[256];
#else
static uint8_t drm_palette[3][256];
#endif

static gamedata_t gdata;

t_config option;

static SDL_Surface* sdl_screen, *backbuffer;
SDL_Surface *sms_bitmap;

static char home_path[256];

static uint_fast8_t save_slot = 0;
static uint_fast8_t quit = 0;

#ifndef NOYUV
#define UINT16_16(val) ((uint32_t)(val * (float)(1<<16)))
static const uint32_t YUV_MAT[3][3] = {
	{UINT16_16(0.2999f),   UINT16_16(0.587f),    UINT16_16(0.114f)},
	{UINT16_16(0.168736f), UINT16_16(0.331264f), UINT16_16(0.5f)},
	{UINT16_16(0.5f),      UINT16_16(0.418688f), UINT16_16(0.081312f)}
};
static uint8_t* dst_yuv[3];
#endif

static uint_fast8_t forcerefresh = 0;
static uint32_t update_window_size(uint32_t w, uint32_t h);

/* This is solely relying on the IPU chip implemented in the kernel
 * for centering, scaling (with bilinear filtering) etc...
*/
static void video_update(void)
{
	uint_fast16_t height, width;
	uint_fast16_t i, pixels_shifting_remove;
	uint_fast8_t a, plane;
	
	if (sms.console == CONSOLE_GG)
	{
		/*	pixels_shifting_remove is used to skip the parts of the screen that we don't want.
			This is much faster than copying it to another buffer.
		*/
		pixels_shifting_remove = (256 * 24) + 48;
		height = 144;
		width = 160;
		if (sdl_screen->h != 144 || forcerefresh == 1)
		{
			update_window_size(160, 144);
			forcerefresh = 0;
		}
	}
	else
	{
		pixels_shifting_remove = (vdp.reg[0] & 0x20) ? 8 : 0;
		height = vdp.height;
		width = 256 - pixels_shifting_remove;
		if (width != sdl_screen->w || sdl_screen->h != vdp.height || forcerefresh == 1)
		{
			update_window_size(width, vdp.height);
			forcerefresh = 0;
		}
	}
	
	/* Yes, this mess is really for the 8-bits palette mode.*/
	if (bitmap.pal.update == 1){
		for(i = 0; i < PALETTE_SIZE; i += 1)
		{
			if(bitmap.pal.dirty[i])
			{
				for(a=0;a<8;a++)
				{
					#ifdef NOYUV
					palette_8bpp[i+(a*32)].r = (bitmap.pal.color[i][0]);
					palette_8bpp[i+(a*32)].g = (bitmap.pal.color[i][1]);
					palette_8bpp[i+(a*32)].b = (bitmap.pal.color[i][2]);
					#else
					/* Set DRM palette */
					drm_palette[0][i+(a*32)] = ( ( UINT16_16(  0) + YUV_MAT[0][0] * bitmap.pal.color[i][0] + YUV_MAT[0][1] * bitmap.pal.color[i][1] + YUV_MAT[0][2] * bitmap.pal.color[i][2]) >> 16 );
					drm_palette[1][i+(a*32)] = ( ( UINT16_16(128) - YUV_MAT[1][0] * bitmap.pal.color[i][0] - YUV_MAT[1][1] * bitmap.pal.color[i][1] + YUV_MAT[1][2] * bitmap.pal.color[i][2]) >> 16 );
					drm_palette[2][i+(a*32)] = ( ( UINT16_16(128) + YUV_MAT[2][0] * bitmap.pal.color[i][0] - YUV_MAT[2][1] * bitmap.pal.color[i][1] - YUV_MAT[2][2] * bitmap.pal.color[i][2]) >> 16 );
					#endif
				}
			}
		}
		#ifdef NOYUV
		SDL_SetPalette(sms_bitmap, SDL_LOGPAL|SDL_PHYSPAL, palette_8bpp, 0, 256);
		SDL_SetPalette(sdl_screen, SDL_LOGPAL|SDL_PHYSPAL, palette_8bpp, 0, 256);
		#endif
	}
	
	#ifdef NOYUV
	if (pixels_shifting_remove) sms_bitmap->pixels += pixels_shifting_remove;
	SDL_BlitSurface(sms_bitmap, NULL, sdl_screen, NULL);
	if (pixels_shifting_remove) sms_bitmap->pixels -= pixels_shifting_remove;
	#else
	
	/* This code is courtesy of Slaneesh. Many thanks to him for the help and special thanks also to Johnny too. 
	 * However Johnny's code is not used because during my testing it was slower.
	 * He still helped me understand the YUV code though, so thanks.
	 * */
	uint8_t *srcbase = sms_bitmap->pixels + pixels_shifting_remove;
	dst_yuv[0] = sdl_screen->pixels;
	dst_yuv[1] = dst_yuv[0] + height * sdl_screen->pitch;
	dst_yuv[2] = dst_yuv[1] + height * sdl_screen->pitch;
    for (plane=0; plane<3; plane++) /* The three Y, U and V planes */
    {
        uint32_t y;
        register uint8_t *pal = drm_palette[plane];
        for (y=0; y < height; y++)   /* The number of lines to copy */
        {
            register uint8_t *src = srcbase + (y*sms_bitmap->w);
            register uint8_t *end = src + width;
            register uint32_t *dst = (uint32_t *)&dst_yuv[plane][width * y];

             __builtin_prefetch(pal, 0, 1 );
             __builtin_prefetch(src, 0, 1 );
             __builtin_prefetch(dst, 1, 0 );

            while (src < end)       /* The actual line data to copy */
            {
                register uint32_t pix;
                pix  = pal[*src++];
                pix |= (pal[*src++])<<8;
                pix |= (pal[*src++])<<16;
                pix |= (pal[*src++])<<24;
                *dst++ = pix;
            }
        }
    }
	#endif
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
                printf("State Saved. >%s\n",stpath);
			}
            else
                printf("State Save Failed! >%s\n",stpath);
			break;
		
		case 1:
			fd = fopen(stpath, "rb");
			if (fd) {
				system_load_state(fd);
				fclose(fd);
                printf("State Saved. >%s\n",stpath);
			}
            else
                printf("State Save Failed! >%s\n",stpath);
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
                    printf("SRAM Saved. >%s\n", gdata.sramfile);
				}
                else
                    printf("SRAM Save Failed! >%s\n", gdata.sramfile);
			}
			break;
		
		case SRAM_LOAD:
			fd = fopen(gdata.sramfile, "rb");
			if (fd) 
			{
				sms.save = 1;
				fread(sram, 0x8000, 1, fd);
				fclose(fd);
                printf("SRAM Loaded. >%s\n", gdata.sramfile);
			}
			else{
                printf("SRAM Load Failed! >%s\n", gdata.sramfile);
				memset(sram, 0x00, 0x8000);
            }
			break;
	}
}

static uint32_t sdl_controls_update_input_down(SDLKey k)
{
	if (k == option.config_buttons[CONFIG_BUTTON_UP])
	{
		input.pad[0] |= INPUT_UP;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_LEFT])
	{
		input.pad[0] |= INPUT_LEFT;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_RIGHT])
	{
		input.pad[0] |= INPUT_RIGHT;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_DOWN])
	{
		input.pad[0] |= INPUT_DOWN;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_BUTTON1])
	{
		input.pad[0] |= INPUT_BUTTON2;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_BUTTON2])
	{
		input.pad[0] |= INPUT_BUTTON1;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_START])
	{
		input.system |= (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;
	}

	/* We can't use switch... case because we are not using constants */
	if (sms.console == CONSOLE_COLECO)
    {
		input.system = 0;
		coleco.keypad[0] = 0xff;
		coleco.keypad[1] = 0xff;
		
		if (k == option.config_buttons[CONFIG_BUTTON_ONE]) coleco.keypad[0] = 1;
		else if (k == option.config_buttons[CONFIG_BUTTON_TWO]) coleco.keypad[0] = 2;
		else if (k == option.config_buttons[CONFIG_BUTTON_THREE]) coleco.keypad[0] = 3;
		else if (k == option.config_buttons[CONFIG_BUTTON_FOUR]) coleco.keypad[0] = 4;
		else if (k == option.config_buttons[CONFIG_BUTTON_FIVE]) coleco.keypad[0] = 5;
		else if (k == option.config_buttons[CONFIG_BUTTON_SIX]) coleco.keypad[0] = 6;
		else if (k == option.config_buttons[CONFIG_BUTTON_SEVEN]) coleco.keypad[0] = 7;
		else if (k == option.config_buttons[CONFIG_BUTTON_EIGHT]) coleco.keypad[0] = 8;
		else if (k == option.config_buttons[CONFIG_BUTTON_NINE]) coleco.keypad[0] = 9;
		else if (k == option.config_buttons[CONFIG_BUTTON_DOLLARS]) coleco.keypad[0] = 10;
		else if (k == option.config_buttons[CONFIG_BUTTON_ASTERISK]) coleco.keypad[0] = 11;
	}
	
	return 1;
}

static uint32_t sdl_controls_update_input_release(SDLKey k)
{
	if (k == option.config_buttons[CONFIG_BUTTON_UP])
	{
		input.pad[0] &= ~INPUT_UP;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_LEFT])
	{
		input.pad[0] &= ~INPUT_LEFT;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_RIGHT])
	{
		input.pad[0] &= ~INPUT_RIGHT;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_DOWN])
	{
		input.pad[0] &= ~INPUT_DOWN;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_BUTTON1])
	{
		input.pad[0] &= ~INPUT_BUTTON2;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_BUTTON2])
	{
		input.pad[0] &= ~INPUT_BUTTON1;
	}
	else if (k == option.config_buttons[CONFIG_BUTTON_START])
	{
		input.system &= (sms.console == CONSOLE_GG) ? ~INPUT_START : ~INPUT_PAUSE;
	}
	
	if (sms.console == CONSOLE_COLECO) input.system = 0;
	
	return 1;
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
	unsigned long i;
	// Set paths, create directories
	snprintf(home_path, sizeof(home_path), "%s/.smsplus/", getenv("HOME"));
	
	if (mkdir(home_path, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", home_path, errno);
	}
	
	// Set the game name
	snprintf(gdata.gamename, sizeof(gdata.gamename), "%s", basename(filename));
	
	// Strip the file extension off
	for (i = strlen(gdata.gamename) - 1; i > 0; i--) {
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


static const char* Return_Text_Button(uint32_t button)
{
	switch(button)
	{
		/* UP button */
		case 273:
			return "DPAD UP";
		/* DOWN button */
		case 274:
			return "DPAD DOWN";
		/* LEFT button */
		case 276:
			return "DPAD LEFT";
		/* RIGHT button */
		case 275:
			return "DPAD RIGHT";
		/* A button */
		case 306:
			return "A button";
		/* B button */
		case 308:
			return "B button";
		/* X button */
		case 304:
			return "X button";
		/* Y button */
		case 32:
			return "Y button";
		/* L button */
		case 9:
			return "L Shoulder";
		/* R button */
		case 8:
			return "R Shoulder";
		/* Start */
		case 13:
			return "Start button";
		case 27:
			return "Select button";
		default:
			return "...";
	}	
}

static void Input_Remapping()
{
	SDL_Event Event;
	char text[50];
	uint32_t pressed = 0;
	int32_t currentselection = 1;
	uint32_t exit_map = 0;
	
	uint8_t menu_config_input = 0;
	
	while(!(currentselection == -2 && pressed == 1))
	{
		pressed = 0;
		SDL_FillRect( backbuffer, NULL, 0 );
		
        while (SDL_PollEvent(&Event))
        {
            if (Event.type == SDL_KEYDOWN)
            {
                switch(Event.key.keysym.sym)
                {
                    case SDLK_UP:
                        currentselection--;
                        if (currentselection < 1)
                        {
							currentselection = 1;
						}
                        break;
                    case SDLK_DOWN:
                        currentselection++;
                        switch(menu_config_input)
                        {
							case 0:
								if (currentselection > 7)
									currentselection = 7;
							break;
							case 1:
								if (currentselection > 11)
									currentselection = 11;
							break;
						}
                        break;
                    case SDLK_LEFT:
						currentselection -= 8;
						if (currentselection < 1) currentselection = 1;
                    break;
                    case SDLK_RIGHT:
						if (menu_config_input == 1)
						{
							currentselection += 8;
							if (currentselection > 11) currentselection = 10;
						}
                    break;
                    case SDLK_LCTRL:
                    case SDLK_RETURN:
                        pressed = 1;
					break;
                    case SDLK_TAB:
                        menu_config_input = 0;
                        currentselection = 1;
					break;
                    case SDLK_BACKSPACE:
                        if (sms.console == CONSOLE_COLECO) 
                        {
							menu_config_input = 1;
						}
						currentselection = 1;
					break;
                    case SDLK_LALT:
                        pressed = 1;
                        currentselection = -2;
					break;
                    case SDLK_ESCAPE:
						option.config_buttons[currentselection - 1] = 0;
					break;
					default:
					break;
                }
            }
        }

        if (pressed)
        {
            switch(currentselection)
            {
				case -2:
					/* Exits */
				break;
                default:
					SDL_FillRect( backbuffer, NULL, 0 );
					print_string("Press button for mapping", TextWhite, TextBlue, 24, 64, backbuffer->pixels);
					SDL_BlitSurface(backbuffer, NULL, sdl_screen, NULL);
					SDL_Flip(sdl_screen);
					exit_map = 0;
					while( !exit_map )
					{
						while (SDL_PollEvent(&Event))
						{
							if (Event.type == SDL_KEYDOWN)
							{
								option.config_buttons[(currentselection - 1) + (menu_config_input ? 7 : 0) ] = Event.key.keysym.sym;
								exit_map = 1;
							}
						}
					}
				break;
            }
        }

		print_string("[A] : Map, [B] : Exit", TextWhite, TextBlue, 20, 8, backbuffer->pixels);
		
		if (menu_config_input == 0)
		{
			snprintf(text, sizeof(text), "UP : %s\n", Return_Text_Button(option.config_buttons[0]));
			if (currentselection == 1) print_string(text, TextRed, 0, 5, 25, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 25, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "DOWN : %s\n", Return_Text_Button(option.config_buttons[1]));
			if (currentselection == 2) print_string(text, TextRed, 0, 5, 45, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 45, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "LEFT : %s\n", Return_Text_Button(option.config_buttons[2]));
			if (currentselection == 3) print_string(text, TextRed, 0, 5, 65, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 65, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "RIGHT : %s\n", Return_Text_Button(option.config_buttons[3]));
			if (currentselection == 4) print_string(text, TextRed, 0, 5, 85, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 85, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "A : %s\n", Return_Text_Button(option.config_buttons[4]));
			if (currentselection == 5) print_string(text, TextRed, 0, 5, 105, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 105, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "B : %s\n", Return_Text_Button(option.config_buttons[5]));
			if (currentselection == 6) print_string(text, TextRed, 0, 5, 125, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 125, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "START : %s\n", Return_Text_Button(option.config_buttons[6]));
			if (currentselection == 7) print_string(text, TextRed, 0, 5, 145, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 145, backbuffer->pixels);
		}
		else if (menu_config_input == 1)
		{
			snprintf(text, sizeof(text), "[*] : %s\n", Return_Text_Button(option.config_buttons[7]));
			if (currentselection == 1) print_string(text, TextRed, 0, 5, 25, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 25, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[#] : %s\n", Return_Text_Button(option.config_buttons[8]));
			if (currentselection == 2) print_string(text, TextRed, 0, 5, 45, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 45, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[1] : %s\n", Return_Text_Button(option.config_buttons[9]));
			if (currentselection == 3) print_string(text, TextRed, 0, 5, 65, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 65, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[2] : %s\n", Return_Text_Button(option.config_buttons[10]));
			if (currentselection == 4) print_string(text, TextRed, 0, 5, 85, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 85, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[3] : %s\n", Return_Text_Button(option.config_buttons[11]));
			if (currentselection == 5) print_string(text, TextRed, 0, 5, 105, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 105, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[4] : %s\n", Return_Text_Button(option.config_buttons[12]));
			if (currentselection == 6) print_string(text, TextRed, 0, 5, 125, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 125, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[5] : %s\n", Return_Text_Button(option.config_buttons[13]));
			if (currentselection == 7) print_string(text, TextRed, 0, 5, 145, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 145, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[6] : %s\n", Return_Text_Button(option.config_buttons[14]));
			if (currentselection == 8) print_string(text, TextRed, 0, 5, 145, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 5, 145, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[7] : %s\n", Return_Text_Button(option.config_buttons[15]));
			if (currentselection == 9) print_string(text, TextRed, 0, 145, 25, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 145, 25, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[8] : %s\n", Return_Text_Button(option.config_buttons[16]));
			if (currentselection == 10) print_string(text, TextRed, 0, 145, 45, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 145, 45, backbuffer->pixels);
			
			snprintf(text, sizeof(text), "[9] : %s\n", Return_Text_Button(option.config_buttons[17]));
			if (currentselection == 11) print_string(text, TextRed, 0, 145, 65, backbuffer->pixels);
			else print_string(text, TextWhite, 0, 145, 65, backbuffer->pixels);
		}
		SDL_BlitSurface(backbuffer, NULL, sdl_screen, NULL);
		SDL_Flip(sdl_screen);
	}
	
}

static void Menu()
{
	uint_fast8_t i;
	char text[50];
    int16_t pressed = 0;
    int16_t currentselection = 1;
    SDL_Event Event;
    
    sdl_screen = SDL_SetVideoMode(HOST_WIDTH_RESOLUTION, HOST_HEIGHT_RESOLUTION, 16, SDL_SWSURFACE);
    
	Sound_Pause();
    
    while (((currentselection != 1) && (currentselection != 7)) || (!pressed))
    {
        pressed = 0;
 		SDL_FillRect( backbuffer, NULL, 0 );

		print_string("SMS Plus GX", TextWhite, 0, 72, 15, backbuffer->pixels);
		
		if (currentselection == 1) print_string("Continue", TextBlue, 0, 5, 27, backbuffer->pixels);
		else  print_string("Continue", TextWhite, 0, 5, 27, backbuffer->pixels);
		
		snprintf(text, sizeof(text), "Load State %d", save_slot);
		
		if (currentselection == 2) print_string(text, TextBlue, 0, 5, 39, backbuffer->pixels);
		else print_string(text, TextWhite, 0, 5, 39, backbuffer->pixels);
		
		snprintf(text, sizeof(text), "Save State %d", save_slot);
		
		if (currentselection == 3) print_string(text, TextBlue, 0, 5, 51, backbuffer->pixels);
		else print_string(text, TextWhite, 0, 5, 51, backbuffer->pixels);
		
		if (currentselection == 4) print_string("Input remapping", TextBlue, 0, 5, 63, backbuffer->pixels);
		else print_string("Input remapping", TextWhite, 0, 5, 63, backbuffer->pixels);
		
		snprintf(text, sizeof(text), "FM Sound : %d", option.fm);
		
		if (currentselection == 5) print_string(text, TextBlue, 0, 5, 75, backbuffer->pixels);
		else print_string(text, TextWhite, 0, 5, 75, backbuffer->pixels);
		
		if (currentselection == 6) print_string("Reset", TextBlue, 0, 5, 87, backbuffer->pixels);
		else print_string("Reset", TextWhite, 0, 5, 87, backbuffer->pixels);

        if (currentselection == 7) print_string("Quit", TextBlue, 0, 5, 99, backbuffer->pixels);
		else print_string("Quit", TextWhite, 0, 5, 99, backbuffer->pixels);

        
		print_string("By gameblabla, ekeeke", TextWhite, 0, 5, 145, backbuffer->pixels);

        while (SDL_PollEvent(&Event))
        {
            if (Event.type == SDL_KEYDOWN)
            {
                switch(Event.key.keysym.sym)
                {
                    case SDLK_UP:
                        currentselection--;
                        if (currentselection == 0)
                            currentselection = 7;
                        break;
                    case SDLK_DOWN:
                        currentselection++;
                        if (currentselection == 8)
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
							case 5:
								if (option.fm > 0) option.fm--;
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
							case 5:
								if (option.fm < 1) option.fm++;
							break;
                        }
                        break;
					default:
					break;
                }
            }
            else if (Event.type == SDL_QUIT)
            {
				currentselection = 8;
			}
        }

        if (pressed)
        {
            switch(currentselection)
            {
                case 6:
                    //reset
                    Sound_Close();
                    Sound_Init();
                    system_poweron();
                    currentselection = 1;
                    break;
				case 4:
					Input_Remapping();
				break;
                case 2 :
                    smsp_state(save_slot, 1);
					currentselection = 1;
                    break;
                case 3 :
					smsp_state(save_slot, 0);
					currentselection = 1;
				break;
				default:
				break;
            }
        }

		SDL_BlitSurface(backbuffer, NULL, sdl_screen, NULL);
		SDL_Flip(sdl_screen);
    }
    sms.use_fm = option.fm;
    for(i=0;i<3;i++)
    {
		SDL_FillRect(sdl_screen, NULL, 0);
		SDL_Flip(sdl_screen);
	}
    
    if (currentselection == 7)
        quit = 1;
	else
		Sound_Unpause();
}


static void config_load()
{
	uint_fast8_t i;
	char config_path[256];
	snprintf(config_path, sizeof(config_path), "%sconfig.cfg", home_path);
	FILE* fp;
	
	fp = fopen(config_path, "rb");
	if (fp)
	{
		fread(&option, sizeof(option), sizeof(int8_t), fp);
		fclose(fp);
        printf("Config loaded. >%s\n",config_path);
	}
	else
	{
        printf("Config NOT loaded. >%s\n",config_path);
        
		for (i = 0; i < sizeof(option); i++)
		{
			option.config_buttons[i] = 0;
		}

		/* Default mapping for the Bittboy in case loading configuration file fails */
		option.config_buttons[CONFIG_BUTTON_UP] = SDLK_UP;
		option.config_buttons[CONFIG_BUTTON_DOWN] = SDLK_DOWN;
		option.config_buttons[CONFIG_BUTTON_LEFT] = SDLK_LEFT;
		option.config_buttons[CONFIG_BUTTON_RIGHT] = SDLK_RIGHT;
		
		option.config_buttons[CONFIG_BUTTON_BUTTON1] = SDLK_LCTRL;
		option.config_buttons[CONFIG_BUTTON_BUTTON2] = SDLK_LALT;
		
		option.config_buttons[CONFIG_BUTTON_START] = SDLK_RETURN;
	}
}

static void config_save()
{
	char config_path[256];
	snprintf(config_path, sizeof(config_path), "%sconfig.cfg", home_path);
	FILE* fp;
	
	fp = fopen(config_path, "wb");
	if (fp)
	{
		fwrite(&option, sizeof(option), sizeof(int8_t), fp);
		fclose(fp);
        printf("Config Saved. >%s\n",config_path);
	}
    else
        printf("Config Save Failed! >%s\n",config_path);
}


static void Cleanup(void)
{
	if (sdl_screen) SDL_FreeSurface(sdl_screen);
	if (sms_bitmap) SDL_FreeSurface(sms_bitmap);
	if (backbuffer) SDL_FreeSurface(backbuffer);
	if (bios.rom) free(bios.rom);
	
	// Deinitialize audio and video output
	Sound_Close();
	
	SDL_Quit();

	// Shut down
	system_poweroff();
	system_shutdown();	
}

uint32_t update_window_size(uint32_t w, uint32_t h)
{
	if (h == 0) h = 192;
#ifdef NOYUV
	sdl_screen = SDL_SetVideoMode(w, h, 8, SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_HWPALETTE);
#else
	sdl_screen = SDL_SetVideoMode(w, h, 24, SDL_HWSURFACE | SDL_TRIPLEBUF | SDL_YUV444 | SDL_ANYFORMAT | SDL_FULLSCREEN);
#endif
	return 0;
}

int main (int argc, char *argv[]) 
{
	uint_fast32_t i;
	SDL_Event event;
	
	if(argc < 2) 
	{
		fprintf(stderr, "Usage: ./smsplus [FILE]\n");
		return 0;
	}
	
	smsp_gamedata_set(argv[1]);
	
	memset(&option, 0, sizeof(option));
	
	option.fm = 1;
	option.spritelimit = 1;
	option.tms_pal = 2;
	option.console = 0;
	option.nosound = 0;
	option.soundlevel = 2;
	
	config_load();

	option.console = 0;
	
	strcpy(option.game_name, argv[1]);
	
	// Force Colecovision mode if extension is .col
	if (strcmp(strrchr(argv[1], '.'), ".col") == 0) option.console = 6;
	// Sometimes Game Gear games are not properly detected, force them accordingly
	else if (strcmp(strrchr(argv[1], '.'), ".gg") == 0) option.console = 3;

	// Load ROM
	if(!load_rom(argv[1])) {
		fprintf(stderr, "Error: Failed to load %s.\n", argv[1]);
		Cleanup();
		return 0;
	}
	
	/* Force 50 Fps refresh rate for PAL only games */
	if (sms.display == DISPLAY_PAL)
	{
		setenv("SDL_VIDEO_REFRESHRATE", "50", 0);
	}
	else
	{
		setenv("SDL_VIDEO_REFRESHRATE", "60", 0);
	}

	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	if (update_window_size(VIDEO_WIDTH_SMS, 240))
	{
		fprintf(stdout, "Could not create display, exiting\n");	
		Cleanup();
		return 0;
	}
	backbuffer = SDL_CreateRGBSurface(SDL_SWSURFACE, HOST_WIDTH_RESOLUTION, HOST_HEIGHT_RESOLUTION, 16, 0, 0, 0, 0);
	sms_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, VIDEO_WIDTH_SMS, 267, 8, 0, 0, 0, 0);
	
	SDL_FillRect(sms_bitmap, NULL, 0 );
	for(i=0;i<3;i++)
	{
		SDL_FillRect(sdl_screen, NULL, 0 );
		SDL_Flip(sdl_screen);
	}
	fprintf(stdout, "CRC : %08X\n", cart.crc);
	
	// Set parameters for internal bitmap
	bitmap.width = VIDEO_WIDTH_SMS;
	bitmap.height = VIDEO_HEIGHT_SMS;
	bitmap.depth = 8;
	bitmap.data = (uint8_t *)sms_bitmap->pixels;
	bitmap.pitch = sms_bitmap->pitch;
	bitmap.viewport.w = VIDEO_WIDTH_SMS;
	bitmap.viewport.h = VIDEO_HEIGHT_SMS;
	bitmap.viewport.x = 0x00;
	bitmap.viewport.y = 0x00;
	
	//sms.territory = settings.misc_region;
	if (sms.console == CONSOLE_SMS || sms.console == CONSOLE_SMS2)
	{ 
		sms.use_fm = option.fm;
	}
	
	bios_init();
	
	// Initialize all systems and power on
	system_poweron();
	
	Sound_Init();
	
	for(i = 0; i < PALETTE_SIZE; i += 1)
	{
		if(bitmap.pal.dirty[i])
		{
			#ifdef NOYUV
			palette_8bpp[i].r = 0;
			palette_8bpp[i].g = 0;
			palette_8bpp[i].b = 0;
			#else
			drm_palette[0][i] = 0;
			drm_palette[1][i] = 0;
			drm_palette[2][i] = 0;
			#endif
		}
	}
	
#ifdef NOYUV
	SDL_SetPalette(sms_bitmap, SDL_LOGPAL|SDL_PHYSPAL, palette_8bpp, 0, 256);
#endif

	forcerefresh = 1;
	
	// Loop until the user closes the window
	while (!quit) 
	{
		if (SDL_PollEvent(&event)) 
		{
			switch(event.type) 
			{
				case SDL_KEYUP:
					sdl_controls_update_input_release(event.key.keysym.sym);
				break;
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE)
					{
						Menu();
						input.system &= (IS_GG) ? ~INPUT_START : ~INPUT_PAUSE;
						forcerefresh = 1;
					}
					sdl_controls_update_input_down(event.key.keysym.sym);
				break;
				case SDL_QUIT:
					quit = 1;
				break;
			}
		}

		// Execute frame(s)
		system_frame(0);
		
		// Refresh sound data
		Sound_Update(snd.output, snd.sample_count);
		
		// Refresh video data
		video_update();
	}
	
	config_save();
	Cleanup();
	
	return 0;
}
