#include "general.h"
#include "smbios.h"
#include "display.h"

#define COLOR_NORMAL 0x07
#define COLOR_TITLE  0x0B
#define COLOR_DIM    0x08
#define COLOR_SUCCESS 0x0A

static void SetColor(UINTN attr)
{
    gST->ConOut->SetAttribute(gST->ConOut, attr);
}

static void PrintSeparator(void)
{
    SetColor(COLOR_DIM);
    Print(L"------------------------------------------------------------\n");
    SetColor(COLOR_NORMAL);
}

static void PrintHeader(const CHAR16* title)
{
    Print(L"\n");
    SetColor(COLOR_TITLE);
    Print(L"  %s\n", title);
    SetColor(COLOR_NORMAL);
    PrintSeparator();
}

static BOOLEAN HasBytes(SMBIOS_STRUCTURE_POINTER table, UINTN offset, UINTN size)
{
    return table.Raw && table.Hdr->Length >= offset + size;
}

static UINT8 ReadU8(SMBIOS_STRUCTURE_POINTER table, UINTN offset)
{
    if (!HasBytes(table, offset, sizeof(UINT8)))
        return 0;

    return table.Raw[offset];
}

static UINT16 ReadU16(SMBIOS_STRUCTURE_POINTER table, UINTN offset)
{
    if (!HasBytes(table, offset, sizeof(UINT16)))
        return 0;

    return (UINT16)table.Raw[offset] | ((UINT16)table.Raw[offset + 1] << 8);
}

static UINT32 ReadU32(SMBIOS_STRUCTURE_POINTER table, UINTN offset)
{
    if (!HasBytes(table, offset, sizeof(UINT32)))
        return 0;

    return (UINT32)table.Raw[offset]
         | ((UINT32)table.Raw[offset + 1] << 8)
         | ((UINT32)table.Raw[offset + 2] << 16)
         | ((UINT32)table.Raw[offset + 3] << 24);
}

static UINT64 ReadU64(SMBIOS_STRUCTURE_POINTER table, UINTN offset)
{
    UINT64 value = 0;

    if (!HasBytes(table, offset, sizeof(UINT64)))
        return 0;

    for (UINTN i = 0; i < sizeof(UINT64); i++)
        value |= ((UINT64)table.Raw[offset + i]) << (8 * i);

    return value;
}

static UINT16 ReadHandle(SMBIOS_STRUCTURE_POINTER table)
{
    return ReadU16(table, 2);
}

static void DisplayStringField(SMBIOS_STRUCTURE_POINTER table, const CHAR16* label, UINT8 index);

static void DisplayStringOffset(SMBIOS_STRUCTURE_POINTER table, const CHAR16* label, UINTN offset)
{
    DisplayStringField(table, label, ReadU8(table, offset));
}

static const CHAR16* ProcessorTypeName(UINT8 type)
{
    switch (type)
    {
    case 3:
        return L"CPU";
    case 4:
        return L"Math Processor";
    case 5:
        return L"DSP";
    case 6:
        return L"Video Processor";
    default:
        return L"Unknown";
    }
}

static const CHAR16* MemoryFormFactorName(UINT8 form)
{
    switch (form)
    {
    case 0x03:
        return L"SIMM";
    case 0x08:
        return L"DIMM";
    case 0x09:
        return L"TSOP";
    case 0x0C:
        return L"SODIMM";
    case 0x0D:
        return L"SRIMM";
    case 0x0F:
        return L"FB-DIMM";
    default:
        return L"Unknown";
    }
}

static const CHAR16* MemoryTypeName(UINT8 type)
{
    switch (type)
    {
    case 0x03:
        return L"DRAM";
    case 0x13:
        return L"DDR2";
    case 0x18:
        return L"DDR3";
    case 0x1A:
        return L"DDR4";
    case 0x22:
        return L"DDR5";
    case 0x1B:
        return L"LPDDR";
    case 0x1C:
        return L"LPDDR2";
    case 0x1D:
        return L"LPDDR3";
    case 0x1E:
        return L"LPDDR4";
    case 0x23:
        return L"LPDDR5";
    default:
        return L"Unknown";
    }
}

static const CHAR16* BootStatusName(UINT8 status)
{
    switch (status)
    {
    case 0:
        return L"No errors";
    case 1:
        return L"No bootable media";
    case 2:
        return L"Normal OS failed";
    case 3:
        return L"Firmware detected failure";
    case 4:
        return L"OS detected failure";
    case 5:
        return L"User requested boot";
    case 6:
        return L"Security violation";
    case 7:
        return L"Image failed";
    case 8:
        return L"Watchdog expired";
    default:
        return L"Unknown";
    }
}

