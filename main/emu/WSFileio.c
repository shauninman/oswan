﻿#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "WSHard.h"
#include "WS.h"
#include "WSFileio.h"
#include "WSRender.h"
#include "cpu/necintrf.h"

#include "shared.h"

#ifdef ZIP_SUPPORT  
#include "unzip.h"
int8_t tempfilecartname_[512];
#endif

int32_t result;
int8_t SaveName[512]; 
int8_t StateName[512];
int8_t IEepPath[512]; 

#ifdef ZIP_SUPPORT  
int32_t check_zip(char *filename)
{
    uint8_t buf[2];
    FILE *fd = NULL;
    fd = fopen(filename, "rb");
    if(!fd) return (0);
    fread(buf, 2, 1, fd);
    fclose(fd);
    if(memcmp(buf, "PK", 2) == 0) return (1);
    return (0);
}
#endif

int32_t WsSetPdata(void)
{
    ROMBanks = 4;
	RAMBanks = 1;
	RAMSize = 0x2000;
	CartKind = 0;
    if ((ROMMap[0xFF] = (uint8_t*)malloc(0x10000)) == NULL)
    {
		fprintf(stderr,"WsSetPdata\n");
        return 0;
    }
    WsReset();
    HVMode = 0;
    return 1;
}

int32_t WsCreate(int8_t *CartName)
{
    int32_t Checksum, i, j;
    FILE* fp;
    uint8_t buf[16];

    for (i = 0; i < 256; i++)
    {
        ROMMap[i] = MemDummy;
        RAMMap[i] = MemDummy;
    }
    memset(IRAM, 0, sizeof(IRAM));
    memset(MemDummy, 0xA0, sizeof(MemDummy));
    memset(IO, 0, sizeof(IO));
    
    if (CartName == NULL)
    {
        return WsSetPdata();
    }
 
#ifdef ZIP_SUPPORT   
	if(check_zip(CartName))
	{
		#define READ_SIZE 8192
		int8_t read_buffer[READ_SIZE];
		unzFile *zipfile = unzOpen(CartName);
		unz_global_info global_info;
		if ( unzGetGlobalInfo( zipfile, &global_info ) != UNZ_OK )
		{
			printf( "Could not read file global info\n" );
			unzClose( zipfile );
			return -1;
		}

        /* Get info about current file. */
        unz_file_info file_info;
        int8_t filename[512];
        if ( unzGetCurrentFileInfo( zipfile, &file_info, filename, 512, NULL, 0, NULL, 0 ) != UNZ_OK )
        {
            printf( "Could not read file info\n" );
            unzClose( zipfile );
            return -1;
        }
        
		/* Entry is a file, so extract it. */
		if ( unzOpenCurrentFile( zipfile ) != UNZ_OK )
		{
			printf( "Could not open file\n" );
			unzClose( zipfile );
			return -1;
		}
		
		
		snprintf(tempfilecartname_, sizeof(tempfilecartname_), "%s.ws", CartName);
		
		/* Open a file to write out the data. */
		int8_t tempfilename[512];
		snprintf(tempfilename, sizeof(tempfilename), "%s%s%s%s", PATH_DIRECTORY, SAVE_DIRECTORY, tempfilecartname_, EXTENSION);
		
		FILE *out = fopen( tempfilename, "wb" );
		if ( out == NULL )
		{
			printf( "could not open destination file\n" );
			unzCloseCurrentFile( zipfile );
			unzClose( zipfile );
			return -1;
		}
		
		int16_t error = UNZ_OK;
		do    
		{
			error = unzReadCurrentFile( zipfile, read_buffer, READ_SIZE );
			if ( error < 0 )
			{
				printf( "Error %d\n", error );
				unzCloseCurrentFile( zipfile );
				unzClose( zipfile );
				return -1;
			}
			if ( error > 0 )
			{
				/* Write data to file. */
				fwrite( read_buffer, error, 1, out );
			}
		} while ( error > 0 );

        fclose( out );
        unzCloseCurrentFile( zipfile );
        
        /*
         * This could be improved, obviously.
         * It should read from memory instead of writing to a file
         * and then reading a from it.
         * Sure, it is deleted right afterwards but on read-only systems,
         * this is not even a possibility.
         * It still works on Sega Dreamcast because it is put in RAM.
         * It might not be able to load big roms though...
        */
        
		if ((fp = fopen(tempfilename, "rb")) == NULL)
		{
			fprintf(stderr,"ERR_FOPEN\n");
			return 1;
		}
		remove(tempfilename);
	}
	else
	{
#endif
		if ((fp = fopen(CartName, "rb")) == NULL)
		{
			fprintf(stderr,"ERR_FOPEN\n");
			return 1;
		}
#ifdef ZIP_SUPPORT
	}
#endif
    
    /* ws_romsize = sizeof(fp); */

    result = fseek(fp, -10, SEEK_END);
    if (fread(buf, 1, 10, fp) != 10)
    {
		fprintf(stderr,"ERR_FREAD_ROMINFO\n");
		if (fp) fclose(fp);
        return 1;
    }
    switch (buf[4])
    {
    case 1:
        ROMBanks = 4;
        break;
    case 2:
        ROMBanks = 8;
        break;
    case 3:
        ROMBanks = 16;
        break;
    case 4:
        ROMBanks = 32;
        break;
    case 5:
        ROMBanks = 48;
        break;
    case 6:
        ROMBanks = 64;
        break;
    case 7:
        ROMBanks = 96;
        break;
    case 8:
        ROMBanks = 128;
        break;
    case 9:
        ROMBanks = 256;
        break;
    default:
        ROMBanks = 0;
        break;
    }
    if (ROMBanks == 0)
    {
		fprintf(stderr,"ERR_ILLEGAL_ROMSIZE\n");
        return 1;
    }
    switch (buf[5])
    {
    case 0x01:
        RAMBanks = 1;
        RAMSize = 0x2000;
        CartKind = 0;
        break;
    case 0x02:
        RAMBanks = 1;
        RAMSize = 0x8000;
        CartKind = 0;
        break;
    case 0x03:
        RAMBanks = 2;
        RAMSize = 0x20000;
        CartKind = 0;
        break;
    case 0x04:
        RAMBanks = 4;
        RAMSize = 0x40000;
        CartKind = 0;
        break;
    case 0x10:
        RAMBanks = 1;
        RAMSize = 0x80;
        CartKind = CK_EEP;
        break;
    case 0x20:
        RAMBanks = 1;
        RAMSize = 0x800;
        CartKind = CK_EEP;
        break;
    case 0x50:
        RAMBanks = 1;
        RAMSize = 0x400;
        CartKind = CK_EEP;
        break;
    default:
        RAMBanks = 1;
        RAMSize = 0x2000;
        CartKind = 0;
        break;
    }

    WsRomPatch(buf);
    
    Checksum = (int32_t)((buf[9] << 8) + buf[8]);
    Checksum += (int32_t)(buf[9] + buf[8]);
    for (i = ROMBanks - 1; i >= 0; i--)
    {
        fseek(fp, (ROMBanks - i) * -0x10000, 2);
        if ((ROMMap[0x100 - ROMBanks + i] = (uint8_t*)malloc(0x10000)) != NULL)
        {
            if (fread(ROMMap[0x100 - ROMBanks + i], 1, 0x10000, fp) == 0x10000)
            {
                for (j = 0; j < 0x10000; j++)
                {
                    Checksum -= ROMMap[0x100 - ROMBanks + i][j];
                }
            }
        }
        else
        {
			fprintf(stderr,"ERR_MALLOC\n");
            return 1;
        }
    }
    fclose(fp);
    if (i >= 0)
    {
        return 0;
    }
    if (Checksum & 0xFFFF)
    {
		fprintf(stderr,"ERR_CHECKSUM\n");
    }
    if (RAMBanks)
    {
        for (i = 0; i < RAMBanks; i++)
        {
            if ((RAMMap[i] = (uint8_t*)malloc(0x10000)) != NULL)
            {
                memset(RAMMap[i], 0x00, 0x10000);
            }
            else
            {
				fprintf(stderr,"ERR_MALLOC 1\n");
				return 1;
            }
        }
    }
    if (RAMSize)
    {
		int8_t* tmp =  strstr(CartName, "/");
		if (tmp == NULL)
		{
			snprintf(SaveName, sizeof(SaveName), "%s%s%s.epm%s", PATH_DIRECTORY, SAVE_DIRECTORY, CartName, EXTENSION);
		}
		else
		{
			snprintf(SaveName, sizeof(SaveName), "%s%s%s.epm%s", PATH_DIRECTORY, SAVE_DIRECTORY, strrchr(CartName, '/')+1, EXTENSION);
		}
        
        if ((fp = fopen(SaveName, "rb")) != NULL)
        {
            for (i = 0; i < RAMBanks; i++)
            {
                if (RAMSize < 0x10000)
                {
                    if (fread(RAMMap[i], 1, RAMSize, fp) != RAMSize)
                    {
						fprintf(stderr,"ERR_FREAD_SAVE\n");
						break;
                    }
                }
                else
                {
                    if (fread(RAMMap[i], 1, 0x10000, fp) != 0x10000)
                    {
						fprintf(stderr,"ERR_FREAD_SAVE 1\n");
                        break;
                    }
                }
            }
            fclose(fp);
        }
        else
        {
			fp = fopen(SaveName, "w");
			fclose(fp);
		}
    }
    else
    {
        SaveName[0] = 0;
    }
    WsReset();
	HVMode = buf[6] & 1;
    
	return 1;
}

