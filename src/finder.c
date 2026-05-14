#include "general.h"
#include "finder.h"
#include "smbios.h"
#include "edk2/PiHob.h"
#include "hob.h"

EFI_GUID SmbiosTableGuid  = { 0xEB9D2D31, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } };
EFI_GUID Smbios3TableGuid = { 0xF2FD1544, 0x9794, 0x4A2C, { 0x99, 0x2E, 0xE5, 0xBB, 0xCF, 0x20, 0xE3, 0x94 } };

#define GET_GUID_HOB_DATA(GuidHob) ((VOID*)(((UINT8*)&((GuidHob)->Name)) + sizeof(EFI_GUID)))

void* FindByHob(void)
{
    EFI_PHYSICAL_ADDRESS* table;
    EFI_PEI_HOB_POINTERS guidHob;

    guidHob.Raw = GetFirstGuidHob(&SmbiosTableGuid);

    if (guidHob.Raw != NULL)
    {
        table = (EFI_PHYSICAL_ADDRESS*)GET_GUID_HOB_DATA(guidHob.Guid);
        if (table != NULL)
        {
            return (void*)(UINTN)*table;
        }
    }

    guidHob.Raw = GetFirstGuidHob(&Smbios3TableGuid);

    if (guidHob.Raw != NULL)
    {
        table = (EFI_PHYSICAL_ADDRESS*)GET_GUID_HOB_DATA(guidHob.Guid);
        if (table != NULL)
        {
            return (void*)(UINTN)*table;
        }
    }

    return NULL;
}

void* FindByConfig(void)
{
    void* table = NULL;

    EFI_STATUS status = LibGetSystemConfigurationTable(&SmbiosTableGuid, &table);
    if (!EFI_ERROR(status) && table != NULL)
        return table;

    status = LibGetSystemConfigurationTable(&Smbios3TableGuid, &table);
    if (!EFI_ERROR(status) && table != NULL)
        return table;

    return NULL;
}

SMBIOS_STRUCTURE_TABLE* FindEntry(void)
{
    void* address;

    address = FindByConfig();
    if (address)
        return (SMBIOS_STRUCTURE_TABLE*)address;

    address = FindByHob();
    if (address)
        return (SMBIOS_STRUCTURE_TABLE*)address;

    return NULL;
}
