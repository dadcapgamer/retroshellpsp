/*
    fmintf.c --
    Interface to YM2413 emulators emulators.
*/
#include "shared.h"

static FM_Context fm_context;
static YM2413 *fmm;

/* Only the 1st Master System supports FM sound. Because it is still processed even though it's not used,
* don't process FM sound if it can't be used in any way.
*/
static uint32_t isfm_used = 0;

void FM_Init(void)
{
	isfm_used = 0;
	fmm = ym2413_init(snd.fm_clock, snd.sample_rate);
	ym2413_reset(fmm);
}


void FM_Shutdown(void)
{
	isfm_used = 0;
	ym2413_shutdown(fmm);
}


void FM_Reset(void)
{
	isfm_used = 0;
	ym2413_reset(fmm);
}


void FM_Update(int16_t **buffer, int32_t length)
{
	if (isfm_used) ym2413_update(fmm, buffer, length);
}

void FM_WriteReg(uint8_t reg, uint8_t data)
{
    FM_Write(0, reg);
    FM_Write(1, data);
}

void FM_Write(uint32_t offset, uint8_t data)
{
    if(offset & 1)
    {
        fm_context.reg[ fm_context.latch ] = data;
	}
    else
    {
        fm_context.latch = data;
	}

	ym2413_write(fmm, offset & 1, data);
	isfm_used = 1;
}

void FM_GetContext(uint8_t *data)
{
    memcpy(data, &fm_context, sizeof(FM_Context));
}

void FM_SetContext(uint8_t *data)
{
    uint8_t i;
    uint8_t *reg = fm_context.reg;

    memcpy(&fm_context, data, sizeof(FM_Context));

    /* If we are loading a save state, we want to update the ym2413_ context
       but not actually write to the current ym2413_ emulator. */
    if(!snd.enabled || !sms.use_fm)
        return;

    FM_Write(0, 0x0E);
    FM_Write(1, reg[0x0E]);

    for(i = 0x00; i <= 0x07; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x10; i <= 0x18; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x20; i <= 0x28; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x30; i <= 0x38; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    FM_Write(0, fm_context.latch);
}

uint32_t FM_GetContextSize(void)
{
    return sizeof(FM_Context);
}

uint8_t *FM_GetContextPtr(void)
{
    return (uint8_t *)&fm_context;
}

uint32_t YM2413_GetContextSize(void)
{
    return sizeof(YM2413);
}

uint8_t *YM2413_GetContextPtr(void)
{
    return (uint8_t *)fmm;
}
