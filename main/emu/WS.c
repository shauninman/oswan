#include <stdio.h>
#include <string.h>
#include <time.h>

/* SDL drawing screen */
extern void graphics_paint(void);

#include "WSRender.h"
#include "WS.h"
#include "game_input.h"
#ifdef SOUND_EMULATION
#include "WSApu.h"
#endif
#include "WSFileio.h"
#include "cpu/necintrf.h"

#define IPeriod 32          	/* HBlank/8 (256/8)					*/

/*int32_t Run;*/
uint8_t *Page[16];             	/* バンク割り当て 						*/
uint8_t IRAM[0x10000];         	/* 内部RAM 64kB = Page[0]			*/
uint8_t IO[0x100];             	/* IO								*/
uint8_t MemDummy[0x10000];     	/* ダミーバンク 64kB					*/
uint8_t *ROMMap[0x100];        	/* C-ROMバンクマップ					*/
uint16_t ROMBanks;    	/* C-ROMバンク数						*/
uint8_t *RAMMap[0x100];        	/* C-RAMバンクマップ					*/
uint8_t RAMBanks;     	/* C-RAMバンク数						*/
int32_t RAMSize;               	 	/* C-RAM総容量						*/
uint16_t IEep[64];              	/* 内蔵EEPROM						*/
struct EEPROM sIEep;        	/* EEPROM読み書き用構造体（内蔵）		*/
struct EEPROM sCEep;        	/* EEPROM読み書き用構造体（カートリッジ）	*/
uint8_t CartKind;     	/* セーブメモリの種類（CK_EEP = EEPROM）	*/

static int32_t ButtonState = 0x0000;    /* Button state: B.A.START.OPTION.X4.X3.X2.X1.Y4.Y3.Y2.Y1	*/
static uint8_t HVMode;
static uint16_t HTimer;
static uint16_t VTimer;
static uint8_t RtcCount;

#ifdef FRAMESKIP
int32_t FrameSkip = 4;
static int32_t SkipCnt = 0;
static const int32_t TblSkip[5][5] = {
    {1,1,1,1,1},
    {0,1,1,1,1},
    {0,1,0,1,1},
    {0,0,1,0,1},
    {0,0,0,0,1},
};
#endif

#ifdef AUDIOFRAMESKIP
static int32_t SkipCnt = 0;
#define UNDERRUN_THRESHOLD	0.5
#define MAX_SKIP_COUNT		5
#endif

#ifdef SOUND_EMULATION
static uint32_t WaveMap;
#endif

#define MONO(C) (C)<<12 | (C)<<7 | (C)<<1

static uint16_t DefColor[] = {
    MONO(0xF), MONO(0xE), MONO(0xD), MONO(0xC), MONO(0xB), MONO(0xA), MONO(0x9), MONO(0x8),
    MONO(0x7), MONO(0x6), MONO(0x5), MONO(0x4), MONO(0x3), MONO(0x2), MONO(0x1), MONO(0x0)
};

