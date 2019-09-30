/*
Watcom C - Simple drives base port and control port detection for 32-bit protected mode using INT 13h API
Written by Piotr Ulaszewski (pulaweb) on the 30th of September 2019
Free to be used for any purpose
*/

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>

#ifndef BOOL
typedef int BOOL;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;
typedef unsigned long  QWORD;
typedef unsigned char  UCHAR;
typedef unsigned short USHORT;
typedef unsigned int   UINT;
typedef unsigned long  ULONG;
#endif

#ifndef LOWORD
#define LOWORD(l)      ((WORD)((DWORD)(l)))
#define HIWORD(l)      ((WORD)(((DWORD)(l)>>16)&0xFFFF))
#define LOBYTE(w)      ((BYTE)(w))
#define HIBYTE(w)      ((BYTE)(((WORD)(w)>>8)&0xFF))
#endif

// DPMI regs structure to simulate real mode interrupt
#pragma pack (push, 1);
typedef struct _DPMIREGS{
    DWORD edi;
    DWORD esi;
    DWORD ebp;
    DWORD reserved;
    DWORD ebx;
    DWORD edx;
    DWORD ecx;
    DWORD eax;
    WORD flags;
    WORD es;
    WORD ds;
    WORD fs;
    WORD gs;
    WORD ip;
    WORD cs;
    WORD sp;
    WORD ss;
} DPMIREGS;

typedef struct {
    WORD buffer_size;
    WORD info_flags;
    DWORD physical_cylinders;
    DWORD physical_heads;
    DWORD physical_sectors_per_track;
    DWORD total_sectors;
    DWORD total_sectors_hidword;
    WORD bytes_per_sector;
    // v2.0+
    DWORD config_param;                     // pointer to Device Parameter Table Extension
    // v3.0+
    WORD path_signaturs;                    // 0BEDD = presence of device path information
    BYTE path_length;
    BYTE reserved1[3];
    BYTE host_bus[4];                       // PCI or ISA
    BYTE interface_type[8];                 // ATA, ATAPI, SCSI, USB, 1394, FIBRE
    BYTE interface_path[8];
    BYTE device_path[8];
    BYTE reserved2;
    BYTE checksum;
} DRIVEPARAM;

typedef struct {
    WORD   base_port;                       // physical I/O port base address
    WORD   control_port;                    // disk-drive control port address
    BYTE   flags;
    BYTE   proprietary;
    BYTE   irq;                             // bits 3-0; bits 7-4 reserved and must be 0
    BYTE   multi_sector;                    // sector count for multi-sector transfers
    BYTE   dma_control;                     // bits 7-4: DMA type (0-2), bits 3-0: DMA channel
    BYTE   pio_control;                     // bits 7-4: reserved (0), bits 3-0: PIO type (1-4)
    WORD   options;
    WORD   reserved;                        // 0
    BYTE   ext_revision;                    // extension revision level
    BYTE   checksum;
}
CONFIGPARAM;

typedef struct {
    BYTE structure_size;
    BYTE reserved;
    WORD sectors_to_transfer;
    WORD transfer_buffer_offset;
    WORD transfer_buffer_segment;
    __int64 starting_sector;
    __int64 flat_transfer_buffer;
} DISKADDRESSPACKET;
#pragma pack (pop);

BOOL DPMI_SimulateRMI (BYTE IntNum, DPMIREGS *regs);
BOOL DPMI_DOSmalloc (QWORD size, WORD *segment, WORD *selector);
void DPMI_DOSfree (WORD *selector);


