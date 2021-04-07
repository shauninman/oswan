#include <stdio.h>
#include <math.h>

#include <SDL/SDL.h>

#include "WSHard.h"
#include "WSApu.h"

SOUND Ch[4];
SWEEP Swp;
NOISE Noise; 
int8_t VoiceOn;

#define BUFSIZEN    0x1000	// ノイズ用テーブル: 0x10000 だったけどそんなにいらないと思う
#define BPSWAV      12000	/* WSのHblankが12KHz */

#define WAV_VOLUME 30

static uint8_t PData[4][32];
static uint8_t PDataN[8][BUFSIZEN];
static uint16_t RandData[BUFSIZEN];
static int16_t sndbuffer[SND_RNGSIZE];	/* Sound Ring Buffer */	//モノラルに変更

static int32_t rBuf, wBuf;
static int32_t freqpush[4];
static int32_t volLpush[4];
static int32_t volRpush[4];

extern uint8_t *Page[16];
extern uint8_t IO[0x100];

SDL_mutex *sound_mutex;
SDL_cond *sound_cv;

int32_t apuBufLen(void)
{
	if (wBuf >= rBuf) return (wBuf - rBuf);
	return SND_RNGSIZE + wBuf - rBuf;
}

void mixaudioCallback(void *userdata, uint8_t *stream, int32_t len)
{
	uint16_t *buffer   = (uint16_t *) stream;
	uint32_t i;
	
	if ((len <= 0) || !buffer || (apuBufLen() < len)) return;
	
	SDL_LockMutex(sound_mutex);
	
	for(i = 0; i < len; i += 4)
	{
		*buffer++ = sndbuffer[rBuf++];
		*buffer++ = sndbuffer[rBuf++];
		if (rBuf >= SND_RNGSIZE) rBuf = 0;
	}

	SDL_UnlockMutex(sound_mutex);
	SDL_CondSignal(sound_cv);
}

void apuWaveCreate(void)
{
    /*memset(sndbuffer,0x00, SND_RNGSIZE);*/
    memset(sndbuffer, 0x00, sizeof(*sndbuffer));
}

void apuWaveRelease(void)
{
	SDL_PauseAudio(1);
}

void apuInit(void)
{
    int32_t i, j;
    
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 32; j++)
        {
            PData[i][j] = 8;
        }
    }
    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < BUFSIZEN; j++)
        {
            PDataN[i][j] = ((apuMrand(15 - i) & 1) ? 15 : 0);
        }
    }

    for (i = 0; i < BUFSIZEN; i++)
    {
        RandData[i] = apuMrand(15);
    }
    apuWaveCreate();
    
	sound_mutex = SDL_CreateMutex();
	sound_cv = SDL_CreateCond();
}

void apuEnd(void)
{
    apuWaveRelease();
	SDL_CondSignal(sound_cv);
}

uint32_t apuMrand(uint32_t Degree)
{
	#define BIT(n) (1<<n)
    typedef struct
    {
        uint32_t N;
        int32_t InputBit;
        int32_t Mask;
    } POLYNOMIAL;

    static POLYNOMIAL TblMask[]=
    {
        { 2,BIT(2) ,BIT(0)|BIT(1)},
        { 3,BIT(3) ,BIT(0)|BIT(1)},
        { 4,BIT(4) ,BIT(0)|BIT(1)},
        { 5,BIT(5) ,BIT(0)|BIT(2)},
        { 6,BIT(6) ,BIT(0)|BIT(1)},
        { 7,BIT(7) ,BIT(0)|BIT(1)},
        { 8,BIT(8) ,BIT(0)|BIT(2)|BIT(3)|BIT(4)},
        { 9,BIT(9) ,BIT(0)|BIT(4)},
        {10,BIT(10),BIT(0)|BIT(3)},
        {11,BIT(11),BIT(0)|BIT(2)},
        {12,BIT(12),BIT(0)|BIT(1)|BIT(4)|BIT(6)},
        {13,BIT(13),BIT(0)|BIT(1)|BIT(3)|BIT(4)},
        {14,BIT(14),BIT(0)|BIT(1)|BIT(4)|BIT(5)},
        {15,BIT(15),BIT(0)|BIT(1)},
        { 0,      0,      0},
    };
    static POLYNOMIAL *pTbl = TblMask;
    static int32_t ShiftReg = BIT(2)-1;
    int32_t XorReg = 0;
    int32_t Masked;

    if(pTbl->N != Degree)
    {
        pTbl = TblMask;
        while(pTbl->N) {
            if(pTbl->N == Degree)
            {
                break;
            }
            pTbl++;
        }
        if(!pTbl->N)
        {
            pTbl--;
        }
        ShiftReg &= pTbl->InputBit-1;
        if(!ShiftReg)
        {
            ShiftReg = pTbl->InputBit-1;
        }
    }
    Masked = ShiftReg & pTbl->Mask;
    while(Masked)
    {
        XorReg ^= Masked & 0x01;
        Masked >>= 1;
    }
    if(XorReg)
    {
        ShiftReg |= pTbl->InputBit;
    }
    else
    {
        ShiftReg &= ~pTbl->InputBit;
    }
    ShiftReg >>= 1;
    return ShiftReg;
}

void apuSetPData(int32_t addr, uint8_t val)
{
    int32_t i, j;

    i = (addr & 0x30) >> 4;
    j = (addr & 0x0F) << 1;
    PData[i][j]=(uint8_t)(val & 0x0F);
    PData[i][j + 1]=(uint8_t)((val & 0xF0)>>4);
}