void ComEeprom(struct EEPROM *eeprom, const uint16_t *cmd, uint16_t *data)
{
    int32_t i, j, op, addr;
    const int32_t tblmask[16][5]=
    {
        {0x0000, 0, 0x0000, 0, 0x0000}, /* dummy */
        {0x0000, 0, 0x0000, 0, 0x0000},
        {0x0000, 0, 0x0000, 0, 0x0000},
        {0x0000, 0, 0x0000, 0, 0x0000},
        {0x000C, 2, 0x0003, 0, 0x0003},
        {0x0018, 3, 0x0006, 1, 0x0007},
        {0x0030, 4, 0x000C, 2, 0x000F},
        {0x0060, 5, 0x0018, 3, 0x001F},
        {0x00C0, 6, 0x0030, 4, 0x003F}, /* 1Kbits IEEPROM */
        {0x0180, 7, 0x0060, 5, 0x007F},
        {0x0300, 8, 0x00C0, 6, 0x00FF},
        {0x0600, 9, 0x0180, 7, 0x01FF},
        {0x0C00, 10, 0x0300, 8, 0x03FF}, /* 16Kbits */
        {0x1800, 11, 0x0600, 9, 0x07FF},
        {0x3000, 12, 0x0C00, 10, 0x0FFF},
        {0x6000, 13, 0x1800, 11, 0x1FFF},
    };
    if(eeprom->data == NULL)
    {
        return;
    }
    for(i = 15, j = 0x8000; i >= 0; i--, j >>= 1)
    {
        if(*cmd & j)
        {
            break;
        }
    }
    op = (*cmd & tblmask[i][0]) >> tblmask[i][1];
    switch(op)
    {
    case 0:
        addr = (*cmd & tblmask[i][2]) >> tblmask[i][3];
        switch(addr)
        {
        case 0: /* 書込み禁止 */
            eeprom->we = 0;
            break;
        case 1: /* 全アドレス書き込み */
            for(j = tblmask[i][4]; j >= 0; j--)
            {
                eeprom->data[j] = *data;
            }
            break;
        case 2: /* チップ消去 */
            if(eeprom->we)
            {
                memset(eeprom->data, 0xFF, sizeof(eeprom->data)*2);
            }
            break;
        case 3: /* 書き込み可能 */
            eeprom->we = 1;
            break;
        }
        *data = 0;
        break;
    case 1: /* 書き込み */
        if(eeprom->we)
        {
            addr = *cmd & tblmask[i][4];
            eeprom->data[addr] = *data;
            /*
            if (ROMBanks == 1 && addr == 0x3A) 
            {
                Run = 0; 
            }
            */
        }
        *data = 0;
        break;
    case 2: /* 読み出し*/
        addr = *cmd & tblmask[i][4];
        *data = eeprom->data[addr];
        break;
    case 3: /* 消去*/
        if(eeprom->we)
        {
            addr = *cmd & tblmask[i][4];
            eeprom->data[addr] = 0xFFFF;
        }
        *data = 0;
        break;
    default: break;
    }
}

uint8_t ReadMem(const uint32_t A)
{
    return Page[(A >> 16) & 0xF][A & 0xFFFF];
}

void WriteMem(const uint32_t A, const uint8_t V)
{
    (*WriteMemFnTable[(A >> 16) & 0x0F])(A, V);
}

static void WriteRom(const uint32_t A, const uint8_t V)
{
    /*ErrorMsg(ERR_WRITE_ROM);*/
}

static void WriteIRam(const uint32_t A, const uint8_t V)
{
    IRAM[A & 0xFFFF] = V;
    if((A & 0xFE00) == 0xFE00)
    {
        SetPalette(A);
    }
#ifdef SOUND_EMULATION
    if(!((A - WaveMap) & 0xFFC0))
    {
        apuSetPData(A & 0x003F, V);
    }
#endif
}