void WsRelease(void)
{
    FILE* fp;
    int32_t i;

    if (SaveName[0] != 0)
    {
        if ((fp = fopen(SaveName, "wb"))!= NULL)
        {
            for (i  =0; i < RAMBanks; i++)
            {
                if (RAMSize<0x10000)
                {
                    if (fwrite(RAMMap[i], 1, RAMSize, fp) != RAMSize)
                    {
                        break;
                    }
                }
                else
                {
                    if (fwrite(RAMMap[i], 1, 0x10000, fp)!=0x10000)
                    {
                        break;
                    }
                }
                free(RAMMap[i]);
                RAMMap[i] = NULL;
            }
            fclose(fp);
        }
        SaveName[0] = '\0';
    }
    for (i = 0xFF; i >= 0; i--)
    {
        if (ROMMap[i] == MemDummy)
        {
            break;
        }
        free(ROMMap[i]);
        ROMMap[i] = MemDummy;
    }
    StateName[0] = '\0';
}

void WsLoadEeprom(void)
{
    FILE* fp;

	snprintf(IEepPath, sizeof(IEepPath), "%s%s%s.epm%s", PATH_DIRECTORY, SAVE_DIRECTORY, "oswan.dat", EXTENSION);

    if ((fp = fopen(IEepPath, "rb")) != NULL)
    {
        result = fread(IEep, sizeof(uint16_t), 64, fp);
        fclose(fp);
    }
	else
	{
		uint16_t* p = IEep + 0x30;
		memset(IEep, 0xFF, 0x60);
		memset(p, 0, 0x20);
		*p++ = 0x211D;
		*p++ = 0x180B;
		*p++ = 0x1C0D;
		*p++ = 0x1D23;
		*p++ = 0x0B1E;
		*p   = 0x0016;
	}
}