static const CHAR16* ConnectorTypeName(UINT8 type)
{
    switch (type)
    {
    case 0x03:
        return L"DB-25 male";
    case 0x04:
        return L"DB-25 female";
    case 0x05:
        return L"DB-15 male";
    case 0x06:
        return L"DB-15 female";
    case 0x07:
        return L"DB-9 male";
    case 0x08:
        return L"DB-9 female";
    case 0x0F:
        return L"PS/2";
    case 0x10:
        return L"USB";
    case 0x11:
        return L"FireWire";
    case 0x12:
        return L"PC-98";
    case 0x13:
        return L"PC-98Hireso";
    case 0x14:
        return L"PC-H98";
    case 0x15:
        return L"PC-98Note";
    case 0x16:
        return L"PC-98Full";
    case 0x17:
        return L"Other";
    case 0x18:
        return L"RJ-45";
    case 0x1F:
        return L"SATA";
    default:
        return L"Unknown";
    }
}

static const CHAR16* PortTypeName(UINT8 type)
{
    switch (type)
    {
    case 0x03:
        return L"Parallel";
    case 0x04:
        return L"Serial";
    case 0x05:
        return L"Keyboard";
    case 0x06:
        return L"Mouse";
    case 0x07:
        return L"SSA SCSI";
    case 0x08:
        return L"USB";
    case 0x09:
        return L"FireWire";
    case 0x0A:
        return L"PCMCIA";
    case 0x0B:
        return L"Cardbus";
    case 0x0C:
        return L"Access Bus";
    case 0x0D:
        return L"SCSI II";
    case 0x0E:
        return L"SCSI Wide";
    case 0x0F:
        return L"PC-98";
    case 0x10:
        return L"Video";
    case 0x11:
        return L"Audio";
    case 0x12:
        return L"Modem";
    case 0x13:
        return L"Network";
    case 0x20:
        return L"SATA";
    default:
        return L"Unknown";
    }
}

static const CHAR16* OnboardDeviceTypeName(UINT8 type)
{
    switch (type & 0x7F)
    {
    case 0x03:
        return L"Video";
    case 0x04:
        return L"SCSI";
    case 0x05:
        return L"Ethernet";
    case 0x06:
        return L"Token Ring";
    case 0x07:
        return L"Sound";
    case 0x08:
        return L"PATA";
    case 0x09:
        return L"SATA";
    case 0x0A:
        return L"SAS";
    default:
        return L"Unknown";
    }
}

static const CHAR16* IpmiInterfaceName(UINT8 type)
{
    switch (type)
    {
    case 0x00:
        return L"Unknown";
    case 0x01:
        return L"KCS";
    case 0x02:
        return L"SMIC";
    case 0x03:
        return L"BT";
    case 0x04:
        return L"SSIF";
    default:
        return L"OEM";
    }
}

static void DisplayUuidField(const SMBIOS_STRUCTURE_POINTER table)
{
    if (table.Hdr->Length < 25)
    {
        Print(L"  UUID:              Not available\n");
        return;
    }

    const UINT8* uuid = table.Raw + 8;

    Print(L"  UUID:              %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
          uuid[0], uuid[1], uuid[2], uuid[3],
          uuid[4], uuid[5], uuid[6], uuid[7],
          uuid[8], uuid[9], uuid[10], uuid[11],
          uuid[12], uuid[13], uuid[14], uuid[15]);
}

static void DisplayStringField(SMBIOS_STRUCTURE_POINTER table, const CHAR16* label, UINT8 index)
{
    const char* value = GetStringAtIndex(table, index);
    if (value)
    {
        Print(L"  %s%a\n", label, value);
    }
    else
    {
        Print(L"  %s[Not set]\n", label);
    }
}

void DisplayType0(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_BIOS_INFORMATION)
        return;

    PrintHeader(L"BIOS Information (Type 0)");

    DisplayStringField(table, L"Vendor:            ", table.Type0->Vendor);
    DisplayStringField(table, L"Version:           ", table.Type0->BiosVersion);
    DisplayStringField(table, L"Release Date:      ", table.Type0->BiosReleaseDate);
}