int main (void) {
    int bios_ext_major = 0;               // simply info
    int bios_ext_minor = 0;
    WORD bios_ext_api = 0;                // api support

    WORD DriveParam_RealSegment = 0;      // for hard disk drive parameters
    WORD DriveParam_MemSelector = 0;      // for hard disk drive parameters
    DRIVEPARAM *DriveParam;               // for hard disk drive parameters
    CONFIGPARAM *ConfigParam;
    DPMIREGS dpmiregs;
    int i;
	
	
    // check int 13h extensions supported
    memset(&dpmiregs, 0, sizeof(DPMIREGS));
    dpmiregs.eax = 0x4100;
    dpmiregs.ebx = 0x55aa;
    dpmiregs.edx = 0x80;
    DPMI_SimulateRMI(0x13, &dpmiregs);
    if (dpmiregs.flags & 1) {
        printf("Sorry INT13H extensions not supported or no hard drive!\n");
        exit(0);
    }
    if (LOWORD(dpmiregs.ebx) != 0xaa55) {
        printf("Sorry INT13H extensions not active or no hard drive!\n");
        exit(0);
    }

    // INT 13h extensions supported
    bios_ext_major = HIBYTE(dpmiregs.eax) >> 4;
    bios_ext_minor = HIBYTE(dpmiregs.eax) & 0xF;
    bios_ext_api = dpmiregs.ecx & 0xFFFF;


    // search for hard disks
    // allocate low DOS memory for drive param buffer
    if (!DPMI_DOSmalloc(512, &DriveParam_RealSegment, &DriveParam_MemSelector)) {
        // DPMI_DOSMalloc error
        printf("Error allocating DOS memory");
        exit(0);
    }
    DriveParam = (DRIVEPARAM *) (DriveParam_RealSegment << 4);


    // BIOS hard drives start from 0x80
    i = 0x80;
    while (i < 0x100) {

        // get drive parameters - int 13h extensions call
        memset(&dpmiregs, 0, sizeof(DPMIREGS));
        memset(DriveParam, 0, sizeof(DRIVEPARAM));
        switch ((bios_ext_major << 4) + bios_ext_minor) {
            case 0x01 : DriveParam->buffer_size = 0x1A; break;
            case 0x20 :
            case 0x21 : DriveParam->buffer_size = 0x1E; break;
            case 0x30 : DriveParam->buffer_size = 0x42; break;
            default   : DriveParam->buffer_size = 0x200;
        }

        dpmiregs.eax = 0x4800;
        dpmiregs.edx = i;
        dpmiregs.esi = 0;
        dpmiregs.ds = DriveParam_RealSegment;
        DPMI_SimulateRMI(0x13, &dpmiregs);

        if (!(dpmiregs.flags & 1) && !HIBYTE(dpmiregs.eax) && DriveParam->total_sectors) {   // test if no CF set and no error code in AH

            // int 13h extensions must be verison 2 or higher and config param structure must be filled
            if ((bios_ext_major >= 2) && (bios_ext_api & 0x01) && (DriveParam->config_param != 0xFFFFFFFF)) {
                // set config param structure
                ConfigParam = (CONFIGPARAM *) ( ((DriveParam->config_param >> 16) << 4) + (WORD)DriveParam->config_param );

                    // skip detection via ATA/SATA if buffer size is less than 30, config_param pointer is not returned in this case
                    if ((DriveParam->buffer_size < 30) || (ConfigParam == NULL)) {
			printf("Config parameters structure not found for 0x%x HDD\n", i);
			break;
		    }
		    
                    if (DriveParam->info_flags & (1 + 4) && !DriveParam->config_param) {
                        // removable drive
                    } else {
                        // hard drive
                        if (ConfigParam->flags & 0x10) printf ("HDD 0x%x is slave, HDD Base port is: 0x%x, HDD Control port is: 0x%x\n", i, ConfigParam->base_port, ConfigParam->control_port);
                        else printf ("HDD 0x%x is master, HDD Base port is: 0x%x, HDD Control port is: 0x%x\n", i, ConfigParam->base_port, ConfigParam->control_port);
                    }
            } else {
                printf ("INT 13h extensions version too low or no config parameters structure.\nUnable to detect hard drive base and control port!\n");
            }
        }
        // next drive
        i++;
    }
    return 1;
}


/******************/
/* DPMI functions */
/******************/

BOOL DPMI_SimulateRMI (BYTE IntNum, DPMIREGS *regs) {
	BYTE noerror = 0;

	_asm {
	mov edi, [regs]
        sub ecx,ecx
        sub ebx,ebx
        mov bl, [IntNum]
        mov eax, 0x300
        int 0x31
        setnc [noerror]
        }	
        return noerror;
}

BOOL DPMI_DOSmalloc (QWORD size, WORD *segment, WORD *selector) {
	BYTE noerror = 0;
	
	_asm {
        mov eax, 0x100
        mov ebx, [size]
        add ebx, 0x0f
        shr ebx, 4
        int 0x31
        setnc [noerror]
        mov ebx, [segment]
        mov [ebx], ax
        mov ebx, [selector]
        mov [ebx], dx
        }
        return noerror;
}

void DPMI_DOSfree (WORD *selector) {
	
	_asm {
        mov eax, [selector]
        mov dx, [eax]
        mov eax, 0x101
        int 0x31
        }
}

