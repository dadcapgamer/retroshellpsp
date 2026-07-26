#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <libgen.h>

#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h> 

#include <linux/fb.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/keyboard.h>

#include "shared.h"
#include "scaler.h"
#include "smsplus.h"
#include "sound_output.h"

#include "font_drawing.h"

static gamedata_t gdata;
static char home_path[256];

static uint16_t* restrict buffer_fbdev[1];
static int fbfd = 0, keyboard_fd = 0;
static struct input_event data;

static struct fb_var_screeninfo orig_vinfo;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;

static size_t screensize = 0;

t_config option;

static uint8_t selectpressed = 0;
static uint8_t save_slot = 0;
static uint8_t quit = 0;

#ifdef SCALE2X_UPSCALER
static uint32_t dst_x, dst_w, dst_h;
static uint16_t* restrict scale2x_buf;
#endif

const int8_t upscalers_available = 1
#ifdef SCALE2X_UPSCALER
+1
#endif
;
static uint16_t* restrict sms_bitmap;

static void Clear_buffers()
{
	memset(buffer_fbdev[0], 0, screensize);
}

static inline void swap_buffers()
{
	//long screen = 0;
	//ioctl(fbfd, FBIO_WAITFORVSYNC, &screen);
	//"Pan" to the back buffer
	ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo);
}

