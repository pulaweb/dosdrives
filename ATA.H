
#define TIMEOUT 20000
#define SMART_CYL_LOW   0x4F
#define SMART_CYL_HI    0xC2 
#define SMART_CMD       0xB0 

// function prototypes
BOOL ata_send_command (BYTE command, BYTE features, BYTE count, BYTE direction, DISKDRIVE *sdrive, BYTE *buffer);
BOOL ata_send_command_extended (BYTE command, BYTE features, BYTE count, BYTE sector, BYTE clow, BYTE chigh, BYTE device, BYTE direction, DISKDRIVE *sdrive, BYTE *buffer);
BOOL ata_send_command_extended_48bit (BYTE command, BYTE features, BYTE count, BYTE sector, BYTE clow, BYTE chigh, BYTE device, BYTE featuresh, BYTE counth, BYTE sectorh, BYTE clowh, BYTE chighh, BYTE direction, DISKDRIVE *sdrive, BYTE *buffer);