void DisplayType1(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_INFORMATION)
        return;

    PrintHeader(L"System Information (Type 1)");

    DisplayStringField(table, L"Manufacturer:      ", table.Type1->Manufacturer);
    DisplayStringField(table, L"Product Name:      ", table.Type1->ProductName);
    DisplayStringField(table, L"Version:           ", table.Type1->Version);
    DisplayStringField(table, L"Serial Number:     ", table.Type1->SerialNumber);
    DisplayUuidField(table);
}

void DisplayType2(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_BASEBOARD_INFORMATION)
        return;

    PrintHeader(L"Baseboard Information (Type 2)");

    DisplayStringField(table, L"Manufacturer:      ", table.Type2->Manufacturer);
    DisplayStringField(table, L"Product Name:      ", table.Type2->ProductName);
    DisplayStringField(table, L"Version:           ", table.Type2->Version);
    DisplayStringField(table, L"Serial Number:     ", table.Type2->SerialNumber);
}

void DisplayType3(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_ENCLOSURE)
        return;

    PrintHeader(L"System Enclosure (Type 3)");

    DisplayStringField(table, L"Manufacturer:      ", table.Type3->Manufacturer);
    DisplayStringField(table, L"Version:           ", table.Type3->Version);
    DisplayStringField(table, L"Serial Number:     ", table.Type3->SerialNumber);
    DisplayStringField(table, L"Asset Tag:         ", table.Type3->AssetTag);
}

void DisplayType4(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_PROCESSOR_INFORMATION)
        return;

    PrintHeader(L"Processor Information (Type 4)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Socket:            ", 0x04);
    Print(L"  Processor Type:    %u (%s)\n", ReadU8(table, 0x05), ProcessorTypeName(ReadU8(table, 0x05)));
    Print(L"  Processor Family:  0x%02x\n", ReadU8(table, 0x06));
    DisplayStringOffset(table, L"Manufacturer:      ", 0x07);

    if (HasBytes(table, 0x08, 8))
    {
        Print(L"  Processor ID:      %02x%02x%02x%02x%02x%02x%02x%02x\n",
              ReadU8(table, 0x08), ReadU8(table, 0x09), ReadU8(table, 0x0A), ReadU8(table, 0x0B),
              ReadU8(table, 0x0C), ReadU8(table, 0x0D), ReadU8(table, 0x0E), ReadU8(table, 0x0F));
    }

    DisplayStringOffset(table, L"Version:           ", 0x10);
    Print(L"  External Clock:    %u MHz\n", ReadU16(table, 0x12));
    Print(L"  Max Speed:         %u MHz\n", ReadU16(table, 0x14));
    Print(L"  Current Speed:     %u MHz\n", ReadU16(table, 0x16));
    Print(L"  Status:            0x%02x\n", ReadU8(table, 0x18));
    Print(L"  Upgrade:           0x%02x\n", ReadU8(table, 0x19));
    DisplayStringOffset(table, L"Serial Number:     ", 0x20);
    DisplayStringOffset(table, L"Asset Tag:         ", 0x21);
    DisplayStringOffset(table, L"Part Number:       ", 0x22);

    if (HasBytes(table, 0x23, 3))
    {
        Print(L"  Cores:             %u physical, %u enabled\n", ReadU8(table, 0x23), ReadU8(table, 0x24));
        Print(L"  Threads:           %u\n", ReadU8(table, 0x25));
    }

    if (HasBytes(table, 0x26, 2))
        Print(L"  Characteristics:   0x%04x\n", ReadU16(table, 0x26));
}

void DisplayType7(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_CACHE_INFORMATION)
        return;

    PrintHeader(L"Cache Information (Type 7)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Socket:            ", 0x04);
    Print(L"  Configuration:     0x%04x\n", ReadU16(table, 0x05));
    Print(L"  Max Size Raw:      0x%04x\n", ReadU16(table, 0x07));
    Print(L"  Installed Raw:     0x%04x\n", ReadU16(table, 0x09));
    Print(L"  Speed:             %u ns\n", ReadU8(table, 0x0F));
    Print(L"  Error Correction:  0x%02x\n", ReadU8(table, 0x10));
    Print(L"  Cache Type:        0x%02x\n", ReadU8(table, 0x11));
    Print(L"  Associativity:     0x%02x\n", ReadU8(table, 0x12));
}

void DisplayType8(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION)
        return;

    PrintHeader(L"Port Connector (Type 8)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Internal Ref:      ", 0x04);
    Print(L"  Internal Type:     0x%02x (%s)\n", ReadU8(table, 0x05), ConnectorTypeName(ReadU8(table, 0x05)));
    DisplayStringOffset(table, L"External Ref:      ", 0x06);
    Print(L"  External Type:     0x%02x (%s)\n", ReadU8(table, 0x07), ConnectorTypeName(ReadU8(table, 0x07)));
    Print(L"  Port Type:         0x%02x (%s)\n", ReadU8(table, 0x08), PortTypeName(ReadU8(table, 0x08)));
}