#define FLASH_CMD_ADDR1         0x0AAA
#define FLASH_CMD_ADDR2         0x0555
#define FLASH_CMD_DATA1         0xAA
#define FLASH_CMD_DATA2         0x55
#define FLASH_CMD_RESET         0xF0
#define FLASH_CMD_ERASE         0x80
#define FLASH_CMD_ERASE_CHIP    0x10
#define FLASH_CMD_ERASE_SECT    0x30
#define FLASH_CMD_CONTINUE_SET  0x20
#define FLASH_CMD_CONTINUE_RES1 0x90
#define FLASH_CMD_CONTINUE_RES2 0xF0
#define FLASH_CMD_CONTINUE_RES3 0x00
#define FLASH_CMD_WRITE         0xA0
static void WriteCRam(const uint32_t A, const uint8_t V)
{
    static int32_t flashCommand1 = 0;
    static int32_t flashCommand2 = 0;
    static int32_t flashWriteSet = 0;
    static int32_t flashWriteOne = 0;
    static int32_t flashWriteReset = 0;
    static int32_t flashWriteEnable = 0;
    int32_t offset = A & 0xFFFF;

	/* 
    if (offset >= RAMSize)
    {
        ErrorMsg(ERR_OVER_RAMSIZE);
    }
    */
    
    /* 
    WonderWitch
    FLASH ROM command sequence
    */
    if (flashCommand2)
    {
        if (offset == FLASH_CMD_ADDR1)
        {
            switch (V) {
            case FLASH_CMD_CONTINUE_SET:
                flashWriteSet   = 1;
                flashWriteReset = 0;
                break;
            case FLASH_CMD_WRITE:
                flashWriteOne = 1;
                break;
            case FLASH_CMD_RESET:
                break;
            case FLASH_CMD_ERASE:
                break;
            case FLASH_CMD_ERASE_CHIP:
                break;
            case FLASH_CMD_ERASE_SECT:
                break;
            }
        }
        flashCommand2 = 0;
    }
    else if (flashCommand1)
    {
        if (offset == FLASH_CMD_ADDR2 && V == FLASH_CMD_DATA2)
        {
            flashCommand2 = 1;
        }
        flashCommand1 = 0;
    }
    else if (offset == FLASH_CMD_ADDR1 && V == FLASH_CMD_DATA1)
    {
        flashCommand1 = 1;
    }
    if (RAMSize != 0x40000 || IO[BNK1SLCT] < 8)
    {
        /* normal sram */
        Page[1][offset] = V;
    }
    else if (IO[BNK1SLCT] >= 8 && IO[BNK1SLCT] < 15)
    {
        /* FLASH ROM use SRAM bank(port 0xC1:8-14)(0xC1:15 0xF0000-0xFFFFF are write protected) */
        if (flashWriteEnable || flashWriteOne)
        {
            Page[IO[BNK1SLCT]][offset] = V;
            flashWriteEnable = 0;
            flashWriteOne = 0;
        }
        else if (flashWriteSet)
        {
            switch (V)
            {
            case FLASH_CMD_WRITE:
                flashWriteEnable = 1;
                flashWriteReset = 0;
                break;
            case FLASH_CMD_CONTINUE_RES1:
                flashWriteReset = 1;
                break;
            case FLASH_CMD_CONTINUE_RES2:
            case FLASH_CMD_CONTINUE_RES3:
                if (flashWriteReset)
                {
                    flashWriteSet = 0;
                    flashWriteReset = 0;
                }
                break;
            default:
                flashWriteReset = 0;
            }
        }
    }
}

#define	WIO(func_name) static uint32_t func_name(const uint32_t A,uint8_t V)
WIO(wNONE) { return 0; }
WIO(w0x07) {
        Scr1TMap = IRAM + ((V & 0x0F) << 11);
        Scr2TMap = IRAM + ((V & 0xF0) << 7);
        return 0; }
WIO(w1C1F) {
        if(IO[COLCTL] & 0x80) return 0;
        int32_t i = (A - 0x1C) << 1;
        MonoColor[i] = DefColor[V & 0x0F];
        MonoColor[i + 1] = DefColor[(V & 0xF0) >> 4];
        for(int32_t k = 0x20; k < 0x40; k++) {
            i = (k & 0x1E) >> 1; int32_t j = 0;
            if(k & 0x01) j = 2;
            Palette[i][j] = MonoColor[IO[k] & 0x07];
            Palette[i][j + 1] = MonoColor[(IO[k] >> 4) & 0x07];
        } return 0; }
WIO(w203F) {
        if (IO[COLCTL] & 0x80) return 0;
        int32_t i = (A & 0x1E) >> 1; int32_t j = 0;
	if (A & 0x01) j = 2;
	Palette[i][j] = MonoColor[V & 0x07];
	Palette[i][j + 1] = MonoColor[(V >> 4) & 0x07];
	return 0; }
WIO(w0x48) {
        if(V & 0x80) {
            int32_t i = *(uint32_t*)(IO + DMASRC); /* IO[]が4バイト境界にあることが必要 */
            int32_t j = *(uint16_t*)(IO + DMADST);
            int32_t k = *(uint16_t*)(IO + DMACNT);
            while(k--) WriteMem(j++, ReadMem(i++));
            *(uint16_t*)(IO + DMACNT) = 0;
            *(uint32_t*)(IO + DMASRC) = i; /* IO[]が4バイト境界にあることが必要 */
            *(uint16_t*)(IO + DMADST) = j;
            IO[A] = V & 0x7F; return 1;
        } return 0; }
