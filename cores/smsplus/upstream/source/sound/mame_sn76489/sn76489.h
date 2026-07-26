#ifndef SN76489_DEFINE
#define SN76489_DEFINE

#include <stdint.h>

typedef struct sn76489_struct
{
	uint32_t m_feedback_mask;
	uint8_t m_noisetap1;
	uint8_t m_noisetap2;
	uint8_t m_negate;
	uint8_t m_stereo;
	uint8_t m_clockdivider;
	uint8_t m_sega_style_psg;

	uint16_t m_clock_divider;
	uint16_t m_whitenoise_tap1;
	uint16_t m_whitenoise_tap2;

	uint8_t m_last_register;

	uint8_t m_ready_state;

	int m_cycles_to_ready;
	int m_stereo_mask;
	int m_current_clock ;
	
	int32_t m_output[4];
	int32_t m_period[4];
	int32_t m_count[4];
	int32_t m_register[8];
	int32_t m_volume[4];
	int32_t m_vol_table[16];

	int32_t m_RNG;

	int32_t m_clocks_per_sample;
} sn76489_t;

extern sn76489_t PSG;

extern void SN76489_Init(uint32_t machine, uint32_t clock, uint32_t sample_rate);
extern void SN76489_GGStereoWrite(uint8_t st);
extern void SN76489_write(uint8_t data);


extern void SN76489_Update(int16_t** outputs, int samples);

#endif