void DisplayType9(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_SLOTS)
        return;

    PrintHeader(L"System Slot (Type 9)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Designation:       ", 0x04);
    Print(L"  Slot Type:         0x%02x\n", ReadU8(table, 0x05));
    Print(L"  Bus Width:         0x%02x\n", ReadU8(table, 0x06));
    Print(L"  Current Usage:     0x%02x\n", ReadU8(table, 0x07));
    Print(L"  Slot Length:       0x%02x\n", ReadU8(table, 0x08));
    Print(L"  Slot ID:           0x%04x\n", ReadU16(table, 0x09));
    Print(L"  Characteristics:   0x%04x\n", ReadU16(table, 0x0B));
}

void DisplayType10(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION)
        return;

    PrintHeader(L"Onboard Devices (Type 10)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    for (UINTN offset = 0x04; offset + 1 < table.Hdr->Length; offset += 2)
    {
        UINT8 type = ReadU8(table, offset);
        UINT8 index = ReadU8(table, offset + 1);
        Print(L"  Device:            0x%02x (%s), enabled %u\n",
              type, OnboardDeviceTypeName(type), (type & 0x80) ? 1 : 0);
        DisplayStringField(table, L"Reference:         ", index);
    }
}

void DisplayType11(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_OEM_STRINGS)
        return;

    PrintHeader(L"OEM Strings (Type 11)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    UINT8 count = ReadU8(table, 0x04);
    Print(L"  Count:             %u\n", count);
    for (UINTN i = 1; i <= count; i++)
        DisplayStringField(table, L"String:            ", (UINT8)i);
}

void DisplayType12(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_CONFIGURATION_OPTIONS)
        return;

    PrintHeader(L"System Configuration Options (Type 12)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    UINT8 count = ReadU8(table, 0x04);
    Print(L"  Count:             %u\n", count);
    for (UINTN i = 1; i <= count; i++)
        DisplayStringField(table, L"Option:            ", (UINT8)i);
}

void DisplayType13(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION)
        return;

    PrintHeader(L"BIOS Language Information (Type 13)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Installable Count: %u\n", ReadU8(table, 0x04));
    Print(L"  Flags:             0x%02x\n", ReadU8(table, 0x05));
    if (HasBytes(table, 0x15, 1))
        Print(L"  Current Language:  %u\n", ReadU8(table, 0x15));
}

void DisplayType15(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_EVENT_LOG)
        return;

    PrintHeader(L"System Event Log (Type 15)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Log Area Length:   %u bytes\n", ReadU16(table, 0x04));
    Print(L"  Header Start:      0x%04x\n", ReadU16(table, 0x06));
    Print(L"  Data Start:        0x%04x\n", ReadU16(table, 0x08));
    Print(L"  Access Method:     0x%02x\n", ReadU8(table, 0x0A));
    Print(L"  Status:            0x%02x\n", ReadU8(table, 0x0B));
    Print(L"  Change Token:      0x%08x\n", ReadU32(table, 0x0C));
    Print(L"  Header Format:     0x%02x\n", ReadU8(table, 0x10));
    Print(L"  Descriptors:       %u x %u bytes\n", ReadU8(table, 0x11), ReadU8(table, 0x12));
}

void DisplayType16(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY)
        return;

    PrintHeader(L"Physical Memory Array (Type 16)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Location:          0x%02x\n", ReadU8(table, 0x04));
    Print(L"  Use:               0x%02x\n", ReadU8(table, 0x05));
    Print(L"  Error Correction:  0x%02x\n", ReadU8(table, 0x06));

    UINT32 maxKb = ReadU32(table, 0x07);
    if (maxKb == 0x80000000 && HasBytes(table, 0x0F, 8))
        Print(L"  Max Capacity:      %d MiB (extended)\n", (UINTN)(ReadU64(table, 0x0F) / 1024));
    else if (maxKb)
        Print(L"  Max Capacity:      %d MiB\n", (UINTN)(maxKb / 1024));
    else
        Print(L"  Max Capacity:      Unknown\n");

    Print(L"  Error Handle:      0x%04x\n", ReadU16(table, 0x0B));
    Print(L"  Device Count:      %u\n", ReadU16(table, 0x0D));
}

