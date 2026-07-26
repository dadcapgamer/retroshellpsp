#include <stdio.h>
#include <SDL/SDL.h>
#include <stdint.h>
#include "shared.h"
#include "smsplus.h"
#include "text_gui.h"

SDL_Surface *font = NULL;
SDL_Surface *bigfontred = NULL;
SDL_Surface *bigfontwhite = NULL;

extern SDL_Surface* sdl_screen;

int32_t gfx_font_width(SDL_Surface* inFont, char* inString) {
	if((inFont == NULL) || (inString == NULL))
		return 0;
	int32_t i, tempCur, tempMax;
	for(i = 0, tempCur = 0, tempMax = 0; inString[i] != '\0'; i++) {
		if(inString[i] == '\t')
			tempCur += 4;
		else if((inString[i] == '\r') || (inString[i] == '\n'))
			tempCur = 0;
		else
			tempCur++;
		if(tempCur > tempMax) tempMax = tempCur;
	}
	tempMax *= (inFont->w >> 4);
	return tempMax;
}

int32_t gfx_font_height(SDL_Surface* inFont) {
	if(inFont == NULL)
		return 0;
	return (inFont->h >> 4);
}

void gfx_font_print(SDL_Surface* dest, int32_t inX, int32_t inY, SDL_Surface* inFont, char* inString) 
{
    if((inFont == NULL) || (inString == NULL))
        return;

    uint16_t* tempBuffer = dest->pixels;
    uint16_t* tempFont = inFont->pixels;
    uint8_t*  tempChar;
    int32_t   tempX = inX;
    int32_t   tempY = inY;
    int32_t i, j, x, y;

    SDL_LockSurface(dest);
    SDL_LockSurface(inFont);

    for(tempChar = (uint8_t*)inString; *tempChar != '\0'; tempChar++) 
    {
		switch(*tempChar)
		{
			case ' ':
				tempX += (inFont->w >> 4);
			break;
			case '\t':
				tempX += ((inFont->w >> 4) << 2);
			break;
			case '\r':
				tempX = inX;
			break;
			case '\n':
				tempX = inX;
				tempY += (inFont->h >> 4);
			break;
		}

        for(j = ((*tempChar >> 4) * (inFont->h >> 4)), y = tempY; (j < (((*tempChar >> 4) + 1) * (inFont->h >> 4))) && (y < dest->h); j++, y++) 
        {
            for(i = ((*tempChar & 0x0F) * (inFont->w >> 4)), x = tempX; (i < (((*tempChar & 0x0F) + 1) * (inFont->w >> 4))) && (x < dest->w); i++, x++)
            {
                tempBuffer[(y * dest->w) + x] |= tempFont[(j * inFont->w) + i];
            }
        }
        tempX += (inFont->w >> 4);
    }
    SDL_UnlockSurface(dest);
    SDL_UnlockSurface(inFont);
}

void gfx_font_print_center(SDL_Surface* dest,int32_t inY, SDL_Surface* inFont, char* inString) 
{
    int32_t tempX = (dest->w - gfx_font_width(inFont, inString)) >> 1;
    gfx_font_print(dest,tempX, inY, inFont, inString);
}

void gfx_font_print_fromright(SDL_Surface* dest,int32_t inX, int32_t inY, SDL_Surface* inFont, char* inString) 
{
    int32_t tempX = inX - gfx_font_width(inFont, inString);
    gfx_font_print(dest,tempX, inY, inFont, inString);
}

SDL_Surface* gfx_tex_load_tga_from_array(uint8_t* buffer) 
{
    if(buffer == NULL)
        return NULL;

    int32_t tga_width;
    int32_t tga_height;
    uint8_t  tga_descriptor;
    
    tga_width = buffer[12] + (buffer[13] << 8);
    tga_height = buffer[14] + (buffer[15] << 8);
    tga_descriptor = buffer[17];

    uint32_t bufIndex = 18;

    SDL_Surface* tempTexture = SDL_CreateRGBSurface(SDL_SWSURFACE, tga_width, tga_height, 16, 0, 0, 0, 0);
    if(tempTexture == NULL) 
    {
        return NULL;
    }

    uint32_t upsideDown = (tga_descriptor & 0x20) > 0;

    int32_t i;
    int32_t iNew;
    uint8_t tempColor[3];
    SDL_LockSurface(tempTexture);
    uint16_t* tempTexPtr = tempTexture->pixels;
    for(i = 0; i < (tga_width * tga_height); i++) 
    {
        tempColor[2] = buffer[bufIndex + 0];
        tempColor[1] = buffer[bufIndex + 1];
        tempColor[0] = buffer[bufIndex + 2];
        bufIndex += 3;

        if (upsideDown)
            iNew = i;
        else
            iNew = (tga_height - 1 - (i / tga_width)) * tga_width + i % tga_width;

        tempTexPtr[iNew] = SDL_MapRGB(sdl_screen->format,tempColor[0], tempColor[1], tempColor[2]);
    }
    SDL_UnlockSurface(tempTexture);
    return tempTexture;
}