static void video_update()
{
	switch(option.fullscreen) 
	{
		// Native
        case 0: 
		if(sms.console == CONSOLE_GG) 
			bitmap_scale(48,0,160,144,160,144,256,HOST_WIDTH_RESOLUTION-160,(uint16_t* restrict)sms_bitmap,(uint16_t* restrict)buffer_fbdev[0]+(HOST_WIDTH_RESOLUTION-160)/2+(HOST_HEIGHT_RESOLUTION-144)/2*HOST_WIDTH_RESOLUTION);
		else 
			bitmap_scale(0,0,256,vdp.height,256,(vdp.height),256,HOST_WIDTH_RESOLUTION-256,(uint16_t* restrict)sms_bitmap,(uint16_t* restrict)buffer_fbdev[0]+(HOST_WIDTH_RESOLUTION-256)/2+(HOST_HEIGHT_RESOLUTION-(vdp.height))/2*HOST_WIDTH_RESOLUTION);
		break;
		// Fullscreen
		case 1:
		if(sms.console == CONSOLE_GG) 
			upscale_160x144_to_320x240((uint32_t* restrict)buffer_fbdev[0], (uint32_t* restrict)sms_bitmap+24);
		else 
			upscale_SMS_to_320x240((uint32_t* restrict)buffer_fbdev[0], (uint32_t* restrict)sms_bitmap, vdp.height);
		break;
		// Hqx
		case 2:
#ifdef SCALE2X_UPSCALER
		if(sms.console == CONSOLE_GG) 
		{
			dst_x = 96;
			dst_w = 320;
			dst_h = 144*2;
		}
		else
		{
			uint32_t hide_left = (vdp.reg[0] & 0x20) ? 1 : 0;
			dst_x = hide_left ? 16 : 0;
			dst_w = (hide_left ? 248 : 256)*2;
			dst_h = vdp.height*2;
		}
		scale2x(sms_bitmap, scale2x_buf, 512, 1024, dst_w, vdp.height);
		bitmap_scale(dst_x,0,dst_w,dst_h,HOST_WIDTH_RESOLUTION,HOST_HEIGHT_RESOLUTION,512,0,scale2x_buf,buffer_fbdev[0]);
#endif
		break;
	}
	swap_buffers();
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
	int bytes;
	if (sms.console == CONSOLE_COLECO)
    {
		coleco.keypad[0] = 0xff;
		coleco.keypad[1] = 0xff;
	}
	
	bytes = read(keyboard_fd, &data, sizeof(data));
	
	//if (data.type == 0x01)
	{
		switch(data.code)
		{
				/* At least allow them to play on the lowest difficulties, Maybe this needs a better implementation ? */
			case 15:
				coleco.keypad[0] = 1;
			break;
			case 16:
				coleco.keypad[0] = 2;
			break;
			case 107:
			case 1:
				if (data.value == 1 && bytes > 0)
					selectpressed = 1;
				else
					selectpressed = 0;
			break;
			case 28:
				if (data.value == 1 && bytes > 0)
					input.system |= (sms.console == CONSOLE_GG) ? INPUT_START : INPUT_PAUSE;
				else if (data.value == 0)
					input.system &= (sms.console == CONSOLE_GG) ? ~INPUT_START : ~INPUT_PAUSE;
			break;
			case 103:
				if (data.value == 1 && bytes > 0)
				{
					input.pad[0] |= INPUT_UP;
				}
				else if (data.value == 0)
					input.pad[0] &= ~INPUT_UP;
			break;	
			case 108:
				if (data.value == 1 && bytes > 0)
				{
					input.pad[0] |= INPUT_DOWN;
				}
				else if (data.value == 0)
					input.pad[0] &= ~INPUT_DOWN;
			break;
			case 105:
				if (data.value == 1 && bytes > 0)
				{
					input.pad[0] |= INPUT_LEFT;
				}
				else if (data.value == 0)
					input.pad[0] &= ~INPUT_LEFT;
			break;	
			case 106:
				if (data.value == 1 && bytes > 0)
				{
					input.pad[0] |= INPUT_RIGHT;
				}
				else if (data.value == 0)
					input.pad[0] &= ~INPUT_RIGHT;
			break;
			case 56:
			case 42:
				if (data.value == 1 && bytes > 0)
					input.pad[0] |= INPUT_BUTTON2;
				else if (data.value == 0)
					input.pad[0] &= ~INPUT_BUTTON2;
			break;
			case 29:
			case 57:
				if (data.value == 1 && bytes > 0)
					input.pad[0] |= INPUT_BUTTON1;
				else if (data.value == 0)
					input.pad[0] &= ~INPUT_BUTTON1;
			break;
			default:
			break;
		}
	}
	
	if (sms.console == CONSOLE_COLECO) input.system = 0;
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
	uint32_t pressed = 0;
    uint32_t currentselection = 1;
    uint32_t miniscreenwidth = 128;
    uint32_t miniscreenheight = 128;
	char text[50];
	int bytes;
	
	Clear_buffers();
	Sound_Pause();
	

	while(((currentselection != 1) && (currentselection != 6)) || (!pressed))
	{
		bytes = read(keyboard_fd, &data, sizeof(data));
		pressed = 0;

		switch(data.code)
		{
			case 103:
				if (data.value == 1)
				{
					currentselection--;
					if (currentselection == 0)
						currentselection = 6;
				}
			break;	
			case 108:
				if (data.value == 1)
				{
					currentselection++;
					if (currentselection == 7)
						currentselection = 1;
				}
			break;
			case 105:
				if (data.value == 1)
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
						case 5:
							option.soundlevel--;
							if (option.soundlevel < 1)
							option.soundlevel = 4;
						break;
					}
				}
			break;	
			case 106:
				if (data.value == 1)
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
						case 5 :
							option.soundlevel++;
							if (option.soundlevel > 4)
							option.soundlevel = 1;
						break;
					}
				}
			break;
			case 29:
				if (data.value == 1)
				{
					pressed = 1;
					switch(currentselection)
					{
						case 5 :
							option.soundlevel++;
							if (option.soundlevel > 4)
							option.soundlevel = 0;
						break;
						case 4 :
							option.fullscreen++;
							if (option.fullscreen > upscalers_available)
							option.fullscreen = 0;
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
			break;
			default:
			break;
		}

		memset(buffer_fbdev[0], 0, screensize);

		print_string("SMS PLUS GX", TextWhite, 0, 105, 15, buffer_fbdev[0]);
		
		if (currentselection == 1) print_string("Continue", TextRed, 0, 5, 45, buffer_fbdev[0]);
		else  print_string("Continue", TextWhite, 0, 5, 45, buffer_fbdev[0]);
		
		snprintf(text, sizeof(text), "Load State %d", save_slot);
		
		if (currentselection == 2) print_string(text, TextRed, 0, 5, 65, buffer_fbdev[0]);
		else print_string(text, TextWhite, 0, 5, 65, buffer_fbdev[0]);
		
		snprintf(text, sizeof(text), "Save State %d", save_slot);
		
		if (currentselection == 3) print_string(text, TextRed, 0, 5, 85, buffer_fbdev[0]);
		else print_string(text, TextWhite, 0, 5, 85, buffer_fbdev[0]);
		

        if (currentselection == 4)
        {
			switch(option.fullscreen)
			{
				case 0:
					print_string("Scaling : Native", TextRed, 0, 5, 105, buffer_fbdev[0]);
				break;
				case 1:
					print_string("Scaling : Stretched", TextRed, 0, 5, 105, buffer_fbdev[0]);
				break;
				case 2:
					print_string("Scaling : EPX/Scale2x", TextRed, 0, 5, 105, buffer_fbdev[0]);
				break;
			}
        }
        else
        {
			switch(option.fullscreen)
			{
				case 0:
					print_string("Scaling : Native", TextWhite, 0, 5, 105, buffer_fbdev[0]);
				break;
				case 1:
					print_string("Scaling : Stretched", TextWhite, 0, 5, 105, buffer_fbdev[0]);
				break;
				case 2:
					print_string("Scaling : EPX/Scale2x", TextWhite, 0, 5, 105, buffer_fbdev[0]);
				break;
			}
        }

		snprintf(text, sizeof(text), "Sound volume : %d", option.soundlevel);
		
		if (currentselection == 5) print_string(text, TextRed, 0, 5, 125, buffer_fbdev[0]);
		else print_string(text, TextWhite, 0, 5, 125, buffer_fbdev[0]);
		
		if (currentselection == 6) print_string("Quit", TextRed, 0, 5, 145, buffer_fbdev[0]);
		else print_string("Quit", TextWhite, 0, 5, 145, buffer_fbdev[0]);
		
		print_string("Based on SMS Plus by Charles Mcdonald", TextWhite, 0, 5, 175, buffer_fbdev[0]);
		print_string("Fork of SMS Plus GX by gameblabla", TextWhite, 0, 5, 190, buffer_fbdev[0]);
		print_string("Scaler : Alekmaul", TextWhite, 0, 5, 205, buffer_fbdev[0]);
		print_string("Text drawing : n2DLib", TextWhite, 0, 5, 220, buffer_fbdev[0]);
		

		if(IS_GG)
		{
			bitmap_scale(48,0,
			160,144,
			miniscreenwidth,miniscreenheight,
			256, HOST_WIDTH_RESOLUTION-miniscreenwidth,
			(uint16_t* restrict)sms_bitmap,(uint16_t* restrict)buffer_fbdev[0]+(HOST_WIDTH_RESOLUTION+45)/2+(HOST_HEIGHT_RESOLUTION-vdp.height+24)/2*HOST_WIDTH_RESOLUTION);
		}
		else
		{
			bitmap_scale(0,0,
			256,vdp.height,
			miniscreenwidth,miniscreenheight,
			256, HOST_WIDTH_RESOLUTION-miniscreenwidth,
			(uint16_t* restrict)sms_bitmap,(uint16_t* restrict)buffer_fbdev[0]+(HOST_WIDTH_RESOLUTION+45)/2+(HOST_HEIGHT_RESOLUTION-vdp.height+24)/2*HOST_WIDTH_RESOLUTION);
		}
		
		swap_buffers();
	}
	
	Clear_buffers();
	
    if (currentselection == 6)
        quit = 1;
    else
		Sound_Unpause();
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
#ifdef SCALE2X_UPSCALER
	if (scale2x_buf) free(scale2x_buf);
	scale2x_buf = NULL;