#ifdef SOUND_EMULATION
WIO(w8081) {
        IO[A] = V; Ch[0].freq = *(uint16_t*)(IO + SND1FRQ) & 0x7ff; /* for 森田将棋 */
        return 1; }
WIO(w8283) {
        IO[A] = V; Ch[1].freq = *(uint16_t*)(IO + SND2FRQ) & 0x7ff; //
        return 1; }
WIO(w8485) {
        IO[A] = V; Ch[2].freq = *(uint16_t*)(IO + SND3FRQ) & 0x7ff; //
        return 1; }
WIO(w8687) {
        IO[A] = V; Ch[3].freq = *(uint16_t*)(IO + SND4FRQ) & 0x7ff; //
        return 1; }
WIO(w0x88) { Ch[0].volL = (V >> 4) & 0x0F; Ch[0].volR = V & 0x0F; return 0; }
WIO(w0x89) { Ch[1].volL = (V >> 4) & 0x0F; Ch[1].volR = V & 0x0F; return 0; }
WIO(w0x8A) { Ch[2].volL = (V >> 4) & 0x0F; Ch[2].volR = V & 0x0F; return 0; }
WIO(w0x8B) { Ch[3].volL = (V >> 4) & 0x0F; Ch[3].volR = V & 0x0F; return 0; }
WIO(w0x8C) { Swp.step = (signed char)V; return 0; }
WIO(w0x8D) { Swp.time = (V + 1) << 5; return 0; }
WIO(w0x8E) { Noise.pattern = V & 0x07; return 0; }
WIO(w0x8F) {
        WaveMap = V << 6;
        for (int32_t i = 0; i < 64; i++) apuSetPData(WaveMap + i, IRAM[WaveMap + i]);
        return 0; }
WIO(w0x90) {
        Ch[0].on = V & 0x01;
        Ch[1].on = V & 0x02;
        Ch[2].on = V & 0x04;
        Ch[3].on = V & 0x08;
        VoiceOn  = V & 0x20;
        Swp.on   = V & 0x40;
        Noise.on = V & 0x80;
        return 0; }
#endif
WIO(w0x91) { IO[A] = V | 0x80; return 1; }	/* ヘッドホンは常にオン */
WIO(w0xA0) { IO[A] = 0x02; return 1; }
WIO(w0xA2) {
        if(V & 0x01) HTimer = *(uint16_t*)(IO + HPRE); else HTimer = 0;
        if(V & 0x04) VTimer = *(uint16_t*)(IO + VPRE); else VTimer = 0;
        return 0; }
WIO(wA4A5) { IO[A] = V; HTimer = *(uint16_t*)(IO + HPRE); return 1; }	/* FF */
WIO(wA6A7) {
        IO[A] = V;
        IO[A + 4] = V; /* Dark eyes */
        if(IO[TIMCTL] & 0x04) VTimer = *(uint16_t*)(IO + VPRE);
        return 1; }
WIO(w0xB3) {
        if(V & 0x20) V &= 0xDF;
        IO[A] = V | 0x04; return 1; }
WIO(w0xB5) {
        IO[KEYCTL] = (uint8_t)(V & 0xF0);
        if(IO[KEYCTL] & 0x40) IO[KEYCTL] |= (uint8_t)((ButtonState >> 8) & 0x0F);
        if(IO[KEYCTL] & 0x20) IO[KEYCTL] |= (uint8_t)((ButtonState >> 4) & 0x0F);
        if(IO[KEYCTL] & 0x10) IO[KEYCTL] |= (uint8_t)(ButtonState & 0x0F);
        return 1; }
