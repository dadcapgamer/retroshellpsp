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
SDL_Surface *sms_bitmap;
static SDL_Surface* img_background;
uint32_t countedFrames = 0;
uint32_t start;

static char home_path[256];

static SDL_Joystick* joystick[2];

extern SDL_Surface *font;
extern SDL_Surface *bigfontred;
extern SDL_Surface *bigfontwhite;

static uint8_t selectpressed = 0;
static uint8_t save_slot = 0;
static uint8_t quit = 0;

static void video_update()
{
	SDL_LockSurface(sdl_screen);
	switch(option.fullscreen) 
	{
        case 0: // native res
		if(sms.console == CONSOLE_GG) 
			bitmap_scale(48,0,160,144,160,144,256,sdl_screen->w-160,(uint16_t* restrict)sms_bitmap->pixels,(uint16_t* restrict)sdl_screen->pixels+(sdl_screen->w-160)/2+(sdl_screen->h-144)/2*sdl_screen->w);
		else 
			bitmap_scale(0,0,256,vdp.height,256,vdp.height,256,sdl_screen->w-256,(uint16_t* restrict)sms_bitmap->pixels,(uint16_t* restrict)sdl_screen->pixels+(sdl_screen->w-256)/2+(sdl_screen->h-vdp.height)/2*sdl_screen->w);
		break;
		case 1: // Fullscreen
		if(sms.console == CONSOLE_GG) 
			upscale_160x144_to_480x272((uint32_t* restrict)sdl_screen->pixels, (uint32_t* restrict)sms_bitmap->pixels+24);
		else 
			upscale_256xXXX_to_480x272((uint32_t* restrict)sdl_screen->pixels, (uint32_t* restrict)sms_bitmap->pixels, vdp.height);
		break;
        case 2: // aspect ratio
		if(sms.console == CONSOLE_GG) 
			upscale_160x144_to_320x272_for_480x272((uint32_t* restrict)sdl_screen->pixels, (uint32_t* restrict)sms_bitmap->pixels+24);
		else 
			upscale_256xXXX_to_384x272_for_480x272((uint32_t* restrict)sdl_screen->pixels, (uint32_t* restrict)sms_bitmap->pixels, vdp.height);
		break;
	}
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

static void Controls()
{
	int16_t x_move = 0, y_move = 0;
	uint8_t *keystate;
	keystate = SDL_GetKeyState(NULL);
	
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

		if(SDL_JoystickGetButton(joystick[i], 2) == SDL_PRESSED || (i == 0 && keystate[SDLK_LCTRL]))
			input.pad[i] |= INPUT_BUTTON2;
		else
			input.pad[i] &= ~INPUT_BUTTON2;
				
		if(SDL_JoystickGetButton(joystick[i], 1) == SDL_PRESSED || (i == 0 && keystate[SDLK_LALT]))
			input.pad[i] |= INPUT_BUTTON1;
		else
			input.pad[i] &= ~INPUT_BUTTON1;

		if(i == 0 && SDL_JoystickGetButton(joystick[i], 9) == SDL_PRESSED || keystate[SDLK_RETURN])
			input.system |= (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;
		else
			input.system &= (sms.console == CONSOLE_GG) ? ~INPUT_START : ~INPUT_PAUSE;
					
		if(SDL_JoystickGetButton(joystick[i], 8) == SDL_PRESSED)
			selectpressed = 1;
	}
	
	SDL_JoystickUpdate();	
}

static uint32_t sdl_controls_update_input(SDLKey k, int32_t p)
{
	if (sms.console == CONSOLE_COLECO)
    {
		coleco.keypad[0] = 0xff;
		coleco.keypad[1] = 0xff;
	}
	
	switch(k)
	{
		/* A USB keyboard can be plugged to the Arcade Mini so map it to*/
		case SDLK_0:
		case SDLK_KP0:
			coleco.keypad[0] = 0;
		break;
		case SDLK_1:
		case SDLK_KP1:
		case SDLK_TAB:
			coleco.keypad[0] = 1;
		break;
		case SDLK_2:
		case SDLK_KP2:
		case SDLK_BACKSPACE:
			coleco.keypad[0] = 2;
		break;
		case SDLK_3:
		case SDLK_KP3:
			coleco.keypad[0] = 3;
		break;
		case SDLK_4:
		case SDLK_KP4:
			coleco.keypad[0] = 4;
		break;
		case SDLK_5:
		case SDLK_KP5:
			coleco.keypad[0] = 5;
		break;
		case SDLK_6:
		case SDLK_KP6:
			coleco.keypad[0] = 6;
		break;
		case SDLK_7:
		case SDLK_KP7:
			coleco.keypad[0] = 7;
		break;
		case SDLK_8:
		case SDLK_KP8:
			coleco.keypad[0] = 8;
		break;
		case SDLK_9:
		case SDLK_KP9:
			coleco.keypad[0] = 9;
		break;
		case SDLK_DOLLAR:
		case SDLK_KP_MULTIPLY:
			coleco.keypad[0] = 10;
		break;
		case SDLK_ASTERISK:
		case SDLK_KP_MINUS:
			coleco.keypad[0] = 11;
		break;
		case SDLK_ESCAPE:
			if(p)
				selectpressed = 1;
		break;
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

void Menu()
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
        bitmap_scale(48,0,160,144,miniscreenwidth,miniscreenheight,256,0,(uint16_t* restrict)sms_bitmap->pixels,(uint16_t* restrict)miniscreen->pixels);
    else
        bitmap_scale(0,0,256,192,miniscreenwidth,miniscreenheight,256,0,(uint16_t* restrict)sms_bitmap->pixels,(uint16_t* restrict)miniscreen->pixels);
        
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
            if (option.fullscreen == 1)
                gfx_font_print(final_screen,5,105,bigfontred,"Stretched");
			else if (option.fullscreen == 2)
				gfx_font_print(final_screen,5,105,bigfontred,"Keep Aspect");
            else
				gfx_font_print(final_screen,5,105,bigfontred,"Native");
        }
        else
        {
            if (option.fullscreen == 1)
                gfx_font_print(final_screen,5,105,bigfontwhite,"Stretched");
			else if (option.fullscreen == 2)
				gfx_font_print(final_screen,5,105,bigfontwhite,"Keep Aspect");
            else
				gfx_font_print(final_screen,5,105,bigfontwhite,"Native");
        }

        if (currentselection == 5)
            gfx_font_print(final_screen,5,125,bigfontred,"Quit");
        else
            gfx_font_print(final_screen,5,125,bigfontwhite,"Quit");

        gfx_font_print_center(final_screen,sdl_screen->h-50-gfx_font_height(font),font,"SMS_SDL for the RS-97");
        gfx_font_print_center(final_screen,sdl_screen->h-40-gfx_font_height(font),font,"AMINI port by gameblabla");
        gfx_font_print_center(final_screen,sdl_screen->h-30-gfx_font_height(font),font,"See full credits on github:");
        gfx_font_print_center(final_screen,sdl_screen->h-20-gfx_font_height(font),font,"https://github.com/gameblabla/sms_sdl");


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
									if (option.fullscreen == 0)
										option.fullscreen = 2;
									else
										option.fullscreen--;
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
									if (option.fullscreen > 2)
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
									if (option.fullscreen == 0)
										option.fullscreen = 2;
									else
										option.fullscreen--;
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
									if (option.fullscreen > 2)
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
                    if (option.fullscreen > 2)
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
    {
        quit = 1;
	}
    else
		Sound_Unpause();
	
	if (miniscreen) SDL_FreeSurface(miniscreen);
	if (black_screen) SDL_FreeSurface(black_screen);
	if (final_screen) SDL_FreeSurface(final_screen);
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

static void Cleanup(void)
{
	if (sdl_screen) SDL_FreeSurface(sdl_screen);
	sdl_screen = NULL;
	
	if (sms_bitmap) SDL_FreeSurface(sms_bitmap);
	sms_bitmap = NULL;
	
	if (font) SDL_FreeSurface(font);
	font = NULL;
	
	if (bigfontwhite) SDL_FreeSurface(bigfontwhite);
	bigfontwhite = NULL;
	
	if (bigfontred) SDL_FreeSurface(bigfontred);
	bios.rom = NULL;
	
	for(uint8_t i=0;i<2;i++)
	{
		if (joystick[i]) SDL_JoystickClose(joystick[i]);
	}
	
	if (bios.rom) free(bios.rom);
	bios.rom = NULL;
	
	// Deinitialize audio and video output
	Sound_Close();
	
	SDL_Quit();

	// Shut down
	system_poweroff();
	system_shutdown();
}

int main (int argc, char *argv[]) 
{
	SDL_Event event;
	// Print Header
	fprintf(stdout, "%s %s\n", APP_NAME, APP_VERSION);
	
	if(argc < 2) 
	{
		fprintf(stderr, "Usage: ./smsplus [FILE]\n");
		Cleanup();
		return 0;
	}
	
	smsp_gamedata_set(argv[1]);
	
	memset(&option, 0, sizeof(option));
	
	option.fullscreen = 1;
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
	
	if (sms.console == CONSOLE_SMS || sms.console == CONSOLE_SMS2)
	{ 
		sms.use_fm = 1; 
	}
	
	strcpy(option.game_name, argv[1]);
	
	// Load ROM
	if(!load_rom(argv[1])) {
		fprintf(stderr, "Error: Failed to load %s.\n", argv[1]);
		Cleanup();
		return 0;
	}
	
	strcpy(option.game_name, argv[1]);
	
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_ENABLE);
	sdl_screen = SDL_SetVideoMode(480, 272, 16, SDL_HWSURFACE);
	SDL_SetCursor(0);
	
	// Needed in order to clear the cursor
	SDL_FillRect( sdl_screen, NULL, 0 );
	SDL_Flip(sdl_screen);
	
	sms_bitmap = SDL_CreateRGBSurface(SDL_SWSURFACE, VIDEO_WIDTH_SMS, 267, 16, 0, 0, 0, 0);
	
	img_background = SDL_LoadBMP("background.bmp");
	
    font = gfx_tex_load_tga_from_array(fontarray);
    bigfontwhite = gfx_tex_load_tga_from_array(bigfontwhitearray);
    bigfontred = gfx_tex_load_tga_from_array(bigfontredarray);
	
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

		if (selectpressed == 1)
		{
            Menu();
            SDL_FillRect(sdl_screen, NULL, 0);
            input.system &= (IS_GG) ? ~INPUT_START : ~INPUT_PAUSE;
            selectpressed = 0;
		}

		Controls();

		if (SDL_PollEvent(&event)) 
		{
			switch(event.type) 
			{
				case SDL_KEYUP:
					sdl_controls_update_input(event.key.keysym.sym, 0);
				break;
				case SDL_KEYDOWN:
					sdl_controls_update_input(event.key.keysym.sym, 1);
				break;
				case SDL_QUIT:
					quit = 1;
				break;
			}
		}
	}
	
	config_save();
	Cleanup();
	
	return 0;
}
