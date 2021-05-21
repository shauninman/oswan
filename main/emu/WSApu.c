#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <SDL/SDL.h>

#include "WSHard.h"
#include "WSApu.h"

SOUND Ch[4];
SWEEP Swp;
NOISE Noise; 
int8_t VoiceOn;
int32_t stereo;

#define BUFSIZEN    0x20000	// ノイズ用テーブル(Arc the Ladの雨のシーン用に増加)
//#define BPSWAV      12000	/* WSのHblankが12KHz */

//#define WAV_VOLUME 32		// 最大 (32768/(120*4+128)) = 53

static uint8_t PData[4][32];
static uint8_t PDataN[8][BUFSIZEN];
static uint16_t RandData[BUFSIZEN];
static int16_t sndbuffer[SND_RNGSIZE*2];	/* Sound Ring Buffer */

static int32_t rBuf, wBuf;
static int32_t freqpush[4];
static int32_t volLpush[4];
static int32_t volRpush[4];

extern uint8_t *Page[16];
extern uint8_t IO[0x100];

SDL_mutex *sound_mutex;

int32_t apuBufLen(void)
{
	if (wBuf >= rBuf) return (wBuf - rBuf);
	return (SND_RNGSIZE<<stereo) + wBuf - rBuf;
}

void mixaudioCallback(void *userdata, uint8_t *stream, int32_t len)
{
	uint32_t *buffer   = (uint32_t *) stream;
	uint32_t i;
	
	if ((len <= 0) || !buffer || (apuBufLen() < len)) return;
	
	SDL_LockMutex(sound_mutex);
	
	for(i = 0; i < len; i += 16)
	{
		*buffer++ = *(uint32_t *)(sndbuffer+rBuf);
		*buffer++ = *(uint32_t *)(sndbuffer+rBuf+2);
		*buffer++ = *(uint32_t *)(sndbuffer+rBuf+4);
		*buffer++ = *(uint32_t *)(sndbuffer+rBuf+6);
		rBuf+=8;
		if (rBuf >= (SND_RNGSIZE<<stereo)) rBuf = 0;
	}

	SDL_UnlockMutex(sound_mutex);
}

void init_SDLaudio(void)
{
#ifdef SOUND_ON
	if (SDL_WasInit(SDL_INIT_AUDIO))
	{
		SDL_CloseAudio();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
	}
#ifdef TRIMUI
	if (access("/dev/dsp1", R_OK | W_OK) == 0) {
		setenv("AUDIODEV", "/dev/dsp1", 1);
		stereo = 1;
	} else {
		setenv("AUDIODEV", "/dev/dsp", 1);
		stereo = 0;
	}
#else
#ifdef STEREO_SOUND
	stereo = 1;
#else
	stereo = 0;
#endif	//STEREO_SOUND
#endif	//TRIMUI
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	SDL_AudioSpec fmt, retFmt;

	/*	Set up SDL sound */
	fmt.freq = 24000;		// 24kHz
	fmt.samples = SND_BNKSIZE;
	fmt.format = AUDIO_S16SYS;
	fmt.channels = stereo+1;
	fmt.callback = mixaudioCallback;
	fmt.userdata = NULL;

    /* Open the audio device and start playing sound! */
    if ( SDL_OpenAudio(&fmt, &retFmt) < 0 )
	{
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        printf("Exiting Oswan...\n");
        exit(1);
    }
#endif	//SOUND_ON
}

void check_USBplug(void)
{
#ifdef	TRIMUI
	int USBaccess = access("/dev/dsp1", R_OK | W_OK);
	if ( ((!USBaccess)&&(!stereo)) || ((USBaccess)&&(stereo)) )
	{	// Sound Output Change
		init_SDLaudio();
		apuWaveCreate();
		SDL_PauseAudio(0);
	}
#endif
}

void apuWaveCreate(void)
{
	memset(sndbuffer, 0x00, sizeof(*sndbuffer));
	rBuf = wBuf = 0;
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
}