WIO(w0xB6) { IO[IRQACK] &= (uint8_t)~V; return 1; }
WIO(w0xBE) {
        ComEeprom(&sIEep, (uint16_t*)(IO + EEPCMD), (uint16_t*)(IO + EEPDATA));
        IO[A] = V >> 4; return 1; }
WIO(w0xC0) {
        if(nec_get_reg(NEC_CS) >= 0x4000) nec_execute(1);
        int32_t j = (V << 4) & 0xF0;
        Page[0x4] = ROMMap[0x4 | j];
        Page[0x5] = ROMMap[0x5 | j];
        Page[0x6] = ROMMap[0x6 | j];
        Page[0x7] = ROMMap[0x7 | j];
        Page[0x8] = ROMMap[0x8 | j];
        Page[0x9] = ROMMap[0x9 | j];
        Page[0xA] = ROMMap[0xA | j];
        Page[0xB] = ROMMap[0xB | j];
        Page[0xC] = ROMMap[0xC | j];
        Page[0xD] = ROMMap[0xD | j];
        Page[0xE] = ROMMap[0xE | j];
        Page[0xF] = ROMMap[0xF | j];
        return 0; }
WIO(w0xC1) {
        if (V >= 8) /* WonderWitch */ Page[1] = MemDummy; else Page[1] = RAMMap[V];
        return 0; }
WIO(w0xC2) { Page[2] = ROMMap[V]; return 0; }
WIO(w0xC3) { Page[3] = ROMMap[V]; return 0; }
WIO(w0xC8) {
        ComEeprom(&sCEep, (uint16_t*)(IO + CEEPCMD), (uint16_t*)(IO + CEEPDATA));
        if(V & 0x10) V >>= 4;
        if(V & 0x20) V >>= 4;
        if(V & 0x40) V >>= 5;
        IO[A] = V; return 1; }
WIO(w0xCA) { if (V == 0x15) RtcCount = 0; return 0; }	/* RTC Command */
WIO(w0xCB) { return 0; }	/* RTC DATA */

uint32_t (*WriteIOtable[0xCC])(const uint32_t A,uint8_t V) = {
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,w0x07,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,w1C1F,w1C1F,w1C1F,w1C1F,
	w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,
	w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,w203F,
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,w0x48,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
#ifdef SOUND_EMULATION
	w8081,w8081,w8283,w8283,w8485,w8485,w8687,w8687,w0x88,w0x89,w0x8A,w0x8B,w0x8C,w0x8D,w0x8E,w0x8F,
	w0x90,w0x91,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
#else
	wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
	wNONE,w0x91,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
#endif
	w0xA0,wNONE,w0xA2,wNONE,wA4A5,wA4A5,wA6A7,wA6A7,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,
	wNONE,wNONE,wNONE,w0xB3,wNONE,w0xB5,w0xB6,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,wNONE,w0xBE,wNONE,
	w0xC0,w0xC1,w0xC2,w0xC3,wNONE,wNONE,wNONE,wNONE,w0xC8,wNONE,w0xCA,w0xCB
	};

void WriteIO(const uint32_t A, uint8_t V)
{
    if (A < 0xCC) if (WriteIOtable[A](A,V)) return;
    if (A < 0x100) IO[A] = V;
}

#define  BCD(value) ((value / 10) << 4) | (value % 10)
uint8_t ReadIO(const uint32_t A)
{
    if (A == 0xCA) return IO[RTCCMD] | 0x80;
    if (A != 0xCB) return IO[A];

//  for 0xCB (RTC DATA)
    if (IO[RTCCMD] == 0x15)  /* Get time command */
    { 
        uint8_t year, mon, mday, wday, hour, min, sec, j;
        struct tm *newtime;
        time_t long_time;

        time(&long_time);
        newtime = localtime(&long_time);
        switch(RtcCount)
        {
        case 0:
            RtcCount++;
            year = newtime->tm_year;
            year %= 100;
            return BCD(year);
        case 1:
            RtcCount++;
            mon = newtime->tm_mon;
            mon++;
            return BCD(mon);
        case 2:
            RtcCount++;
            mday = newtime->tm_mday;
            return BCD(mday);
        case 3:
            RtcCount++;
            wday = newtime->tm_wday;
            return BCD(wday);
        case 4:
            RtcCount++;
            hour = newtime->tm_hour;
            j = BCD(hour);
            if (hour > 11)
                j |= 0x80;
            return j;
        case 5:
            RtcCount++;
            min = newtime->tm_min;
            return BCD(min);
        case 6:
            RtcCount = 0;
            sec = newtime->tm_sec;
            return BCD(sec);
        }
        return 0;
    }
    else {
        /* set ack */
        return (IO[RTCDATA] | 0x80);
    }
}