static void DisplayMemoryDeviceSize(SMBIOS_STRUCTURE_POINTER table)
{
    UINT16 size = ReadU16(table, 0x0C);

    if (size == 0)
    {
        Print(L"  Size:              No module installed\n");
    }
    else if (size == 0xFFFF)
    {
        Print(L"  Size:              Unknown\n");
    }
    else if (size == 0x7FFF && HasBytes(table, 0x1C, 4))
    {
        Print(L"  Size:              %d MiB\n", (UINTN)ReadU32(table, 0x1C));
    }
    else if (size & 0x8000)
    {
        Print(L"  Size:              %u KiB\n", size & 0x7FFF);
    }
    else
    {
        Print(L"  Size:              %u MiB\n", size);
    }
}

void DisplayType17(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_MEMORY_DEVICE)
        return;

    PrintHeader(L"Memory Device (Type 17)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Array Handle:      0x%04x\n", ReadU16(table, 0x04));
    Print(L"  Error Handle:      0x%04x\n", ReadU16(table, 0x06));
    Print(L"  Total Width:       %u bits\n", ReadU16(table, 0x08));
    Print(L"  Data Width:        %u bits\n", ReadU16(table, 0x0A));
    DisplayMemoryDeviceSize(table);
    Print(L"  Form Factor:       0x%02x (%s)\n", ReadU8(table, 0x0E), MemoryFormFactorName(ReadU8(table, 0x0E)));
    DisplayStringOffset(table, L"Locator:           ", 0x10);
    DisplayStringOffset(table, L"Bank Locator:      ", 0x11);
    Print(L"  Memory Type:       0x%02x (%s)\n", ReadU8(table, 0x12), MemoryTypeName(ReadU8(table, 0x12)));
    Print(L"  Type Detail:       0x%04x\n", ReadU16(table, 0x13));
    Print(L"  Speed:             %u MT/s\n", ReadU16(table, 0x15));
    DisplayStringOffset(table, L"Manufacturer:      ", 0x17);
    DisplayStringOffset(table, L"Serial Number:     ", 0x18);
    DisplayStringOffset(table, L"Asset Tag:         ", 0x19);
    DisplayStringOffset(table, L"Part Number:       ", 0x1A);
    Print(L"  Attributes:        0x%02x\n", ReadU8(table, 0x1B));

    if (HasBytes(table, 0x20, 2))
        Print(L"  Configured Speed:  %u MT/s\n", ReadU16(table, 0x20));
}

void DisplayType19(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS)
        return;

    PrintHeader(L"Memory Array Mapped Address (Type 19)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    UINT32 startKb = ReadU32(table, 0x04);
    UINT32 endKb = ReadU32(table, 0x08);
    if (startKb == 0xFFFFFFFF && endKb == 0xFFFFFFFF && HasBytes(table, 0x0F, 16))
    {
        Print(L"  Start Address:     0x%08x\n", (UINTN)ReadU64(table, 0x0F));
        Print(L"  End Address:       0x%08x\n", (UINTN)ReadU64(table, 0x17));
    }
    else
    {
        Print(L"  Start Address:     %u MiB\n", startKb / 1024);
        Print(L"  End Address:       %u MiB\n", endKb / 1024);
    }
    Print(L"  Array Handle:      0x%04x\n", ReadU16(table, 0x0C));
    Print(L"  Partition Width:   %u\n", ReadU8(table, 0x0E));
}

void DisplayType20(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS)
        return;

    PrintHeader(L"Memory Device Mapped Address (Type 20)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    UINT32 startKb = ReadU32(table, 0x04);
    UINT32 endKb = ReadU32(table, 0x08);
    if (startKb == 0xFFFFFFFF && endKb == 0xFFFFFFFF && HasBytes(table, 0x13, 16))
    {
        Print(L"  Start Address:     0x%08x\n", (UINTN)ReadU64(table, 0x13));
        Print(L"  End Address:       0x%08x\n", (UINTN)ReadU64(table, 0x1B));
    }
    else
    {
        Print(L"  Start Address:     %u MiB\n", startKb / 1024);
        Print(L"  End Address:       %u MiB\n", endKb / 1024);
    }
    Print(L"  Device Handle:     0x%04x\n", ReadU16(table, 0x0C));
    Print(L"  Array Map Handle:  0x%04x\n", ReadU16(table, 0x0E));
    Print(L"  Row/Interleave:    %u / %u / %u\n",
          ReadU8(table, 0x10), ReadU8(table, 0x11), ReadU8(table, 0x12));
}

