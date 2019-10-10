
/*
Watcom C - Simple ATA commands for 32-bit protected mode
The user is responsible to make sure enough allocated memory is available when passing the buffer pointer
Written by Piotr Ulaszewski (pulaweb) on the 7th of October 2019
This is just a demonstartion on how to obtain hard drive info on any ATA legacy system
*/

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <i86.h>
#include <string.h> 
#include "drives.h"
#include "ata.h"


// ATA status and error registers
extern BYTE status_register;
extern BYTE device_register;
extern BYTE chigh_register;
extern BYTE clow_register;
extern BYTE sector_register;
extern BYTE count_register;
extern BYTE error_register;

extern BYTE lbahigh07_register;
extern BYTE lbahigh815_register;
extern BYTE lbamid07_register;
extern BYTE lbamid815_register;
extern BYTE lbalow07_register;
extern BYTE lbalow815_register;
extern BYTE count07_register;
extern BYTE count815_register;

WORD buff[256];

// direction 0 - no data, 1 - from drive, 2 - to drive
BOOL ata_send_command (BYTE command, BYTE features, BYTE count, BYTE direction, DISKDRIVE *sdrive, BYTE *buffer)
{
    return ata_send_command_extended (command, features, count, 0, 0, 0, 0, direction, sdrive, buffer);
}