void apuEnd(void)
{
    apuWaveRelease();
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
		if (Ch[channel].on) {
			volLpush[channel] = Ch[channel].volL;
			volRpush[channel] = Ch[channel].volR;
		} else	volLpush[channel] = volRpush[channel] = 0;	// chがoffなら音量0
	}
}

void apuWaveSet(void)
{
//	TRIMUI 変更点
//	・マスク機能はないので sound[x] を削除
//	・合成は12 x 2 = 24kHz (PCM/ノイズは12kHzのまま)
//	・point[channel] の処理を見直して音痴を改善
    static uint32_t point[4] = {1, 1, 1, 1};
    int32_t Vol1L = 0, Vol1R = 0, Vol2L = 0, Vol2R = 0;
    int32_t value1, value2, vVol;
    uint32_t index1, index2, channel;
    div_t divresult;

    apuSweep();		// sweepを計算 (12kHz)
    
    for (channel = 0; channel < 4; channel++)
    {
        if ((Ch[channel].on) && ((Ch[channel].volL | Ch[channel].volR) != 0))
        {
            if (channel == 1 && VoiceOn)	continue;	// PCM合成中はch1を再生しない
            else if (channel == 3 && Noise.on)	// ch3がノイズの場合
            {
		divresult = div( point[3]<<8 , 2048 - Ch[3].freq );
		index1 = divresult.quot % BUFSIZEN;
		value1 = PDataN[Noise.pattern][index1] - 8;
		value2 = value1;		// ノイズは12kHzで十分
		if ((index1|divresult.rem) == 0) point[3] = 0;
		point[3]++;
            }
            else
            {	// 3072000 / BPSWAV(12000) = 256
		index1 = (((point[channel]<<8) - 128) / (2048 - freqpush[channel])) & 0x1f;	// 中間処理
		divresult = div( point[channel]<<8 , 2048 - Ch[channel].freq );	// 余りも使うのでdiv命令で
		index2 = divresult.quot & 0x1f;
		value1 = PData[channel][index1] - 8;
		value2 = PData[channel][index2] - 8;
		if ((index2|divresult.rem) == 0) point[channel] = 0;	// 音痴改善
		point[channel]++;
            }
		if (stereo)
		{
		    Vol1L += value1 * volLpush[channel];
		    Vol1R += value1 * volRpush[channel];
		    Vol2L += value2 * Ch[channel].volL;
		    Vol2R += value2 * Ch[channel].volR;
		} else {
		    // モノラル化：どちらか音量の大きい方を採用
		    Vol1L += value1 * (volLpush[channel] > volRpush[channel] ? volLpush[channel] : volRpush[channel]);
		    Vol2L += value2 * (Ch[channel].volL > Ch[channel].volR ? Ch[channel].volL : Ch[channel].volR);
		}
        } else	point[channel] = 1;	// 無音時は波形メモリポイントをリセット
    }
    vVol = (apuVoice() - 0x80);		// PCM合成 (12kHz)

#ifdef AUDIOFRAMESKIP
    while (apuBufLen() >= ((SND_RNGSIZE<<stereo) - 16)) SDL_Delay(1);	// buffer overrun wait
#endif

    SDL_LockMutex(sound_mutex);

    if (stereo)
    {
	*(uint32_t *)(sndbuffer+wBuf)   = (uint16_t)(Vol1L+vVol)<<5 | (uint16_t)(Vol1R+vVol)<<21; // -19456 max
	*(uint32_t *)(sndbuffer+wBuf+2) = (uint16_t)(Vol2L+vVol)<<5 | (uint16_t)(Vol2R+vVol)<<21; // ((-8*15)*4-128)*32
	wBuf+=4;
    } else {
	*(uint32_t *)(sndbuffer+wBuf)   = (uint16_t)(Vol1L+vVol)<<5 | (uint16_t)(Vol2L+vVol)<<21;
	wBuf+=2;
    }

    if (wBuf >= (SND_RNGSIZE<<stereo) ) wBuf = 0;

    SDL_UnlockMutex(sound_mutex);
}