void DisplayType22(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_PORTABLE_BATTERY)
        return;

    PrintHeader(L"Portable Battery (Type 22)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Location:          ", 0x04);
    DisplayStringOffset(table, L"Manufacturer:      ", 0x05);
    DisplayStringOffset(table, L"Manufacture Date:  ", 0x06);
    DisplayStringOffset(table, L"Serial Number:     ", 0x07);
    DisplayStringOffset(table, L"Device Name:       ", 0x08);
    Print(L"  Chemistry:         0x%02x\n", ReadU8(table, 0x09));
    Print(L"  Design Capacity:   %u mWh\n", ReadU16(table, 0x0A));
    Print(L"  Design Voltage:    %u mV\n", ReadU16(table, 0x0C));
    DisplayStringOffset(table, L"SBDS Version:      ", 0x0E);
    Print(L"  Max Error:         %u%%\n", ReadU8(table, 0x0F));
}

void DisplayType23(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_RESET)
        return;

    PrintHeader(L"System Reset (Type 23)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Capabilities:      0x%02x\n", ReadU8(table, 0x04));
    Print(L"  Reset Count:       %u\n", ReadU16(table, 0x05));
    Print(L"  Reset Limit:       %u\n", ReadU16(table, 0x07));
    Print(L"  Timer Interval:    %u minutes\n", ReadU16(table, 0x09));
    Print(L"  Timeout:           %u minutes\n", ReadU16(table, 0x0B));
}

void DisplayType24(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_HARDWARE_SECURITY)
        return;

    PrintHeader(L"Hardware Security (Type 24)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Security Settings: 0x%02x\n", ReadU8(table, 0x04));
}

void DisplayType30(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_OUT_OF_BAND_REMOTE_ACCESS)
        return;

    PrintHeader(L"Out-of-Band Remote Access (Type 30)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Manufacturer:      ", 0x04);
    Print(L"  Connections:       0x%02x\n", ReadU8(table, 0x05));
}

void DisplayType32(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION)
        return;

    PrintHeader(L"System Boot Information (Type 32)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Boot Status:       0x%02x (%s)\n", ReadU8(table, 0x0A), BootStatusName(ReadU8(table, 0x0A)));
}

void DisplayType38(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_IPMI_DEVICE_INFORMATION)
        return;

    PrintHeader(L"IPMI Device Information (Type 38)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Interface Type:    0x%02x (%s)\n", ReadU8(table, 0x04), IpmiInterfaceName(ReadU8(table, 0x04)));
    Print(L"  Spec Revision:     0x%02x\n", ReadU8(table, 0x05));
    Print(L"  I2C Address:       0x%02x\n", ReadU8(table, 0x06));
    Print(L"  NV Storage Addr:   0x%02x\n", ReadU8(table, 0x07));
    Print(L"  Base Address:      0x%08x\n", (UINTN)ReadU64(table, 0x08));
    Print(L"  Base Modifier:     0x%02x\n", ReadU8(table, 0x10));
    Print(L"  Interrupt Number:  %u\n", ReadU8(table, 0x11));
}

void DisplayType39(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_SYSTEM_POWER_SUPPLY)
        return;

    PrintHeader(L"System Power Supply (Type 39)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Power Unit Group:  %u\n", ReadU8(table, 0x04));
    DisplayStringOffset(table, L"Location:          ", 0x05);
    DisplayStringOffset(table, L"Device Name:       ", 0x06);
    DisplayStringOffset(table, L"Manufacturer:      ", 0x07);
    DisplayStringOffset(table, L"Serial Number:     ", 0x08);
    DisplayStringOffset(table, L"Asset Tag:         ", 0x09);
    DisplayStringOffset(table, L"Model Part Number: ", 0x0A);
    DisplayStringOffset(table, L"Revision:          ", 0x0B);
    Print(L"  Max Power:         %u W\n", ReadU16(table, 0x0C));
    Print(L"  Characteristics:   0x%04x\n", ReadU16(table, 0x0E));
    Print(L"  Input Probe:       0x%04x\n", ReadU16(table, 0x10));
    Print(L"  Cooling Device:    0x%04x\n", ReadU16(table, 0x12));
    Print(L"  Input Current:     0x%04x\n", ReadU16(table, 0x14));
}