void WsSaveEeprom(void)
{
    FILE* fp;

	snprintf(IEepPath, sizeof(IEepPath), "%s%s%s.epm%s", PATH_DIRECTORY, SAVE_DIRECTORY, "oswan.dat", EXTENSION);

    if ((fp = fopen(IEepPath, "wb")) != NULL)
    {
        fwrite(IEep, sizeof(uint16_t), 64, fp);
		if (fp) fclose(fp);
    }
}

#define MacroLoadNecRegisterFromFile(F,R) \
		result = fread(&value, sizeof(uint32_t), 1, fp); \
	    nec_set_reg(R,value); 
int32_t WsLoadState(const int8_t *savename, int32_t num)
{
    FILE* fp;
    int8_t buf[256];
	uint32_t value;
	int32_t i;
	
	snprintf(buf, sizeof(buf), "%s%s%s.%d.sta%s", PATH_DIRECTORY, SAVE_DIRECTORY, strrchr(savename,'/')+1, num, EXTENSION);
    if ((fp = fopen(buf, "rb")) == NULL)
    {
		return 1;
	}
	MacroLoadNecRegisterFromFile(fp,NEC_IP);
	MacroLoadNecRegisterFromFile(fp,NEC_AW);
	MacroLoadNecRegisterFromFile(fp,NEC_BW);
	MacroLoadNecRegisterFromFile(fp,NEC_CW);
	MacroLoadNecRegisterFromFile(fp,NEC_DW);
	MacroLoadNecRegisterFromFile(fp,NEC_CS);
	MacroLoadNecRegisterFromFile(fp,NEC_DS);
	MacroLoadNecRegisterFromFile(fp,NEC_ES);
	MacroLoadNecRegisterFromFile(fp,NEC_SS);
	MacroLoadNecRegisterFromFile(fp,NEC_IX);
	MacroLoadNecRegisterFromFile(fp,NEC_IY);
	MacroLoadNecRegisterFromFile(fp,NEC_BP);
	MacroLoadNecRegisterFromFile(fp,NEC_SP);
	MacroLoadNecRegisterFromFile(fp,NEC_FLAGS);
	MacroLoadNecRegisterFromFile(fp,NEC_VECTOR);
	MacroLoadNecRegisterFromFile(fp,NEC_PENDING);
	MacroLoadNecRegisterFromFile(fp,NEC_NMI_STATE);
	MacroLoadNecRegisterFromFile(fp,NEC_IRQ_STATE);
    result = fread(IRAM, sizeof(uint8_t), 0x10000, fp);
    result = fread(IO, sizeof(uint8_t), 0x100, fp);
    for (i  =0; i < RAMBanks; i++)
    {
        if (RAMSize < 0x10000)
        {
            result = fread(RAMMap[i], 1, RAMSize, fp);
        }
        else
        {
            result = fread(RAMMap[i], 1, 0x10000, fp);
        }
    }
	result = fread(Palette, sizeof(uint16_t), 16 * 16, fp);
    fclose(fp);
	WriteIO(0xC1, IO[0xC1]);
	WriteIO(0xC2, IO[0xC2]);
	WriteIO(0xC3, IO[0xC3]);
	WriteIO(0xC0, IO[0xC0]);
//	for (i = 0x80; i <= 0x90; i++)
	for (i = 0x07; i <= 0xB3; i++)
	{
		WriteIO(i, IO[i]);
	}
	
    return 0;
}