// direction 0 - no data, 1 - from drive, 2 - to drive
BOOL ata_send_command_extended (BYTE command, BYTE features, BYTE count, BYTE sector, BYTE clow, BYTE chigh, BYTE device, BYTE direction, DISKDRIVE *sdrive, BYTE *buffer)
{
    unsigned __int64 Frequency;
    unsigned __int64 WaitStart;
    unsigned __int64 WaitEnd;
    DWORD WaitTime;

// Read Function Write Function
// ----------------------------------------------------------------------
// 0 1 0 0 0 1 F0 Data Register / Data Register
// 0 1 0 0 1 1 F1 Error Register / (Write Precomp Reg.)
// 0 1 0 1 0 1 F2 Sector / Count Sector Count
// 0 1 0 1 1 1 F3 Sector Number / Sector Number
// 0 1 1 0 0 1 F4 Cylinder Low / Cylinder Low
// 0 1 1 0 1 1 F5 Cylinder High / Cylinder High
// 0 1 1 1 0 1 F6 SDH Register / SDH Register
// 0 1 1 1 1 1 F7 Status Register / Command Register
// ----------------------------------------------------------------------
// status register flags
// Bit 7: BSY (Busy) Set when the drive is busy and unable to process any new ATA commands.
// Bit 6: DRDY (Data Ready) Set when the device is ready to accept ATA commands from the host.
// Bit 5: DWF (Drive Write Fault) Always set to 0.
// Bit 4: DSC (Drive Seek Complete) Set when the drive heads have been positioned over a specific track.
// Bit 3: DRQ (Data Request) Set when device is read to transfer a word or byte of data to or from the host and the device.
// Bit 2: CORR (Corrected Data) Always set to 0.
// Bit 1: IDX (Index) Always set to 0.
// Bit 0: ERR (Error) Set when an error occurred during the previous ATA command.


    // get direct access ports
    WORD ata_base = sdrive->ata_base;
    WORD ata_ctrl = sdrive->ata_ctrl;
    BOOL ata_master = sdrive->ata_master;
    BYTE *bufferptr = buffer;
    WORD in_val = 0;
    int idx = 0;

    QueryPerformanceFrequency (&Frequency);     // frequency of the high resolution timer - ticks for 1 ns

    // wait for controller not busy
    QueryPerformanceCounter (&WaitStart);
    while (TRUE) {
        in_val = inp(ata_base + 7);
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        if (in_val & 0x80) {                    // Drive in BSY state, try to reset controller
            outp(ata_ctrl, 0x06);               // perform device reset (SRST + nIEN bits)
            _asm nop;
            _asm nop;
            _asm nop;
            _asm nop;
            delay(10);
            outp(ata_ctrl, 0x02);               // perform device reset (nIEN bits)
            _asm nop;
            _asm nop;
            _asm nop;
            _asm nop;
            delay(10);
        }
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        if (((in_val & 0x40) == 0x40) && ((in_val & 0x80) == 0x00)) break;
        QueryPerformanceCounter (&WaitEnd);
        WaitTime = (DWORD) ((unsigned __int64)(1000 * (WaitEnd - WaitStart)/Frequency));   // calculate wait time in ms
        if (WaitTime > TIMEOUT) {
            status_register = in_val;
            error_register = inp(ata_base + 1);
            return FALSE;
        }
    }

    // device ignored - master/slave taken from sdrive
    outp(ata_base + 6, (ata_master ? (0xA0 + 0x40 + (device & 0xf)) : (0xB0 + 0x40 + (device & 0xf))));
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // LBA high
    outp(ata_base + 5, (BYTE)chigh);                                  // cylinder high register (23-16 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // LBA mid
    outp(ata_base + 4, (BYTE)clow);                                   // cylinder low register (15-8 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // LBA low
    outp(ata_base + 3, (BYTE)sector);                                 // sector number (0-7 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // sector count
    outp(ata_base + 2, (BYTE)count);                                  // sector count reg - count
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // feature
    outp(ata_base + 1, (BYTE)features);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // command
    outp(ata_base + 7, (BYTE)command);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // Wait for data ready
    QueryPerformanceCounter (&WaitStart);
    while (TRUE) {
        in_val = inp(ata_base + 7);
        if ((in_val & 0x01) == 0x01) {
            status_register = in_val;
            error_register = inp(ata_base + 1);
            return FALSE;                            // ERR - Drive error condition
        }
        if ((in_val & 0x80) == 0x00) break;          // Drive not in BSY state
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        QueryPerformanceCounter (&WaitEnd);
        WaitTime = (DWORD) ((unsigned __int64)(1000 * (WaitEnd - WaitStart)/Frequency));   // calculate wait time in ms
        // unlimited wait time for secure erase 0xf4
        if ((command != 0xf4) && (WaitTime > TIMEOUT)) {
            // status_register = in_val;
            // error_register = inp(ata_base + 1);
            status_register = 0xff;
            error_register = 0xff;
            return FALSE;
        }
    }

    // read/write 256 words if command returns/sends data
    while ((in_val & 0x48) == 0x48) {
        if (direction == 1) {
            // receive
            for (idx = 0; idx != 256; idx++) buff[idx] = inpw(ata_base);
            memcpy(bufferptr, buff, 512);
            bufferptr += 512;
        } else if (direction == 2) {
            // send
            memcpy(buff, bufferptr, 512);
            for (idx = 0; idx != 256; idx++) outpw(ata_base, buff[idx]);
            bufferptr += 512;
        } else {
            break;
        }
        delay(10);
        in_val = inp(ata_base + 7);
    }

    QueryPerformanceCounter (&WaitStart);
    while (TRUE) {
        in_val = inp(ata_base + 7);
        if ((in_val & 0x01) == 0x01) {
            status_register = in_val;
            error_register = inp(ata_base + 1);
            return FALSE;                                // drive error ERR
        }
        if ((in_val & 0x80) == 0x00) break;              // drive not in BSY state
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        QueryPerformanceCounter (&WaitEnd);
        WaitTime = (DWORD) ((unsigned __int64)(1000 * (WaitEnd - WaitStart)/Frequency));   // calculate wait time in ms

        // unlimited wait time for secure erase 0xf4
        if ((command != 0xf4) && (WaitTime > TIMEOUT)) {
            // status_register = in_val;
            // error_register = inp(ata_base + 1);
            status_register = 0xff;
            error_register = 0xff;
            return FALSE;
        }
    }

    // return status
    delay(10);
    status_register = in_val;
    error_register = inp(ata_base + 1);

    device_register = inp(ata_base + 6);
    chigh_register = inp(ata_base + 5);
    clow_register = inp(ata_base + 4);
    sector_register = inp(ata_base + 3);
    count_register = inp(ata_base + 2);

    if (error_register) return FALSE;
    return TRUE;
}