void DisplayType41(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION)
        return;

    PrintHeader(L"Onboard Device Extended (Type 41)");

    UINT8 type = ReadU8(table, 0x05);
    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    DisplayStringOffset(table, L"Reference:         ", 0x04);
    Print(L"  Device Type:       0x%02x (%s)\n", type, OnboardDeviceTypeName(type));
    Print(L"  Enabled:           %u\n", (type & 0x80) ? 1 : 0);
    Print(L"  Instance:          %u\n", ReadU8(table, 0x06));
    Print(L"  Segment Group:     %u\n", ReadU16(table, 0x07));
    Print(L"  Bus:               %u\n", ReadU8(table, 0x09));
    Print(L"  Device/Function:   0x%02x\n", ReadU8(table, 0x0A));
}

void DisplayType42(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE)
        return;

    PrintHeader(L"Management Controller Host Interface (Type 42)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Interface Type:    0x%02x\n", ReadU8(table, 0x04));
    Print(L"  Interface Length:  %u bytes\n", ReadU8(table, 0x05));
    if (HasBytes(table, 0x06, 4))
        Print(L"  Interface Data:    %02x %02x %02x %02x\n",
              ReadU8(table, 0x06), ReadU8(table, 0x07), ReadU8(table, 0x08), ReadU8(table, 0x09));
}

void DisplayType43(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_TPM_DEVICE)
        return;

    PrintHeader(L"TPM Device (Type 43)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    if (HasBytes(table, 0x04, 4))
    {
        Print(L"  Vendor ID:         %c%c%c%c\n",
              (CHAR16)ReadU8(table, 0x04), (CHAR16)ReadU8(table, 0x05),
              (CHAR16)ReadU8(table, 0x06), (CHAR16)ReadU8(table, 0x07));
    }
    Print(L"  Spec Version:      %u.%u\n", ReadU8(table, 0x08), ReadU8(table, 0x09));
    Print(L"  Firmware Version:  0x%08x\n", ReadU32(table, 0x0A));
    Print(L"  Description Index: %u\n", ReadU8(table, 0x12));
    DisplayStringOffset(table, L"Description:       ", 0x12);
    Print(L"  Characteristics:   0x%08x\n", ReadU32(table, 0x13));
}

void DisplayType44(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Type != SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION)
        return;

    PrintHeader(L"Processor Additional Information (Type 44)");

    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Referenced Handle: 0x%04x\n", ReadU16(table, 0x04));
    Print(L"  Specific Block:    0x%02x\n", ReadU8(table, 0x06));
    Print(L"  Header Length:     %u bytes\n", table.Hdr->Length);
}

static void DisplayGenericTable(SMBIOS_STRUCTURE_POINTER table)
{
    PrintHeader(L"SMBIOS Table");
    Print(L"  Type:              %u\n", table.Hdr->Type);
    Print(L"  Handle:            0x%04x\n", ReadHandle(table));
    Print(L"  Header Length:     %u bytes\n", table.Hdr->Length);
    Print(L"  Full Length:       %u bytes\n", TableLength(table));
}

static UINTN sPgLine;
static UINTN sPgRows;
static BOOLEAN sPgExit;

static void sPgInit(void)
{
    UINTN cols = 80;
    sPgRows = 25;
    if (gST->ConOut && gST->ConOut->Mode)
        gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &cols, &sPgRows);
    if (sPgRows < 8) sPgRows = 25;
    sPgRows -= 2;
    sPgLine = 0;
    sPgExit = FALSE;
}

static BOOLEAN sPgCheck(UINTN n)
{
    if (sPgExit) return FALSE;
    sPgLine += n;
    if (sPgLine >= sPgRows) {
        SetColor(COLOR_DIM);
        Print(L"  --- [any key: continue, Q: quit] ---\n");
        SetColor(COLOR_NORMAL);
        UINTN idx;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);
        EFI_INPUT_KEY key;
        gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
        if (key.UnicodeChar == L'q' || key.UnicodeChar == L'Q') {
            sPgExit = TRUE;
            return FALSE;
        }
        sPgLine = 0;
    }
    return TRUE;
}

static UINTN sGetCols(void)
{
    UINTN c = 80, r;
    if (gST->ConOut && gST->ConOut->Mode)
        gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &c, &r);
    return c;
}

static void sCenter(const CHAR16 *s)
{
    UINTN cols = sGetCols();
    UINTN len = 0;
    while (s[len]) len++;
    UINTN pad = (len < cols) ? (cols - len) / 2 : 0;
    for (UINTN i = 0; i < pad; i++) Print(L" ");
    Print(s);
}