WriteMemFn WriteMemFnTable[16]= {
    WriteIRam,
    WriteCRam,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
    WriteRom,
};

void WsReset (void)
{
    int32_t i, j;

    Page[0x0] = IRAM;
    sIEep.data = IEep;
    sIEep.we = 0;
    if(CartKind & CK_EEP)
    {
        Page[0x1] = MemDummy;
        sCEep.data = (uint16_t*)(RAMMap[0x00]);
        sCEep.we = 0;
    }
    else
    {
        Page[0x1] = RAMMap[0x00];
        sCEep.data = NULL;
        sCEep.we = 0;
    }
    Page[0xF] = ROMMap[0xFF];
    i = (IO[SPRTAB] & 0x1F) << 9;
    i += IO[SPRBGN] << 2;
    j = IO[SPRCNT] << 2;
    memcpy(SprTMap, IRAM + i, j);
    SprTTMap = SprTMap;
    SprETMap = SprTMap + j - 4;
    WriteIO(0x07, 0x00);
    WriteIO(0x14, 0x01);
    WriteIO(0x1C, 0x99);
    WriteIO(0x1D, 0xFD);
    WriteIO(0x1E, 0xB7);
    WriteIO(0x1F, 0xDF);
    WriteIO(0x20, 0x30);
    WriteIO(0x21, 0x57);
    WriteIO(0x22, 0x75);
    WriteIO(0x23, 0x76);
    WriteIO(0x24, 0x15);
    WriteIO(0x25, 0x73);
    WriteIO(0x26, 0x77);
    WriteIO(0x27, 0x77);
    WriteIO(0x28, 0x20);
    WriteIO(0x29, 0x75);
    WriteIO(0x2A, 0x50);
    WriteIO(0x2B, 0x36);
    WriteIO(0x2C, 0x70);
    WriteIO(0x2D, 0x67);
    WriteIO(0x2E, 0x50);
    WriteIO(0x2F, 0x77);
    WriteIO(0x30, 0x57);
    WriteIO(0x31, 0x54);
    WriteIO(0x32, 0x75);
    WriteIO(0x33, 0x77);
    WriteIO(0x34, 0x75);
    WriteIO(0x35, 0x17);
    WriteIO(0x36, 0x37);
    WriteIO(0x37, 0x73);
    WriteIO(0x38, 0x50);
    WriteIO(0x39, 0x57);
    WriteIO(0x3A, 0x60);
    WriteIO(0x3B, 0x77);
    WriteIO(0x3C, 0x70);
    WriteIO(0x3D, 0x77);
    WriteIO(0x3E, 0x10);
    WriteIO(0x3F, 0x73);
    WriteIO(0x01, 0x00);
    WriteIO(0x8F, 0x03);
    WriteIO(0x91, 0x80);
    WriteIO(0xA0, 0x02);
    WriteIO(0xB3, 0x04);
    WriteIO(0xBA, 0x01);
    WriteIO(0xBB, 0x00);
    WriteIO(0xBC, 0x30); /* 内蔵EEPROM */
    WriteIO(0xBD, 0x01); /* 書き込み可能 */
    WriteIO(0xBE, 0x83);
    IO[BNKSLCT] = 0x0F;
    j = 0xF0;
    Page[0x4] = ROMMap[0x4 | j];
    Page[0x5] = ROMMap[0x5 | j];
    Page[0x6] = ROMMap[0x6 | j];
    Page[0x7] = ROMMap[0x7 | j];
    Page[0x8] = ROMMap[0x8 | j];
    Page[0x9] = ROMMap[0x9 | j];
    Page[0xA] = ROMMap[0xA | j];
    Page[0xB] = ROMMap[0xB | j];
    Page[0xC] = ROMMap[0xC | j];
    Page[0xD] = ROMMap[0xD | j];
    Page[0xE] = ROMMap[0xE | j];
    Page[0xF] = ROMMap[0xF | j];
    WriteIO(0xC2, 0xFF);
    WriteIO(0xC3, 0xFF);
    IRAM[0x75AC]=0x41;
    IRAM[0x75AD]=0x5F;
    IRAM[0x75AE]=0x43;
    IRAM[0x75AF]=0x31;
    IRAM[0x75B0]=0x6E;
    IRAM[0x75B1]=0x5F;
    IRAM[0x75B2]=0x63;
    IRAM[0x75B3]=0x31;
    ButtonState = 0x0000;
    nec_reset(NULL);
    nec_set_reg(NEC_SP, 0x2000);
}

