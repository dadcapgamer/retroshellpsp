
#ifndef FMINTF_H_
#define FMINTF_H_

enum 
{
    SND_NULL,
    SND_EMU2413,        /* Mitsutaka Okazaki's YM2413 emulator, removed until i can switch to upstream due to unclear license */
    SND_YM2413          /* Jarek Burczynski's YM2413 emulator */
};

typedef struct 
{
    uint8_t latch;
    uint8_t reg[0x40];
} FM_Context;

/* Function prototypes */
void FM_Init(void);
void FM_Shutdown(void);
void FM_Reset(void);
void FM_Update(int16_t **buffer, int32_t length);
void FM_Write(uint32_t offset, uint8_t data);
void FM_GetContext(uint8_t *data);
void FM_SetContext(uint8_t *data);
uint32_t FM_GetContextSize(void);
uint8_t *FM_GetContextPtr(void);
uint32_t YM2413_GetContextSize(void);
uint8_t *YM2413_GetContextPtr(void);
void FM_WriteReg(uint8_t reg, uint8_t data);

#endif /* FMINTF_H_ */