#endif
	if (sms_bitmap) free(sms_bitmap);
	sms_bitmap = NULL;
	
	if (buffer_fbdev[0]) free(buffer_fbdev[0]);
	munmap(buffer_fbdev[0], screensize);
	buffer_fbdev[0] = NULL;
	
	if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &orig_vinfo)) 
	{
		printf("Error re-setting variable information.\n");
	}
	
	if (fbfd) close(fbfd);
	fbfd = NULL;
	
	if (keyboard_fd) close(keyboard_fd);
	keyboard_fd = NULL;
	
	if (bios.rom) free(bios.rom);
	bios.rom = NULL;
	
	// Deinitialize audio and video output
	Sound_Close();
	
	// Shut down
	system_poweroff();
	system_shutdown();	
}

int main (int argc, char *argv[]) 
{
	if(argc < 2) 
	{
		fprintf(stderr, "Usage: ./smsplus [FILE]\n");
		return 0;
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
		sms.use_fm = 1; 
	
	// Load ROM
	if(!load_rom(argv[1])) {
		fprintf(stderr, "Error: Failed to load %s.\n", argv[1]);
		Cleanup();
		return 1;
	}

	keyboard_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	if (!keyboard_fd) 
	{
		printf("Error: Cannot open Keyboard\n");
		return 1;
	}
	
	fbfd = open("/dev/fb0", O_RDWR | O_SYNC);
	if (!fbfd) 
	{
		printf("Error: cannot open framebuffer device.\n");
		return 1 ;
	}
	// Get variable screen information
	if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error reading variable information.\n");
	}

	// Store for reset (copy vinfo to vinfo_orig)
	memcpy(&orig_vinfo, &vinfo, sizeof(struct fb_var_screeninfo));

	// Change variable info
	vinfo.bits_per_pixel = 16;
	if (ioctl(fbfd, FBIOPUT_VSCREENINFO, &vinfo)) {
		printf("Error : Setting variable information.\n");
	}
	
	// Get fixed screen information
	if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {
		printf("Error reading fixed information.\n");
	}
	
	screensize = (320*240)*2;
	buffer_fbdev[0] = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	sms_bitmap = malloc((VIDEO_WIDTH_SMS*267)*sizeof(uint16_t));

    vinfo.xoffset = 0;
    vinfo.yoffset = 0;
	ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo); 