void WsRomPatch(const uint8_t *buf)
{
    if((buf[0] == 0x01) && (buf[1] == 0x01) && (buf[2] == 0x16)) 					/* SWJ-BANC16 STAR HEARTS 			*/
    {
        RAMBanks = 1;
        RAMSize = 0x8000;
        CartKind = 0;
    }
    if((buf[0] == 0x01) && (buf[1] == 0x00) && (buf[2] == 0x2C || buf[2] == 0x2F)) 	/* SWJ-BAN02C,02F デジタルパートナー 	*/
    {
        RAMBanks = 1;
        RAMSize = 0x8000;
        CartKind = 0;
    }
    if((buf[0] == 0x01) && (buf[1] == 0x01) && (buf[2] == 0x38)) 					/* SWJ-BANC38 NARUTO 木ノ葉忍法帖		*/
    {
        RAMBanks = 1;
        RAMSize = 0x10000;
        CartKind = 0;
    }
}

static void Interrupt0(void) {
	static int32_t Joyz=0x0000;
	if (IO[RSTRL] == 144)
	{
	    uint32_t VCounter;
	
	    ButtonState = WsInputGetState(HVMode);
	    if((ButtonState ^ Joyz) & Joyz)
	    {
	        if(IO[IRQENA] & KEY_IFLAG)
	        {
	            IO[IRQACK] |= KEY_IFLAG;
	        }
	    }
	    Joyz = ButtonState;
	    /* Vblankカウントアップ */
	    VCounter = *(uint16_t*)(IO + VCNTH) << 16 | *(uint16_t*)(IO + VCNTL);
	    VCounter++;
	    *(uint16_t*)(IO + VCNTL) = (uint16_t)VCounter;
	    *(uint16_t*)(IO + VCNTH) = (uint16_t)(VCounter >> 16);
	}
}

static void Interrupt1(void) {
#ifdef SOUND_ON
	apuWaveSet0();	// 24kHz拡張用: この時点での周波数・音量を保存
#endif
}
static void Interrupt2(void) { }

static void Interrupt3(void) { }

static void Interrupt4(void) {
	int32_t i,j;
	if(IO[RSTRL] == 142)
	{
		i = (IO[SPRTAB] & 0x1F) << 9;
		i += IO[SPRBGN] << 2;
		j = IO[SPRCNT] << 2;
		memcpy(SprTMap, IRAM + i, j);
		SprTTMap = SprTMap;
		SprETMap= SprTMap + j - 4;
	}
	
	if(IO[LCDSLP] & 0x01)
	{
		#ifdef FRAMESKIP
		if(IO[RSTRL] == 0)
		{
			SkipCnt--;
			if(SkipCnt < 0)
			{
				SkipCnt = 4;
			}
		}
		if(TblSkip[FrameSkip][SkipCnt])
		#else
		#ifdef AUDIOFRAMESKIP
		if (SkipCnt == 0)
		#endif
		#endif
		{
			if(IO[RSTRL] < 144)
			{
				RefreshLine(IO[RSTRL]);
			}
			else if(IO[RSTRL] == 144)
			{
				graphics_paint();
			}
		}
		#ifdef AUDIOFRAMESKIP
			if(IO[RSTRL] == 144) {
				if (apuBufLen() < (SND_RNGSIZE * UNDERRUN_THRESHOLD))
				{
					SkipCnt++;
					if (SkipCnt > MAX_SKIP_COUNT) SkipCnt = 0;
				} else	SkipCnt = 0;
			}
		#endif
	}
}