#define MacroStoreNecRegisterToFile(F,R) \
	    value = nec_get_reg(R); \
		fwrite(&value, sizeof(uint32_t), 1, fp);
		
int32_t WsSaveState(const int8_t *savename, int32_t num)
{
    FILE* fp;
    int8_t buf[256];
	uint32_t value;
	int32_t i;
	
	snprintf(buf, sizeof(buf), "%s%s%s.%d.sta%s", PATH_DIRECTORY, SAVE_DIRECTORY, strrchr(savename,'/')+1, num, EXTENSION);
    if ((fp = fopen(buf, "w+")) == NULL)
    {
		printf("FAILURE...\n");
		return 1;
	}
	MacroStoreNecRegisterToFile(fp,NEC_IP);
	MacroStoreNecRegisterToFile(fp,NEC_AW);
	MacroStoreNecRegisterToFile(fp,NEC_BW);
	MacroStoreNecRegisterToFile(fp,NEC_CW);
	MacroStoreNecRegisterToFile(fp,NEC_DW);
	MacroStoreNecRegisterToFile(fp,NEC_CS);
	MacroStoreNecRegisterToFile(fp,NEC_DS);
	MacroStoreNecRegisterToFile(fp,NEC_ES);
	MacroStoreNecRegisterToFile(fp,NEC_SS);
	MacroStoreNecRegisterToFile(fp,NEC_IX);
	MacroStoreNecRegisterToFile(fp,NEC_IY);
	MacroStoreNecRegisterToFile(fp,NEC_BP);
	MacroStoreNecRegisterToFile(fp,NEC_SP);
	MacroStoreNecRegisterToFile(fp,NEC_FLAGS);
	MacroStoreNecRegisterToFile(fp,NEC_VECTOR);
	MacroStoreNecRegisterToFile(fp,NEC_PENDING);
	MacroStoreNecRegisterToFile(fp,NEC_NMI_STATE);
	MacroStoreNecRegisterToFile(fp,NEC_IRQ_STATE);
    fwrite(IRAM, sizeof(uint8_t), 0x10000, fp);
    fwrite(IO, sizeof(uint8_t), 0x100, fp);
    for (i  =0; i < RAMBanks; i++)
    {
        if (RAMSize < 0x10000)
        {
            fwrite(RAMMap[i], 1, RAMSize, fp);
        }
        else
        {
            fwrite(RAMMap[i], 1, 0x10000, fp);
        }
    }
	fwrite(Palette, sizeof(uint16_t), 16 * 16, fp);
    fclose(fp);

    return 0;
}


