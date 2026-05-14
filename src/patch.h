#ifndef PATCH_H
#define PATCH_H

#include "general.h"

void PatchType0(SMBIOS_STRUCTURE_TABLE* entry);
void PatchType1(SMBIOS_STRUCTURE_TABLE* entry);
void PatchType2(SMBIOS_STRUCTURE_TABLE* entry);
void PatchType3(SMBIOS_STRUCTURE_TABLE* entry);
void PatchType4(SMBIOS_STRUCTURE_TABLE* entry);
void PatchType17(SMBIOS_STRUCTURE_TABLE* entry);
void PatchAll(SMBIOS_STRUCTURE_TABLE* entry);

/* Persistent HWID backup — saved at first spoof, restored on demand */
BOOLEAN HasSmbiosBackup(void);
UINT32  SmbiosBackupFieldCount(void);
void    SaveSmbiosDefaults(SMBIOS_STRUCTURE_TABLE* entry);
void    RestoreSmbiosDefaults(SMBIOS_STRUCTURE_TABLE* entry);
void    ShowSmbiosBackupDiff(SMBIOS_STRUCTURE_TABLE* entry);

/* System UUID raw-field backup (Type 1 offset 0x08, 16 bytes) */
BOOLEAN HasSystemUuidBackup(void);
void    SaveSystemUuidBackup(SMBIOS_STRUCTURE_TABLE* entry);
void    RestoreSystemUuidBackup(SMBIOS_STRUCTURE_TABLE* entry);
void    ShowSystemUuidDiff(SMBIOS_STRUCTURE_TABLE* entry);

/* CPU speed raw-field backup (Type 4 offsets 0x14/0x16) */
BOOLEAN HasCpuSpeedBackup(void);
void    SaveCpuSpeedBackup(SMBIOS_STRUCTURE_TABLE* entry);
void    RestoreCpuSpeedBackup(SMBIOS_STRUCTURE_TABLE* entry);

#endif
