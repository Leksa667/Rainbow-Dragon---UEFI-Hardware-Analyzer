#ifndef DISPLAY_H
#define DISPLAY_H

#include "general.h"
#include "smbios.h"

void DisplayType0(SMBIOS_STRUCTURE_POINTER table);
void DisplayType1(SMBIOS_STRUCTURE_POINTER table);
void DisplayType2(SMBIOS_STRUCTURE_POINTER table);
void DisplayType3(SMBIOS_STRUCTURE_POINTER table);
void DisplayType4(SMBIOS_STRUCTURE_POINTER table);
void DisplayType7(SMBIOS_STRUCTURE_POINTER table);
void DisplayType8(SMBIOS_STRUCTURE_POINTER table);
void DisplayType9(SMBIOS_STRUCTURE_POINTER table);
void DisplayType10(SMBIOS_STRUCTURE_POINTER table);
void DisplayType11(SMBIOS_STRUCTURE_POINTER table);
void DisplayType12(SMBIOS_STRUCTURE_POINTER table);
void DisplayType13(SMBIOS_STRUCTURE_POINTER table);
void DisplayType15(SMBIOS_STRUCTURE_POINTER table);
void DisplayType16(SMBIOS_STRUCTURE_POINTER table);
void DisplayType17(SMBIOS_STRUCTURE_POINTER table);
void DisplayType19(SMBIOS_STRUCTURE_POINTER table);
void DisplayType20(SMBIOS_STRUCTURE_POINTER table);
void DisplayType22(SMBIOS_STRUCTURE_POINTER table);
void DisplayType23(SMBIOS_STRUCTURE_POINTER table);
void DisplayType24(SMBIOS_STRUCTURE_POINTER table);
void DisplayType30(SMBIOS_STRUCTURE_POINTER table);
void DisplayType32(SMBIOS_STRUCTURE_POINTER table);
void DisplayType38(SMBIOS_STRUCTURE_POINTER table);
void DisplayType39(SMBIOS_STRUCTURE_POINTER table);
void DisplayType41(SMBIOS_STRUCTURE_POINTER table);
void DisplayType42(SMBIOS_STRUCTURE_POINTER table);
void DisplayType43(SMBIOS_STRUCTURE_POINTER table);
void DisplayType44(SMBIOS_STRUCTURE_POINTER table);
void DisplayAllTables(const SMBIOS_STRUCTURE_TABLE* entry);
void WaitForAnyKey(void);

#endif