static void Interrupt5(void) {
#ifdef SOUND_ON
	/* Hblank毎に1サンプルセットすることで12KHzのwaveデータが出来る */
	apuWaveSet();
	*(uint16_t*)(IO + NCSR) = apuShiftReg();
#endif
}

static void Interrupt6(void) {
	if((IO[TIMCTL] & 0x01) && HTimer)
	{
	    HTimer--;
	    if(!HTimer)
	    {
	        if(IO[TIMCTL] & 0x02)
	        {
	            HTimer = *(uint16_t*)(IO + HPRE);
	        }
	        if(IO[IRQENA] & HTM_IFLAG)
	        {
	            IO[IRQACK] |= HTM_IFLAG;
	        }
	    }
	}
	else if(*(uint16_t*)(IO + HPRE) == 1)
	{
	    if(IO[IRQENA] & HTM_IFLAG)
	    {
	        IO[IRQACK] |= HTM_IFLAG;
	    }
	}
	if((IO[IRQENA] & VBB_IFLAG) && (IO[RSTRL] == 144))
	{
	    IO[IRQACK] |= VBB_IFLAG;
	}
	if((IO[TIMCTL] & 0x04) && (IO[RSTRL] == 144) && VTimer)
	{
	    VTimer--;
	    if(!VTimer)
	    {
	        if(IO[TIMCTL] & 0x08)
	        {
	            VTimer = *(uint16_t*)(IO + VPRE);
	        }
	        if(IO[IRQENA] & VTM_IFLAG)
	        {
	            IO[IRQACK] |= VTM_IFLAG;
	        }
	    }
	}
	if((IO[IRQENA] & RST_IFLAG) && (IO[RSTRL] == IO[RSTRLC]))
	{
	    IO[IRQACK] |= RST_IFLAG;
	}
}

static void Interrupt7(void) {
	IO[RSTRL]++;
	if(IO[RSTRL] >= 159)
	{
	    IO[RSTRL] = 0;
	}
	/* Hblankカウントアップ */
	(*(uint16_t*)(IO + HCNT))++;
}

void (*InterruptTable[8])(void) = {
	Interrupt0,Interrupt1,Interrupt2,Interrupt3,
	Interrupt4,Interrupt5,Interrupt6,Interrupt7
};

int32_t Interrupt(void)
{
    static int32_t LCount=0;

    if(++LCount>=8) LCount=0; 	/* 8回で1Hblank期間 */
    InterruptTable[LCount]();

    return IO[IRQACK];
}

int32_t WsRun(void)
{
    static int32_t period = IPeriod;
    int32_t i, iack, inum;
    uint16_t cycle;
    
//    for(i = 0; i < 1706; i++)
    for(i = 0; i < 159*8; i++) // 1/75s
    {
        cycle = nec_execute(period);
        period += IPeriod - cycle;
        if(Interrupt())
        {
            iack = IO[IRQACK];
            for(inum = 7; inum >= 0; inum--)
            {
                if(iack & 0x80)
                {
                    break;
                }
                iack <<= 1;
            }
            nec_int((inum + IO[IRQBSE]) << 2);
        }
    }
    return 0;
}

void SetHVMode(const uint8_t Mode)
{
    HVMode = Mode;
}

void WsInit(void) {
#ifdef SOUND_ON
	apuInit();
#endif
	WsLoadEeprom();
}

void WsDeInit(void) {
	WsSaveEeprom();
	WsRelease();
#ifdef SOUND_ON
	apuEnd();
#endif
}

