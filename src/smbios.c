#include "general.h"
#include "smbios.h"

#define SMBIOS_SCAN_LIMIT 0x10000U

static const UINT8* g_SmbiosBoundStart = NULL;
static const UINT8* g_SmbiosBoundEnd = NULL;

UINTN SmbiosEntryTableLength(const SMBIOS_STRUCTURE_TABLE* entry)
{
    if (!entry || !entry->TableAddress)
        return 0;

    if (entry->TableLength)
        return entry->TableLength;

    if (entry->NumberOfSmbiosStructures && entry->MaxStructureSize)
    {
        UINTN estimate = (UINTN)entry->NumberOfSmbiosStructures * entry->MaxStructureSize;
        return estimate < SMBIOS_SCAN_LIMIT ? estimate : SMBIOS_SCAN_LIMIT;
    }

    return SMBIOS_SCAN_LIMIT;
}

void SmbiosSetActiveTableBounds(const SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN length = SmbiosEntryTableLength(entry);

    if (!entry || !entry->TableAddress || !length)
    {
        g_SmbiosBoundStart = NULL;
        g_SmbiosBoundEnd = NULL;
        return;
    }

    g_SmbiosBoundStart = (const UINT8*)((UINTN)entry->TableAddress);
    g_SmbiosBoundEnd = g_SmbiosBoundStart + length;
}

UINTN SmbiosBytesRemaining(const SMBIOS_STRUCTURE_TABLE* entry, SMBIOS_STRUCTURE_POINTER table)
{
    UINTN length = SmbiosEntryTableLength(entry);
    const UINT8* start;
    const UINT8* end;

    if (!entry || !entry->TableAddress || !table.Raw || !length)
        return 0;

    start = (const UINT8*)((UINTN)entry->TableAddress);
    end = start + length;

    if (table.Raw < start || table.Raw >= end)
        return 0;

    return (UINTN)(end - table.Raw);
}

static const UINT8* ActiveEndFor(const UINT8* raw)
{
    if (g_SmbiosBoundStart && g_SmbiosBoundEnd && raw >= g_SmbiosBoundStart && raw < g_SmbiosBoundEnd)
        return g_SmbiosBoundEnd;

    return raw + SMBIOS_SCAN_LIMIT;
}

UINT16 TableLength(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Length < 4)
        return 0;

    const UINT8* pointer = table.Raw + table.Hdr->Length;
    const UINT8* end = ActiveEndFor(table.Raw);

    if (pointer >= end)
        return 0;

    while (pointer + 1 < end)
    {
        if (pointer[0] == 0 && pointer[1] == 0)
        {
            UINTN length = (UINTN)(pointer - table.Raw + 2);
            return length <= 0xFFFF ? (UINT16)length : 0;
        }

        pointer++;
    }

    return 0;
}

SMBIOS_STRUCTURE_POINTER FindTableByType(const SMBIOS_STRUCTURE_TABLE* entry, UINT8 type, UINTN index)
{
    SMBIOS_STRUCTURE_POINTER smbiosTable;
    smbiosTable.Raw = NULL;

    if (!entry || !entry->TableAddress)
        return smbiosTable;

    SmbiosSetActiveTableBounds(entry);

    smbiosTable.Raw = (UINT8*)((UINTN)entry->TableAddress);
    UINTN typeIndex = 0;
    UINTN remaining = SmbiosBytesRemaining(entry, smbiosTable);

    while (remaining >= 4 && ((typeIndex != index) || (smbiosTable.Hdr->Type != type)))
    {
        if (smbiosTable.Hdr->Type == SMBIOS_TYPE_END_OF_TABLE)
        {
            smbiosTable.Raw = 0;
            return smbiosTable;
        }

        if (smbiosTable.Hdr->Type == type)
        {
            typeIndex++;
        }

        UINTN length = TableLength(smbiosTable);
        if (!length || length > remaining)
        {
            smbiosTable.Raw = NULL;
            return smbiosTable;
        }

        smbiosTable.Raw = (UINT8*)(smbiosTable.Raw + length);
        remaining = SmbiosBytesRemaining(entry, smbiosTable);
    }

    if (remaining < 4)
        smbiosTable.Raw = NULL;

    return smbiosTable;
}

UINTN StringLength(const char* text, UINTN maxLength)
{
    UINTN length = 0;
    const char* end;

    if (maxLength > 0)
    {
        for (length = 0; length < maxLength; length++)
        {
            if (text[length] == 0)
            {
                break;
            }
        }

        if (length == 0)
            return 0;

        end = &text[length - 1];

        while ((length != 0) && ((*end == ' ') || (*end == 0)))
        {
            end--;
            length--;
        }
    }
    else
    {
        end = text;
        while (*end)
        {
            end++;
            length++;
        }
    }

    return length;
}

static const char* AdvanceToNthString(const SMBIOS_STRUCTURE_POINTER table, UINT8 targetIndex, UINTN tableLength)
{
    UINT8 index = 1;
    const char* astr = (const char*)(table.Raw + table.Hdr->Length);
    const char* end = (const char*)(table.Raw + tableLength);

    if (astr >= end || *astr == 0)
        return NULL;

    while (index < targetIndex)
    {
        while (astr < end && *astr != 0)
            astr++;

        if (astr >= end)
            return NULL;

        astr++;

        if (astr >= end || *astr == 0)
        {
            return NULL;
        }

        index++;
    }

    return astr;
}

const char* GetStringAtIndex(SMBIOS_STRUCTURE_POINTER table, UINT8 index)
{
    if (!table.Raw || index == 0)
        return NULL;

    UINTN length = TableLength(table);
    if (!length)
        return NULL;

    return AdvanceToNthString(table, index, length);
}

void EditString(SMBIOS_STRUCTURE_POINTER table, SMBIOS_STRING* field, const char* buffer)
{
    if (!table.Raw || !buffer || !field)
        return;

    UINT8 index = 1;
    char* astr = (char*)(table.Raw + table.Hdr->Length);

    while (index != *field)
    {
        if (*astr)
        {
            index++;
        }

        while (*astr != 0)
            astr++;
        astr++;

        if (*astr == 0)
        {
            if (*field == 0)
            {
                astr[1] = 0;
            }

            *field = index;

            if (index == 1)
            {
                astr--;
            }
            break;
        }
    }

    UINTN astrLength = StringLength(astr, 0);
    UINTN bstrLength = StringLength(buffer, 256);

    if (bstrLength < astrLength)
    {
        Print(L"[FAIL] Input string too short (need %d, got %d)\n", astrLength, bstrLength);
        return;
    }

    CopyMem(astr, (VOID*)buffer, astrLength);
}