void DisplayAllTables(const SMBIOS_STRUCTURE_TABLE* entry)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    sPgInit();

    if (!entry || !entry->TableAddress)
    {
        Print(L"\n[FAIL] Invalid SMBIOS entry\n");
        return;
    }

    Print(L"\n");
    SetColor(COLOR_TITLE);
    sCenter(L"Rainbow Dragon - SMBIOS Deep Report");
    Print(L"\n");
    SetColor(COLOR_NORMAL);
    PrintSeparator();
    Print(L"  SMBIOS v%u.%u  -  Entry Point: 0x%08x\n",
          entry->MajorVersion, entry->MinorVersion, (UINTN)entry);
    Print(L"  Table Address: 0x%08x\n", (UINTN)entry->TableAddress);
    PrintSeparator();
    sPgCheck(5);

    SMBIOS_STRUCTURE_POINTER table;
    table.Raw = (UINT8*)((UINTN)entry->TableAddress);
    SmbiosSetActiveTableBounds(entry);

    if (!table.Raw)
    {
        Print(L"[FAIL] Table address is null\n");
        return;
    }

    UINTN tableCount = 0;
    UINTN remaining = SmbiosBytesRemaining(entry, table);
    while (remaining >= 4 && table.Hdr->Type != SMBIOS_TYPE_END_OF_TABLE && tableCount < 512)
    {
        switch (table.Hdr->Type)
        {
        case SMBIOS_TYPE_BIOS_INFORMATION:
            DisplayType0(table);
            break;
        case SMBIOS_TYPE_SYSTEM_INFORMATION:
            DisplayType1(table);
            break;
        case SMBIOS_TYPE_BASEBOARD_INFORMATION:
            DisplayType2(table);
            break;
        case SMBIOS_TYPE_SYSTEM_ENCLOSURE:
            DisplayType3(table);
            break;
        case SMBIOS_TYPE_PROCESSOR_INFORMATION:
            DisplayType4(table);
            break;
        case SMBIOS_TYPE_CACHE_INFORMATION:
            DisplayType7(table);
            break;
        case SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION:
            DisplayType8(table);
            break;
        case SMBIOS_TYPE_SYSTEM_SLOTS:
            DisplayType9(table);
            break;
        case SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION:
            DisplayType10(table);
            break;
        case SMBIOS_TYPE_OEM_STRINGS:
            DisplayType11(table);
            break;
        case SMBIOS_TYPE_SYSTEM_CONFIGURATION_OPTIONS:
            DisplayType12(table);
            break;
        case SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION:
            DisplayType13(table);
            break;
        case SMBIOS_TYPE_SYSTEM_EVENT_LOG:
            DisplayType15(table);
            break;
        case SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY:
            DisplayType16(table);
            break;
        case SMBIOS_TYPE_MEMORY_DEVICE:
            DisplayType17(table);
            break;
        case SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS:
            DisplayType19(table);
            break;
        case SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS:
            DisplayType20(table);
            break;
        case SMBIOS_TYPE_PORTABLE_BATTERY:
            DisplayType22(table);
            break;
        case SMBIOS_TYPE_SYSTEM_RESET:
            DisplayType23(table);
            break;
        case SMBIOS_TYPE_HARDWARE_SECURITY:
            DisplayType24(table);
            break;
        case SMBIOS_TYPE_OUT_OF_BAND_REMOTE_ACCESS:
            DisplayType30(table);
            break;
        case SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION:
            DisplayType32(table);
            break;
        case SMBIOS_TYPE_IPMI_DEVICE_INFORMATION:
            DisplayType38(table);
            break;
        case SMBIOS_TYPE_SYSTEM_POWER_SUPPLY:
            DisplayType39(table);
            break;
        case SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION:
            DisplayType41(table);
            break;
        case SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE:
            DisplayType42(table);
            break;
        case SMBIOS_TYPE_TPM_DEVICE:
            DisplayType43(table);
            break;
        case SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION:
            DisplayType44(table);
            break;
        default:
            DisplayGenericTable(table);
            break;
        }

        UINTN length = TableLength(table);
        if (length == 0 || length < table.Hdr->Length || length > remaining)
        {
            Print(L"\n[FAIL] Malformed SMBIOS table length, stopping report\n");
            break;
        }

        table.Raw += length;
        remaining = SmbiosBytesRemaining(entry, table);
        tableCount++;
        if (!sPgCheck(1)) break;
    }

    if (!sPgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

void WaitForAnyKey(void)
{
    Print(L"  Press any key to continue...");

    EFI_INPUT_KEY key;
    UINTN index;

    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);
    gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
}