#ifdef SCALE2X_UPSCALER
	scale2x_buf = malloc(((VIDEO_WIDTH_SMS*2)*(267*2))*sizeof(uint16_t));
#endif
	
	fprintf(stdout, "CRC : %08X\n", cart.crc);
	
	// Set parameters for internal bitmap
	bitmap.width = VIDEO_WIDTH_SMS;
	bitmap.height = VIDEO_HEIGHT_SMS;
	bitmap.depth = 16;
	bitmap.data = (uint8_t *)sms_bitmap;
	bitmap.pitch = 512;
	bitmap.viewport.w = VIDEO_WIDTH_SMS;
	bitmap.viewport.h = VIDEO_HEIGHT_SMS;
	bitmap.viewport.x = 0x00;
	bitmap.viewport.y = 0x00;
	
	//sms.territory = settings.misc_region;
	
	bios_init();

	// Initialize all systems and power on
	system_poweron();

	Sound_Init();
	
	Clear_buffers();
	
	// Loop until the user closes the window
	while (!quit) 
	{
		// Execute frame(s)
		system_frame(0);
		
		// Refresh sound data
		Sound_Update(snd.output, snd.sample_count);
		
		// Refresh video data
		video_update();
		
		if (selectpressed == 1)
		{
			Menu();
            input.system &= (IS_GG) ? ~INPUT_START : ~INPUT_PAUSE;
            selectpressed = 0;
		}
		
		Controls();
	}
	
	config_save();
	Cleanup();
	
	return 0;
}