uint8_t apuVoice(void)
{
    static int32_t index = 0, b = 0;
    uint8_t v;

    if ((IO[SDMACTL] & 0x98) == 0x98) /* Hyper voice */
    { 
        v = Page[IO[SDMASH] + b][*(uint16_t*)(IO + SDMASL) + index++];
        if ((*(uint16_t*)(IO + SDMASL) + index) == 0)
        {
            b++;
        }
        if (v < 0x80)
        {
            v += 0x80;
        }
        else
        {
            v -= 0x80;
        }
        if (*(uint16_t*)(IO+SDMACNT) <= index)
        {
            index = 0;
            b = 0;
        }
        return v;
    }
    else if ((IO[SDMACTL] & 0x88) == 0x80) /* DMA start */
    { 
        IO[SND2VOL] = Page[IO[SDMASH] + b][*(uint16_t*)(IO + SDMASL) + index++];
        if ((*(uint16_t*)(IO + SDMASL) + index) == 0)
        {
            b++;
        }
        if (*(uint16_t*)(IO + SDMACNT) <= index)
        {
            IO[SDMACTL] &= 0x7F; /* DMA end */
            *(uint16_t*)(IO + SDMACNT) = 0;
            index = 0;
            b = 0;
        }
    }
    return ((VoiceOn) ? IO[SND2VOL] : 0x80);
}

void apuSweep(void)
{
    if ((Swp.step) && Swp.on) /* Sweep on */
    {
        if (Swp.cnt < 0)
        {
            Swp.cnt = Swp.time;
            Ch[2].freq += Swp.step;
            Ch[2].freq &= 0x7ff;
        }
        Swp.cnt--;
    }
}

uint16_t apuShiftReg(void)
{
    static int32_t nPos = 0;
    /* Noise counter */
    if (++nPos >= BUFSIZEN)
    {
        nPos = 0;
    }
    return RandData[nPos];
}

void apuWaveSet0(void)		// 12 → 24kHzへの拡張・周波数と音量だけ保存
{
	uint8_t channel;
	for (channel = 0; channel < 4; channel++)
	{
		freqpush[channel] = Ch[channel].freq;
		volLpush[channel] = Ch[channel].volL;
		volRpush[channel] = Ch[channel].volR;
	}
}

void apuWaveSet(void)
{
//	TRIMUI 変更点
//	・マスク機能はないので sound[x] を削除
//	・ステレオからモノラルに変更
//	・合成は12 x 2 = 24kHz (PCM/ノイズは12kHzのまま)
    static uint32_t point[4] = {1, 1, 1, 1};
    static uint32_t preindex[4] = {0, 0, 0, 0};
    int32_t Vol1 = 0, Vol2 = 0;
    int32_t value1, value2, vVol;
    uint32_t index1, index2, channel;

#ifdef AUDIOFRAMESKIP
    while (apuBufLen() >= (SND_RNGSIZE - 16)) SDL_Delay(1);	// buffer overrun wait
#endif

    SDL_LockMutex(sound_mutex);
    
    apuSweep();		// sweepを計算 (12kHz)
    
    for (channel = 0; channel < 4; channel++)
    {
        if (Ch[channel].on)
        {
            if (channel == 1 && VoiceOn)	continue;	// PCM合成中はch1を再生しない
            else if (channel == 3 && Noise.on)	// ch3がノイズの場合
            {
		index1 = ((3072000 / BPSWAV) * point[3] / (2048 - Ch[3].freq)) % BUFSIZEN;
		value1 = PDataN[Noise.pattern][index1] - 8;
		value2 = value1;		// ノイズは12kHzで十分
                if ((index1 == 0) && preindex[3])	point[3] = 0;
		preindex[3] = index1; point[3]++;
            }
            else
            {	// 3072000 / BPSWAV(12000) = 256
		index1 = ( (point[channel]<<9) - 256) / (2048 - freqpush[channel]);	// 中間処理
		index2 = ( (point[channel]<<9)      ) / (2048 - Ch[channel].freq );
		index1 = ((index1 >> 1) + (index1 & 1)) & 0x1f;	// 四捨五入
		index2 = ((index2 >> 1) + (index2 & 1)) & 0x1f;	// (あんまり変わらないかも、要検討)
                if ((index2 == 0) && preindex[channel])	point[channel] = 0;
		preindex[channel] = index2; point[channel]++;
		value1 = PData[channel][index1] - 8;
		value2 = PData[channel][index2] - 8;
            }
		// モノラル化：どちらか音量の大きい方を採用
		Vol1 += value1 * (volLpush[channel] > volRpush[channel] ? volLpush[channel] : volRpush[channel]);
		Vol2 += value2 * (Ch[channel].volL > Ch[channel].volR ? Ch[channel].volL : Ch[channel].volR);
        }
    }
    vVol = (apuVoice() - 0x80);		// PCM合成 (12kHz)

    sndbuffer[wBuf++] = (Vol1 + vVol) * WAV_VOLUME;
    sndbuffer[wBuf++] = (Vol2 + vVol) * WAV_VOLUME;
    if (wBuf >= SND_RNGSIZE) wBuf = 0;

    SDL_UnlockMutex(sound_mutex);
    SDL_CondSignal(sound_cv);
}