BOOL ata_send_command_extended_48bit (BYTE command, BYTE features, BYTE count, BYTE sector, BYTE clow, BYTE chigh, BYTE device, BYTE featuresh, BYTE counth, BYTE sectorh, BYTE clowh, BYTE chighh, BYTE direction, DISKDRIVE *sdrive, BYTE *buffer)
{
    unsigned __int64 Frequency;
    unsigned __int64 WaitStart;
    unsigned __int64 WaitEnd;
    DWORD WaitTime;

// Read Function Write Function
// ----------------------------------------------------------------------
// 0 1 0 0 0 1 F0 Data Register / Data Register
// 0 1 0 0 1 1 F1 Error Register / (Write Precomp Reg.)
// 0 1 0 1 0 1 F2 Sector / Count Sector Count
// 0 1 0 1 1 1 F3 Sector Number / Sector Number
// 0 1 1 0 0 1 F4 Cylinder Low / Cylinder Low
// 0 1 1 0 1 1 F5 Cylinder High / Cylinder High
// 0 1 1 1 0 1 F6 SDH Register / SDH Register
// 0 1 1 1 1 1 F7 Status Register / Command Register
// ----------------------------------------------------------------------
// status register flags
// Bit 7: BSY (Busy) Set when the drive is busy and unable to process any new ATA commands.
// Bit 6: DRDY (Data Ready) Set when the device is ready to accept ATA commands from the host.
// Bit 5: DWF (Drive Write Fault) Always set to 0.
// Bit 4: DSC (Drive Seek Complete) Set when the drive heads have been positioned over a specific track.
// Bit 3: DRQ (Data Request) Set when device is read to transfer a word or byte of data to or from the host and the device.
// Bit 2: CORR (Corrected Data) Always set to 0.
// Bit 1: IDX (Index) Always set to 0.
// Bit 0: ERR (Error) Set when an error occurred during the previous ATA command.

    // get direct access ports
    WORD ata_base = sdrive->ata_base;
    WORD ata_ctrl = sdrive->ata_ctrl;
    BOOL ata_master = sdrive->ata_master;
    BYTE *bufferptr = buffer;
    WORD in_val = 0;
    int idx = 0;

    QueryPerformanceFrequency (&Frequency);     // frequency of the high resolution timer - ticks for 1 ns

    // wait for controller not busy
    QueryPerformanceCounter (&WaitStart);
    while (TRUE) {
        in_val = inp(ata_base + 7);
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        if (in_val & 0x80) {                    // Drive in BSY state, try to reset controller
            outp(ata_ctrl, 0x06);               // perform device reset (SRST + nIEN bits)
            _asm nop;
            _asm nop;
            _asm nop;
            _asm nop;
            delay(10);
            outp(ata_ctrl, 0x02);               // perform device reset (nIEN bits)
            _asm nop;
            _asm nop;
            _asm nop;
            _asm nop;
            delay(10);
        }
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        if (((in_val & 0x40) == 0x40) && ((in_val & 0x80) == 0x00)) break;
        QueryPerformanceCounter (&WaitEnd);
        WaitTime = (DWORD) ((unsigned __int64)(1000 * (WaitEnd - WaitStart)/Frequency));   // calculate wait time in ms
        if (WaitTime > TIMEOUT) {
            status_register = in_val;
            error_register = inp(ata_base + 1);
            return FALSE;
        }
    }

    // device ignored - master/slave taken from sdrive
    outp(ata_base + 6, (ata_master ? (0xA0 + 0x40 + (device & 0xf)) : (0xB0 + 0x40 + (device & 0xf))));
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // previous content
    outp(ata_base + 5, (BYTE)chighh);                                 // cylinder high register (47-40 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // previous content
    outp(ata_base + 4, (BYTE)clowh);                                  // cylinder low register (39-32 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // previous content
    outp(ata_base + 3, (BYTE)sectorh);                                // sector number (31-24 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // previous content
    outp(ata_base + 2, (BYTE)counth);                                 // sector count reg - count
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // previous content
    outp(ata_base + 1, (BYTE)featuresh);                              // features high
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // device ignored - master/slave taken from sdrive
    outp(ata_base + 6, (ata_master ? (0xA0 + 0x40) : (0xB0 + 0x40 )));
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // LBA high
    outp(ata_base + 5, (BYTE)chigh);                                  // cylinder high register (23-16 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // LBA mid
    outp(ata_base + 4, (BYTE)clow);                                   // cylinder low register (15-8 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // LBA low
    outp(ata_base + 3, (BYTE)sector);                                 // sector number (0-7 lba)
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // sector count
    outp(ata_base + 2, (BYTE)count);                                  // sector count reg - count
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // feature
    outp(ata_base + 1, (BYTE)features);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // command
    outp(ata_base + 7, (BYTE)command);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    // Wait for data ready
    QueryPerformanceCounter (&WaitStart);
    while (TRUE) {
        in_val = inp(ata_base + 7);
        if ((in_val & 0x01) == 0x01) {
            status_register = in_val;
            error_register = inp(ata_base + 1);
            return FALSE;                            // ERR - Drive error condition
        }
        if ((in_val & 0x80) == 0x00) break;          // Drive not in BSY state
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        QueryPerformanceCounter (&WaitEnd);
        WaitTime = (DWORD) ((unsigned __int64)(1000 * (WaitEnd - WaitStart)/Frequency));   // calculate wait time in ms
        // unlimited wait time for secure erase 0xf4
        if ((command != 0xf4) && (WaitTime > TIMEOUT)) {
            // status_register = in_val;
            // error_register = inp(ata_base + 1);
            status_register = 0xff;
            error_register = 0xff;
            return FALSE;
        }
    }

    // read/write 256 words if command returns/sends data
    while ((in_val & 0x48) == 0x48) {
        if (direction == 1) {
            // receive
            for (idx = 0; idx != 256; idx++) buff[idx] = inpw(ata_base);
            memcpy(bufferptr, buff, 512);
            bufferptr += 512;
        } else if (direction == 2) {
            // send
            memcpy(buff, bufferptr, 512);
            for (idx = 0; idx != 256; idx++) outpw(ata_base, buff[idx]);
            bufferptr += 512;
        } else {
            break;
        }
        delay(10);
        in_val = inp(ata_base + 7);
    }

    QueryPerformanceCounter (&WaitStart);
    while (TRUE) {
        in_val = inp(ata_base + 7);
        if ((in_val & 0x01) == 0x01) {
            status_register = in_val;
            error_register = inp(ata_base + 1);
            return FALSE;                                // drive error ERR
        }
        if ((in_val & 0x80) == 0x00) break;              // drive not in BSY state
        _asm nop;
        _asm nop;
        _asm nop;
        _asm nop;
        QueryPerformanceCounter (&WaitEnd);
        WaitTime = (DWORD) ((unsigned __int64)(1000 * (WaitEnd - WaitStart)/Frequency));   // calculate wait time in ms

        if (WaitTime > TIMEOUT) {
            // status_register = in_val;
            // error_register = inp(ata_base + 1);
            status_register = 0xff;
            error_register = 0xff;
            return FALSE;
        }
    }

    // return status
    delay(10);
    status_register = in_val;
    error_register = inp(ata_base + 1);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    device_register = inp(ata_base + 6);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    chigh_register = inp(ata_base + 5);
    lbahigh07_register = chigh_register;
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    clow_register = inp(ata_base + 4);
    lbamid07_register = clow_register;
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    sector_register = inp(ata_base + 3);
    lbalow07_register = sector_register;
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    count_register = inp(ata_base + 2);
    count07_register = count_register;
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    outp(ata_ctrl, BIT7);          // set HOB bit
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    lbalow815_register = inp(ata_base + 3);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    lbamid815_register = inp(ata_base + 4);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    lbahigh815_register = inp(ata_base + 5);
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;
    outp(ata_ctrl, 0);             // clear HOB bit
    _asm nop;
    _asm nop;
    _asm nop;
    _asm nop;

    if (error_register) return FALSE;
    return TRUE;
}
