#include "general.h"
#include "diagnostics.h"
#include "display.h"
#include "finder.h"
#include "smbios.h"
#include "patch.h"
#include "acpi_patch.h"

#define COLOR_NORMAL   0x07
#define COLOR_TITLE    0x0B
#define COLOR_OK       0x0A
#define COLOR_WARN     0x0E
#define COLOR_BAD      0x0C
#define COLOR_DIM      0x08
#define COLOR_SELECTED 0x1F

#define RD_SCAN_UP        0x0001
#define RD_SCAN_DOWN      0x0002
#define RD_SCAN_RIGHT     0x0003
#define RD_SCAN_LEFT      0x0004
#define RD_SCAN_HOME      0x0005
#define RD_SCAN_END       0x0006
#define RD_SCAN_PAGE_UP   0x0009
#define RD_SCAN_PAGE_DOWN 0x000A
#define RD_SCAN_ESC       0x0017

typedef struct { UINT32 RedMask, GreenMask, BlueMask, ReservedMask; } GOP_PBM;
typedef struct { UINT32 Ver, HRes, VRes, Fmt; GOP_PBM Info; UINT32 PSL; } GOP_MI;
typedef struct { UINT8 B, G, R, A; } GOP_PX;
typedef struct { UINT32 Max, Mode; GOP_MI* Info; UINTN SInfo; EFI_PHYSICAL_ADDRESS FB; UINTN FBSz; } GOP_MODE;
typedef struct _GOP GOP;
struct _GOP {
    EFI_STATUS (EFIAPI *QMode)(GOP*, UINT32, UINTN*, GOP_MI**);
    EFI_STATUS (EFIAPI *SMode)(GOP*, UINT32);
    EFI_STATUS (EFIAPI *Blt)(GOP*, GOP_PX*, UINT32, UINTN, UINTN, UINTN, UINTN, UINTN, UINTN, UINTN);
    GOP_MODE* Mode;
};

typedef struct
{
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE* SystemTable;
    EFI_HANDLE DeviceHandle;
    VOID* FilePath;
    VOID* Reserved;
    UINT32 LoadOptionsSize;
    VOID* LoadOptions;
    VOID* ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    EFI_STATUS (EFIAPI *Unload)(EFI_HANDLE ImageHandle);
} RD_LOADED_IMAGE;



#define GOP_GUID_VALUE {0x9042a9de,0x23dc,0x4a38,{0x96,0xfb,0x7a,0xde,0xd0,0x80,0x51,0x6a}}

static EFI_GUID RdAcpiTableGuid  = { 0xEB9D2D30, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } };
static EFI_GUID RdAcpi2TableGuid = { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } };
static EFI_GUID RdLoadedImageProtocolGuid = { 0x5B1B31A1, 0x9562, 0x11D2, { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID RdDevicePathProtocolGuid = { 0x09576E91, 0x6D3F, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID RdSimpleFileSystemProtocolGuid = { 0x964E5B22, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };

static EFI_GUID RdAbsolutePointerProtocolGuid = { 0x8D59D32B, 0xC655, 0x4A9E, { 0x9B, 0x15, 0xF2, 0x59, 0x04, 0x99, 0x2A, 0x43 } };
static EFI_GUID RdBlockIoProtocolGuid = { 0x964E5B21, 0x6459, 0x11D2, { 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B } };
static EFI_GUID RdSimpleNetworkProtocolGuid = { 0xA19832B9, 0xAC25, 0x11D3, { 0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } };
static EFI_GUID RdTcg2ProtocolGuid = { 0x607F766C, 0x7455, 0x42BE, { 0x93, 0x0B, 0xE4, 0xD7, 0x6D, 0xB2, 0x72, 0x0F } };

typedef struct
{
    UINT32 MediaId;
    BOOLEAN RemovableMedia;
    BOOLEAN MediaPresent;
    BOOLEAN LogicalPartition;
    BOOLEAN ReadOnly;
    BOOLEAN WriteCaching;
    UINT32 BlockSize;
    UINT32 IoAlign;
    UINT64 LastBlock;
    UINT64 LowestAlignedLba;
    UINT32 LogicalBlocksPerPhysicalBlock;
} RD_BLOCK_IO_MEDIA;

typedef struct _RD_BLOCK_IO RD_BLOCK_IO;
struct _RD_BLOCK_IO
{
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Reset)(RD_BLOCK_IO*, BOOLEAN);
    EFI_STATUS (EFIAPI *ReadBlocks)(RD_BLOCK_IO*, UINT32, UINT64, UINTN, VOID*);
    EFI_STATUS (EFIAPI *WriteBlocks)(RD_BLOCK_IO*, UINT32, UINT64, UINTN, VOID*);
    EFI_STATUS (EFIAPI *FlushBlocks)(RD_BLOCK_IO*);
    RD_BLOCK_IO_MEDIA *Media;
};

typedef struct _RD_SIMPLE_NETWORK RD_SIMPLE_NETWORK;
typedef struct
{
    UINT32 State;
    UINT32 HwAddressSize;
    UINT8 MacAddress[32];
    UINT8 PermanentAddress[32];
    UINT32 IfType;
    BOOLEAN MaxBitRateSupported;
    BOOLEAN MediaPresentSupported;
    BOOLEAN MediaPresent;
} RD_SNP_MODE;

struct _RD_SIMPLE_NETWORK
{
    UINT64 Revision;
    EFI_STATUS (EFIAPI *Start)(RD_SIMPLE_NETWORK*);
    EFI_STATUS (EFIAPI *Stop)(RD_SIMPLE_NETWORK*);
    EFI_STATUS (EFIAPI *Initialize)(RD_SIMPLE_NETWORK*, UINTN, UINTN);
    EFI_STATUS (EFIAPI *Reset)(RD_SIMPLE_NETWORK*, BOOLEAN);
    EFI_STATUS (EFIAPI *Shutdown)(RD_SIMPLE_NETWORK*);
    EFI_STATUS (EFIAPI *ReceiveFilters)(RD_SIMPLE_NETWORK*, UINT32, UINT32, BOOLEAN, UINTN, UINT32*);
    EFI_STATUS (EFIAPI *StationAddress)(RD_SIMPLE_NETWORK*, BOOLEAN, VOID*);
    EFI_STATUS (EFIAPI *Statistics)(RD_SIMPLE_NETWORK*, BOOLEAN, UINTN*, VOID*);
    EFI_STATUS (EFIAPI *MCastIpToMac)(RD_SIMPLE_NETWORK*, BOOLEAN, VOID*, VOID*);
    EFI_STATUS (EFIAPI *NvData)(RD_SIMPLE_NETWORK*, BOOLEAN, UINTN, VOID*);
    EFI_STATUS (EFIAPI *GetStatus)(RD_SIMPLE_NETWORK*, UINT32*, VOID**);
    EFI_STATUS (EFIAPI *Transmit)(RD_SIMPLE_NETWORK*, UINTN, UINTN, VOID*, VOID*, VOID*);
    EFI_STATUS (EFIAPI *Receive)(RD_SIMPLE_NETWORK*, UINTN*, VOID*, VOID**, VOID*);
    EFI_EVENT WaitForPacket;
    RD_SNP_MODE *Mode;
};

static EFI_HANDLE g_ImageHandle = NULL;

void DiagnosticsSetImageHandle(EFI_HANDLE imageHandle)
{
    g_ImageHandle = imageHandle;
}

static void SetColor(UINTN attr)
{
    gST->ConOut->SetAttribute(gST->ConOut, attr);
}

static void Rule(void)
{
    SetColor(COLOR_DIM);
    Print(L"  ------------------------------------------------------------\n");
    SetColor(COLOR_NORMAL);
}

static UINTN PgGetCols(void)
{
    UINTN c = 80, r;
    if (gST->ConOut && gST->ConOut->Mode)
        gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &c, &r);
    return c;
}

static void PgCenter(const CHAR16 *s)
{
    UINTN cols = PgGetCols();
    UINTN len = 0;
    while (s[len]) len++;
    UINTN pad = (len < cols) ? (cols - len) / 2 : 0;
    for (UINTN i = 0; i < pad; i++) Print(L" ");
    Print(s);
}

static UINTN g_PgLine;
static UINTN g_PgRows;
static BOOLEAN g_PgExit;
static BOOLEAN g_PgRestart;

static void PgInit(void)
{
    UINTN cols = 80;
    g_PgRows = 25;
    if (gST->ConOut && gST->ConOut->Mode)
        gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &cols, &g_PgRows);
    if (g_PgRows < 8) g_PgRows = 25;
    g_PgRows -= 2;
    g_PgLine = 0;
    g_PgExit = FALSE;
}

static BOOLEAN PgCheck(UINTN n)
{
    if (g_PgExit) return FALSE;
    g_PgLine += n;
    if (g_PgLine >= g_PgRows) {
        SetColor(COLOR_DIM);
        Print(L"  [SPACE/PgDn: next  UP/PgUp: prev page  Q: quit]\n");
        SetColor(COLOR_NORMAL);
        while (TRUE)
        {
            UINTN idx;
            gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);
            EFI_INPUT_KEY key;
            gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
            if (key.UnicodeChar == L'q' || key.UnicodeChar == L'Q' || key.ScanCode == RD_SCAN_ESC) {
                g_PgExit = TRUE;
                return FALSE;
            }
            if (key.UnicodeChar == L' ' || key.ScanCode == RD_SCAN_DOWN || key.ScanCode == RD_SCAN_PAGE_DOWN || key.UnicodeChar == L'\r') {
                g_PgLine = 0;
                return TRUE;
            }
            if (key.ScanCode == RD_SCAN_UP || key.ScanCode == RD_SCAN_PAGE_UP) {
                g_PgRestart = TRUE;
                g_PgExit = TRUE;
                gST->ConOut->ClearScreen(gST->ConOut);
                return FALSE;
            }
        }
    }
    return TRUE;
}

void PgReset(void)
{
    g_PgExit = FALSE;
    g_PgRestart = FALSE;
}

BOOLEAN WasPgRestarted(void)
{
    return g_PgRestart;
}

static void Panel(const CHAR16* title)
{
    Print(L"\n");
    SetColor(COLOR_TITLE);
    Print(L"  %s\n", title);
    Rule();
}

static void CPanel(const CHAR16* title)
{
    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(title);
    Print(L"\n");
    Rule();
    PgCheck(3);
}

static void BrandLine(void)
{
    SetColor(COLOR_OK);
    Print(L"  Created by Leksa667 - Netari\n");
    SetColor(COLOR_NORMAL);
}

static const CHAR16* BuildArch(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return L"x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return L"ia32";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return L"aarch64";
#elif defined(__arm__) || defined(_M_ARM)
    return L"arm";
#else
    return L"unknown";
#endif
}

static BOOLEAN LocateGop(GOP** out)
{
    EFI_GUID guid = GOP_GUID_VALUE;
    *out = NULL;
    return !EFI_ERROR(gBS->LocateProtocol(&guid, NULL, (VOID**)out)) && *out;
}

static const CHAR16* PixelFormatName(UINT32 format)
{
    switch (format)
    {
    case 0:
        return L"RGBx";
    case 1:
        return L"BGRx";
    case 2:
        return L"BitMask";
    case 3:
        return L"BltOnly";
    default:
        return L"Unknown";
    }
}

static UINTN SafeTableLength(SMBIOS_STRUCTURE_POINTER table)
{
    if (!table.Raw || table.Hdr->Length < 4)
        return 0;

    UINTN length = TableLength(table);
    if (length < table.Hdr->Length)
        return 0;

    return length;
}

static UINTN CountSmbiosTables(const SMBIOS_STRUCTURE_TABLE* entry, UINTN* counts, UINTN countSize)
{
    if (!entry || !entry->TableAddress)
        return 0;

    SMBIOS_STRUCTURE_POINTER table;
    table.Raw = (UINT8*)((UINTN)entry->TableAddress);
    SmbiosSetActiveTableBounds(entry);

    UINTN total = 0;
    UINTN remaining = SmbiosBytesRemaining(entry, table);
    while (table.Raw && remaining >= 4 && table.Hdr->Type != SMBIOS_TYPE_END_OF_TABLE && total < 512)
    {
        if (table.Hdr->Type < countSize)
            counts[table.Hdr->Type]++;

        UINTN length = SafeTableLength(table);
        if (length == 0 || length > remaining)
            break;

        table.Raw += length;
        remaining = SmbiosBytesRemaining(entry, table);
        total++;
    }

    return total;
}

static BOOLEAN HasConfigTable(EFI_GUID* guid, VOID** table)
{
    *table = NULL;
    return !EFI_ERROR(LibGetSystemConfigurationTable(guid, table)) && *table;
}

static const CHAR16* MemoryTypeName(UINT32 type)
{
    switch (type)
    {
    case EfiReservedMemoryType:
        return L"Reserved";
    case EfiLoaderCode:
        return L"Loader Code";
    case EfiLoaderData:
        return L"Loader Data";
    case EfiBootServicesCode:
        return L"Boot Code";
    case EfiBootServicesData:
        return L"Boot Data";
    case EfiRuntimeServicesCode:
        return L"Runtime Code";
    case EfiRuntimeServicesData:
        return L"Runtime Data";
    case EfiConventionalMemory:
        return L"Conventional";
    case EfiUnusableMemory:
        return L"Unusable";
    case EfiACPIReclaimMemory:
        return L"ACPI Reclaim";
    case EfiACPIMemoryNVS:
        return L"ACPI NVS";
    case EfiMemoryMappedIO:
        return L"MMIO";
    case EfiMemoryMappedIOPortSpace:
        return L"MMIO Ports";
    case EfiPalCode:
        return L"PAL Code";
    default:
        return L"Other";
    }
}

static UINTN PagesToMiB(UINT64 pages)
{
    return (UINTN)(pages / 256);
}

static void PrintStatus(const CHAR16* label, BOOLEAN ok)
{
    Print(L"  ");
    SetColor(ok ? COLOR_OK : COLOR_WARN);
    Print(ok ? L"[OK] " : L"[--] ");
    SetColor(COLOR_NORMAL);
    Print(L"%s\n", label);
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

static void PrintStringField(SMBIOS_STRUCTURE_POINTER table, const CHAR16* label, UINT8 index)
{
    const char* value = GetStringAtIndex(table, index);

    Print(L"  %s", label);
    if (value)
        Print(L"%a\n", value);
    else
        Print(L"[not set]\n");
}

static void PrintStringOffset(SMBIOS_STRUCTURE_POINTER table, const CHAR16* label, UINTN offset)
{
    PrintStringField(table, label, ReadU8(table, offset));
}

static void PrintUuidField(SMBIOS_STRUCTURE_POINTER table, const CHAR16* label)
{
    if (!HasBytes(table, 0x08, 16))
    {
        Print(L"  %s[not set]\n", label);
        return;
    }

    const UINT8* uuid = table.Raw + 0x08;
    Print(L"  %s%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
          label,
          uuid[0], uuid[1], uuid[2], uuid[3],
          uuid[4], uuid[5], uuid[6], uuid[7],
          uuid[8], uuid[9], uuid[10], uuid[11],
          uuid[12], uuid[13], uuid[14], uuid[15]);
}

static UINTN MemoryDeviceMiB(SMBIOS_STRUCTURE_POINTER table)
{
    UINT16 size = ReadU16(table, 0x0C);

    if (size == 0 || size == 0xFFFF)
        return 0;

    if (size == 0x7FFF && HasBytes(table, 0x1C, 4))
        return ReadU32(table, 0x1C);

    if (size & 0x8000)
        return (size & 0x7FFF) / 1024;

    return size;
}

static const CHAR16* SmbiosMemoryTypeName(UINT8 type)
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

static const CHAR16* SmbiosMemoryFormFactorName(UINT8 form)
{
    switch (form)
    {
    case 0x08:
        return L"DIMM";
    case 0x09:
        return L"TSOP";
    case 0x0A:
        return L"Row of chips";
    case 0x0B:
        return L"RIMM";
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

static const CHAR16* BoardTypeName(UINT8 type)
{
    switch (type)
    {
    case 0x0A:
        return L"Motherboard";
    case 0x0B:
        return L"Processor/Memory Module";
    case 0x0C:
        return L"I/O Module";
    case 0x0D:
        return L"Interconnect Board";
    default:
        return L"Unknown";
    }
}

static const CHAR16* ChassisTypeName(UINT8 type)
{
    switch (type & 0x7F)
    {
    case 0x03:
        return L"Desktop";
    case 0x06:
        return L"Mini Tower";
    case 0x07:
        return L"Tower";
    case 0x08:
        return L"Portable";
    case 0x09:
        return L"Laptop";
    case 0x0A:
        return L"Notebook";
    case 0x0D:
        return L"All-in-One";
    case 0x1E:
        return L"Tablet";
    case 0x1F:
        return L"Convertible";
    default:
        return L"Unknown";
    }
}

static const CHAR16* SlotUsageName(UINT8 usage)
{
    switch (usage)
    {
    case 0x03:
        return L"Available";
    case 0x04:
        return L"In use";
    case 0x05:
        return L"Unavailable";
    default:
        return L"Unknown";
    }
}

static const CHAR16* SlotLengthName(UINT8 length)
{
    switch (length)
    {
    case 0x03:
        return L"Short";
    case 0x04:
        return L"Long";
    case 0x05:
        return L"2.5 drive";
    case 0x06:
        return L"3.5 drive";
    default:
        return L"Unknown";
    }
}

static const CHAR16* OnboardDeviceTypeName(UINT8 type)
{
    switch (type & 0x7F)
    {
    case 0x01:
        return L"Other";
    case 0x02:
        return L"Unknown";
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

static EFI_GUID RdEfiGlobalVariableGuid = { 0x8BE4DF61, 0x93CA, 0x11D2, { 0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C } };

static BOOLEAN ReadGlobalVariable(const CHAR16* name, VOID* buffer, UINTN* size, UINT32* attributes)
{
    UINT32 attrs = 0;
    EFI_STATUS status = gRT->GetVariable((CHAR16*)name, &RdEfiGlobalVariableGuid, &attrs, size, buffer);

    if (attributes)
        *attributes = attrs;

    return !EFI_ERROR(status);
}

static BOOLEAN QueryGlobalVariable(const CHAR16* name, UINTN* requiredSize, UINT32* attributes)
{
    UINTN size = 0;
    UINT32 attrs = 0;
    EFI_STATUS status = gRT->GetVariable((CHAR16*)name, &RdEfiGlobalVariableGuid, &attrs, &size, NULL);

    if (requiredSize)
        *requiredSize = size;

    if (attributes)
        *attributes = attrs;

    return status == EFI_BUFFER_TOO_SMALL || status == EFI_SUCCESS;
}

static BOOLEAN GlobalVariableExists(const CHAR16* name, UINTN* requiredSize)
{
    return QueryGlobalVariable(name, requiredSize, NULL);
}

static BOOLEAN ReadGlobalU8(const CHAR16* name, UINT8* value)
{
    UINTN size = sizeof(*value);
    return ReadGlobalVariable(name, value, &size, NULL) && size == sizeof(*value);
}

static BOOLEAN ReadGlobalU16(const CHAR16* name, UINT16* value)
{
    UINTN size = sizeof(*value);
    return ReadGlobalVariable(name, value, &size, NULL) && size == sizeof(*value);
}

static BOOLEAN ReadGlobalU64(const CHAR16* name, UINT64* value)
{
    UINTN size = sizeof(*value);
    return ReadGlobalVariable(name, value, &size, NULL) && size == sizeof(*value);
}

static void PrintVariableMeta(const CHAR16* name)
{
    UINTN size = 0;
    UINT32 attrs = 0;

    Print(L"  %s: ", name);
    if (QueryGlobalVariable(name, &size, &attrs))
        Print(L"%d bytes, attr 0x%08x\n", size, attrs);
    else
        Print(L"unavailable\n");
}

static void PrintAsciiVariable(const CHAR16* name)
{
    char value[96];
    UINTN size = sizeof(value) - 1;

    gBS->SetMem(value, sizeof(value), 0);
    Print(L"  %s: ", name);
    if (ReadGlobalVariable(name, value, &size, NULL))
    {
        value[(size < sizeof(value)) ? size : sizeof(value) - 1] = 0;
        Print(L"%a\n", value);
    }
    else
    {
        Print(L"unavailable\n");
    }
}

static CHAR16 HexDigit(UINT8 value)
{
    value &= 0x0F;
    return (CHAR16)(value < 10 ? L'0' + value : L'A' + value - 10);
}

static void MakeBootOptionName(UINT16 id, CHAR16* name)
{
    name[0] = L'B';
    name[1] = L'o';
    name[2] = L'o';
    name[3] = L't';
    name[4] = HexDigit((UINT8)(id >> 12));
    name[5] = HexDigit((UINT8)(id >> 8));
    name[6] = HexDigit((UINT8)(id >> 4));
    name[7] = HexDigit((UINT8)id);
    name[8] = 0;
}

static void PrintChar16Bounded(const CHAR16* text, UINTN maxChars)
{
    for (UINTN i = 0; i < maxChars; i++)
    {
        CHAR16 c = text[i];
        if (!c)
            break;
        Print(L"%c", (c >= 32 && c < 127) ? c : L'.');
    }
}

static void PrintBootOptionSummary(UINT16 id)
{
    CHAR16 name[9];
    UINT8 data[1536];
    UINTN size = sizeof(data);
    UINT32 varAttrs = 0;

    MakeBootOptionName(id, name);
    gBS->SetMem(data, sizeof(data), 0);

    if (!ReadGlobalVariable(name, data, &size, &varAttrs))
    {
        UINTN required = 0;
        if (QueryGlobalVariable(name, &required, &varAttrs))
            Print(L"  %s: %d bytes, attr 0x%08x\n", name, required, varAttrs);
        else
            Print(L"  %s: unavailable\n", name);
        return;
    }

    if (size < 6)
    {
        Print(L"  %s: malformed (%d bytes)\n", name, size);
        return;
    }

    UINT32 optionAttrs = (UINT32)data[0]
                       | ((UINT32)data[1] << 8)
                       | ((UINT32)data[2] << 16)
                       | ((UINT32)data[3] << 24);
    UINT16 pathBytes = (UINT16)data[4] | ((UINT16)data[5] << 8);
    UINTN descChars = (size - 6) / sizeof(CHAR16);

    Print(L"  %s: opt 0x%08x, path %u bytes, var 0x%08x, ",
          name, optionAttrs, pathBytes, varAttrs);
    PrintChar16Bounded((const CHAR16*)(data + 6), descChars);
    Print(L"\n");
}

static void PrintTableHeaderInfo(const CHAR16* label, const EFI_TABLE_HEADER* header)
{
    UINT64 signature = header ? header->Signature : 0;

    if (!header)
    {
        Print(L"  %s: unavailable\n", label);
        return;
    }

    Print(L"  %s Signature: 0x%08x%08x\n",
          label, (UINTN)(signature >> 32), (UINTN)(signature & 0xFFFFFFFF));
    Print(L"  %s Revision:  0x%08x\n", label, header->Revision);
    Print(L"  %s Header:    %u bytes, CRC32 0x%08x\n", label, header->HeaderSize, header->CRC32);
}

static void PrintGuidValue(const EFI_GUID* guid)
{
    Print(L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          guid->Data1, guid->Data2, guid->Data3,
          guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
          guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
}

static void PrintAsciiFixed(const CHAR16* label, const UINT8* text, UINTN length)
{
    Print(L"  %s", label);
    for (UINTN i = 0; i < length; i++)
    {
        UINT8 c = text[i];
        Print(L"%c", (CHAR16)((c >= 32 && c <= 126) ? c : '.'));
    }
    Print(L"\n");
}

static void PrintAsciiInline(const UINT8* text, UINTN length)
{
    for (UINTN i = 0; i < length; i++)
    {
        UINT8 c = text[i];
        Print(L"%c", (CHAR16)((c >= 32 && c <= 126) ? c : '.'));
    }
}

static UINT8 ByteSum(const UINT8* data, UINTN length)
{
    UINT8 sum = 0;

    for (UINTN i = 0; i < length; i++)
        sum = (UINT8)(sum + data[i]);

    return sum;
}

static UINT16 ReadLe16(const UINT8* data)
{
    return (UINT16)data[0] | ((UINT16)data[1] << 8);
}

static UINT32 ReadLe32(const UINT8* data)
{
    return (UINT32)data[0]
         | ((UINT32)data[1] << 8)
         | ((UINT32)data[2] << 16)
         | ((UINT32)data[3] << 24);
}

static UINT64 ReadLe64(const UINT8* data)
{
    UINT64 value = 0;

    for (UINTN i = 0; i < sizeof(UINT64); i++)
        value |= ((UINT64)data[i]) << (8 * i);

    return value;
}

void DisplayOverview(const SMBIOS_STRUCTURE_TABLE* entry)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - System Overview");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    CPanel(L"Platform");
    Print(L"  Firmware Vendor:   %s\n", gST->FirmwareVendor ? gST->FirmwareVendor : L"[unknown]");
    Print(L"  Firmware Revision: 0x%08x\n", gST->FirmwareRevision);
    Print(L"  UEFI Revision:     %u.%u (0x%08x)\n",
          (gST->Hdr.Revision >> 16) & 0xFFFF, gST->Hdr.Revision & 0xFFFF, gST->Hdr.Revision);
    Print(L"  Build Target:      %s\n", BuildArch());
    Print(L"  Config Tables:     %d\n", gST->NumberOfTableEntries);

    CPanel(L"Console & Runtime");
    if (gST->ConOut && gST->ConOut->Mode)
    {
        UINTN cols = 0;
        UINTN rows = 0;
        UINTN mode = (UINTN)gST->ConOut->Mode->Mode;
        EFI_STATUS status = gST->ConOut->QueryMode(gST->ConOut, mode, &cols, &rows);
        Print(L"  Console Mode:      %d / %d\n", mode, gST->ConOut->Mode->MaxMode);
        if (!EFI_ERROR(status))
            Print(L"  Console Grid:      %d x %d\n", cols, rows);
        Print(L"  Text Attribute:    0x%02x\n", gST->ConOut->Mode->Attribute);
        Print(L"  Cursor:            %d,%d visible %u\n",
              gST->ConOut->Mode->CursorColumn,
              gST->ConOut->Mode->CursorRow,
              gST->ConOut->Mode->CursorVisible ? 1 : 0);
    }
    EFI_TIME now;
    EFI_TIME_CAPABILITIES caps;
    if (gRT->GetTime && !EFI_ERROR(gRT->GetTime(&now, &caps)))
    {
        Print(L"  Firmware Time:     %04u-%02u-%02u %02u:%02u:%02u\n",
              now.Year, now.Month, now.Day, now.Hour, now.Minute, now.Second);
        Print(L"  Time Zone:         %d, daylight 0x%02x\n", now.TimeZone, now.Daylight);
        Print(L"  Clock Caps:        res %u, accuracy %u, sets-zero %u\n",
              caps.Resolution, caps.Accuracy, caps.SetsToZero ? 1 : 0);
    }

    CPanel(L"Discovery");
    PrintStatus(L"UEFI system table is available", gST != NULL);
    PrintStatus(L"Boot services table is available", gBS != NULL);
    PrintStatus(L"Runtime services table is available", gRT != NULL);

    if (entry)
    {
        SetColor(COLOR_OK);
        Print(L"  [OK] ");
        SetColor(COLOR_NORMAL);
        Print(L"SMBIOS v%u.%u at 0x%08x\n",
              entry->MajorVersion, entry->MinorVersion, (UINTN)entry->TableAddress);
    }
    else
    {
        SetColor(COLOR_WARN);
        Print(L"  [--] ");
        SetColor(COLOR_NORMAL);
        Print(L"SMBIOS was not exposed through the standard UEFI paths\n");
    }

    VOID* table = NULL;
    PrintStatus(L"ACPI 2.0 table", HasConfigTable(&RdAcpi2TableGuid, &table));
    PrintStatus(L"ACPI 1.0 table", HasConfigTable(&RdAcpiTableGuid, &table));
    PrintStatus(L"SecureBoot variable", GlobalVariableExists(L"SecureBoot", NULL));
    PrintStatus(L"BootOrder variable", GlobalVariableExists(L"BootOrder", NULL));
    PrintStatus(L"Absolute Pointer input", !EFI_ERROR(gBS->LocateProtocol(&RdAbsolutePointerProtocolGuid, NULL, &table)));

    RD_LOADED_IMAGE* loaded = NULL;
    BOOLEAN hasLoadedImage = g_ImageHandle
        && !EFI_ERROR(gBS->HandleProtocol(g_ImageHandle, &RdLoadedImageProtocolGuid, (VOID**)&loaded))
        && loaded;
    PrintStatus(L"Loaded Image Protocol", hasLoadedImage);
    if (hasLoadedImage)
        Print(L"  Image Size:        %d KiB\n", (UINTN)(loaded->ImageSize / 1024));

    GOP* gop = NULL;
    if (LocateGop(&gop) && gop->Mode && gop->Mode->Info)
    {
        SetColor(COLOR_OK);
        Print(L"  [OK] ");
        SetColor(COLOR_NORMAL);
        Print(L"Graphics Output: %ux%u, %s\n",
              gop->Mode->Info->HRes, gop->Mode->Info->VRes,
              PixelFormatName(gop->Mode->Info->Fmt));
    }
    else
    {
        PrintStatus(L"Graphics Output Protocol", FALSE);
    }

    if (entry)
    {
        UINTN counts[SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION + 1];
        gBS->SetMem(counts, sizeof(counts), 0);
        UINTN total = CountSmbiosTables(entry, counts, sizeof(counts) / sizeof(counts[0]));

        CPanel(L"SMBIOS Quick Map");
        Print(L"  Table Length:      %d bytes\n", SmbiosEntryTableLength(entry));
        Print(L"  Max Struct Size:   %u bytes\n", entry->MaxStructureSize);
        Print(L"  Declared Count:    %u\n", entry->NumberOfSmbiosStructures);
        Print(L"  Tables detected:   %d\n", total);
        Print(L"  BIOS/System/Board: %d / %d / %d\n",
              counts[SMBIOS_TYPE_BIOS_INFORMATION],
              counts[SMBIOS_TYPE_SYSTEM_INFORMATION],
              counts[SMBIOS_TYPE_BASEBOARD_INFORMATION]);
        Print(L"  CPU entries:       %d\n", counts[SMBIOS_TYPE_PROCESSOR_INFORMATION]);
        Print(L"  Memory arrays:     %d\n", counts[SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY]);
        Print(L"  Memory devices:    %d\n", counts[SMBIOS_TYPE_MEMORY_DEVICE]);
        Print(L"  TPM entries:       %d\n", counts[SMBIOS_TYPE_TPM_DEVICE]);
    }

    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

void DisplayHardwareReport(const SMBIOS_STRUCTURE_TABLE* entry)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - Full Hardware Report");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    if (!entry || !entry->TableAddress)
    {
        SetColor(COLOR_WARN);
        Print(L"  SMBIOS is not available on this firmware path.\n");
        SetColor(COLOR_NORMAL);
        WaitForAnyKey();
        return;
    }

    SmbiosSetActiveTableBounds(entry);

    CPanel(L"Machine Identity");
    SMBIOS_STRUCTURE_POINTER table = FindTableByType(entry, SMBIOS_TYPE_BIOS_INFORMATION, 0);
    if (table.Raw)
    {
        PrintStringField(table, L"BIOS Vendor:       ", table.Type0->Vendor);
        PrintStringField(table, L"BIOS Version:      ", table.Type0->BiosVersion);
        PrintStringField(table, L"BIOS Date:         ", table.Type0->BiosReleaseDate);
        if (HasBytes(table, 0x06, 2))
            Print(L"  BIOS Segment:      0x%04x\n", ReadU16(table, 0x06));
        if (HasBytes(table, 0x09, 1))
            Print(L"  ROM Size Byte:     0x%02x\n", ReadU8(table, 0x09));
        if (HasBytes(table, 0x0A, 8))
            Print(L"  BIOS Chars:        0x%08x%08x\n",
                  (UINTN)ReadU32(table, 0x0E), (UINTN)ReadU32(table, 0x0A));
        if (HasBytes(table, 0x12, 2))
            Print(L"  BIOS Ext Chars:    0x%02x 0x%02x\n", ReadU8(table, 0x12), ReadU8(table, 0x13));
        if (HasBytes(table, 0x14, 4))
        {
            Print(L"  BIOS Revision:     %u.%u\n", ReadU8(table, 0x14), ReadU8(table, 0x15));
            Print(L"  EC FW Revision:    %u.%u\n", ReadU8(table, 0x16), ReadU8(table, 0x17));
        }
    }

    table = FindTableByType(entry, SMBIOS_TYPE_SYSTEM_INFORMATION, 0);
    if (table.Raw)
    {
        PrintStringField(table, L"System Maker:      ", table.Type1->Manufacturer);
        PrintStringField(table, L"Product Name:      ", table.Type1->ProductName);
        PrintStringField(table, L"System Version:    ", table.Type1->Version);
        PrintStringField(table, L"System Serial:     ", table.Type1->SerialNumber);
        PrintUuidField(table, L"System UUID:       ");
        if (HasBytes(table, 0x18, 1))
            Print(L"  Wake-up Type:      0x%02x\n", ReadU8(table, 0x18));
        PrintStringOffset(table, L"SKU Number:        ", 0x19);
        PrintStringOffset(table, L"Family:            ", 0x1A);
    }

    table = FindTableByType(entry, SMBIOS_TYPE_BASEBOARD_INFORMATION, 0);
    if (table.Raw)
    {
        PrintStringField(table, L"Board Maker:       ", table.Type2->Manufacturer);
        PrintStringField(table, L"Board Product:     ", table.Type2->ProductName);
        PrintStringField(table, L"Board Version:     ", table.Type2->Version);
        PrintStringField(table, L"Board Serial:      ", table.Type2->SerialNumber);
        PrintStringOffset(table, L"Board Asset Tag:   ", 0x08);
        PrintStringOffset(table, L"Board Location:    ", 0x0A);
        if (HasBytes(table, 0x09, 1))
            Print(L"  Board Flags:       0x%02x\n", ReadU8(table, 0x09));
        if (HasBytes(table, 0x0D, 1))
            Print(L"  Board Type:        0x%02x (%s)\n", ReadU8(table, 0x0D), BoardTypeName(ReadU8(table, 0x0D)));
    }

    table = FindTableByType(entry, SMBIOS_TYPE_SYSTEM_ENCLOSURE, 0);
    if (table.Raw)
    {
        PrintStringField(table, L"Chassis Maker:     ", table.Type3->Manufacturer);
        if (HasBytes(table, 0x05, 1))
            Print(L"  Chassis Type:      0x%02x (%s)\n", ReadU8(table, 0x05), ChassisTypeName(ReadU8(table, 0x05)));
        PrintStringField(table, L"Chassis Version:   ", table.Type3->Version);
        PrintStringField(table, L"Chassis Serial:    ", table.Type3->SerialNumber);
        PrintStringField(table, L"Asset Tag:         ", table.Type3->AssetTag);
        if (HasBytes(table, 0x09, 4))
        {
            Print(L"  Boot State:        0x%02x\n", ReadU8(table, 0x09));
            Print(L"  Power State:       0x%02x\n", ReadU8(table, 0x0A));
            Print(L"  Thermal State:     0x%02x\n", ReadU8(table, 0x0B));
            Print(L"  Security Status:   0x%02x\n", ReadU8(table, 0x0C));
        }
        if (HasBytes(table, 0x11, 2))
            Print(L"  Height/Cords:      %u U / %u\n", ReadU8(table, 0x11), ReadU8(table, 0x12));
    }

    CPanel(L"Processor");
    for (UINTN i = 0; i < 8; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_PROCESSOR_INFORMATION, i);
        if (!table.Raw)
            break;

        Print(L"  CPU #%d\n", i + 1);
        PrintStringOffset(table, L"    Socket:         ", 0x04);
        Print(L"    Type/Family:    0x%02x / 0x%02x\n", ReadU8(table, 0x05), ReadU8(table, 0x06));
        PrintStringOffset(table, L"    Manufacturer:   ", 0x07);
        if (HasBytes(table, 0x08, 8))
            Print(L"    Processor ID:   %02x%02x%02x%02x%02x%02x%02x%02x\n",
                  ReadU8(table, 0x08), ReadU8(table, 0x09), ReadU8(table, 0x0A), ReadU8(table, 0x0B),
                  ReadU8(table, 0x0C), ReadU8(table, 0x0D), ReadU8(table, 0x0E), ReadU8(table, 0x0F));
        PrintStringOffset(table, L"    Version:        ", 0x10);
        Print(L"    External Clock: %u MHz\n", ReadU16(table, 0x12));
        Print(L"    Current Speed:  %u MHz\n", ReadU16(table, 0x16));
        Print(L"    Max Speed:      %u MHz\n", ReadU16(table, 0x14));
        Print(L"    Voltage Raw:    0x%02x\n", ReadU8(table, 0x11));
        Print(L"    Status/Upgrade: 0x%02x / 0x%02x\n", ReadU8(table, 0x18), ReadU8(table, 0x19));
        if (HasBytes(table, 0x1A, 6))
            Print(L"    Cache Handles:  L1 0x%04x, L2 0x%04x, L3 0x%04x\n",
                  ReadU16(table, 0x1A), ReadU16(table, 0x1C), ReadU16(table, 0x1E));
        if (HasBytes(table, 0x23, 3))
            Print(L"    Cores/Threads:  %u / %u\n", ReadU8(table, 0x23), ReadU8(table, 0x25));
        if (HasBytes(table, 0x26, 2))
            Print(L"    CPU Features:   0x%04x\n", ReadU16(table, 0x26));
        if (HasBytes(table, 0x2A, 6))
            Print(L"    Cores Ext:      %u enabled %u threads %u\n",
                  ReadU16(table, 0x2A), ReadU16(table, 0x2C), ReadU16(table, 0x2E));
        PrintStringOffset(table, L"    Serial:         ", 0x20);
        PrintStringOffset(table, L"    Asset Tag:      ", 0x21);
        PrintStringOffset(table, L"    Part Number:    ", 0x22);
        if (!PgCheck(1)) return;
    }

    CPanel(L"Memory");
    table = FindTableByType(entry, SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY, 0);
    if (table.Raw)
    {
        UINT32 maxKb = ReadU32(table, 0x07);
        if (maxKb == 0x80000000 && HasBytes(table, 0x0F, 8))
            Print(L"  Max Capacity:      %d MiB\n", (UINTN)(ReadU64(table, 0x0F) / 1024));
        else if (maxKb)
            Print(L"  Max Capacity:      %d MiB\n", (UINTN)(maxKb / 1024));
        Print(L"  Device Slots:      %u\n", ReadU16(table, 0x0D));
    }

    UINTN totalMiB = 0;
    UINTN installedModules = 0;
    for (UINTN i = 0; i < 32; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_MEMORY_DEVICE, i);
        if (!table.Raw)
            break;

        UINTN sizeMiB = MemoryDeviceMiB(table);
        if (sizeMiB)
        {
            totalMiB += sizeMiB;
            installedModules++;
        }
    }
    Print(L"  Installed Modules: %d\n", installedModules);
    Print(L"  Installed Memory:  %d MiB\n", totalMiB);

    for (UINTN i = 0; i < 16; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_MEMORY_DEVICE, i);
        if (!table.Raw)
            break;

        UINTN sizeMiB = MemoryDeviceMiB(table);
        if (!sizeMiB)
            continue;

        Print(L"  DIMM #%d: %d MiB, %s, %u MT/s\n",
              i + 1, sizeMiB, SmbiosMemoryTypeName(ReadU8(table, 0x12)), ReadU16(table, 0x15));
        Print(L"    Width:          %u data / %u total bits\n", ReadU16(table, 0x0A), ReadU16(table, 0x08));
        Print(L"    Form Factor:    0x%02x (%s)\n", ReadU8(table, 0x0E), SmbiosMemoryFormFactorName(ReadU8(table, 0x0E)));
        PrintStringOffset(table, L"    Locator:        ", 0x10);
        PrintStringOffset(table, L"    Bank:           ", 0x11);
        Print(L"    Type Detail:    0x%04x\n", ReadU16(table, 0x13));
        PrintStringOffset(table, L"    Manufacturer:   ", 0x17);
        PrintStringOffset(table, L"    Serial:         ", 0x18);
        PrintStringOffset(table, L"    Asset Tag:      ", 0x19);
        PrintStringOffset(table, L"    Part Number:    ", 0x1A);
        Print(L"    Attributes:     0x%02x\n", ReadU8(table, 0x1B));
        if (HasBytes(table, 0x20, 2))
            Print(L"    Configured:     %u MT/s\n", ReadU16(table, 0x20));
        if (HasBytes(table, 0x22, 6))
            Print(L"    Voltage mV:     min %u, max %u, configured %u\n",
                  ReadU16(table, 0x22), ReadU16(table, 0x24), ReadU16(table, 0x26));
        if (!PgCheck(1)) return;
    }

    CPanel(L"Security Hardware");
    table = FindTableByType(entry, SMBIOS_TYPE_TPM_DEVICE, 0);
    if (table.Raw)
    {
        if (HasBytes(table, 0x04, 4))
            Print(L"  TPM Vendor:        %c%c%c%c\n",
                  (CHAR16)ReadU8(table, 0x04), (CHAR16)ReadU8(table, 0x05),
                  (CHAR16)ReadU8(table, 0x06), (CHAR16)ReadU8(table, 0x07));
        Print(L"  TPM Spec:          %u.%u\n", ReadU8(table, 0x08), ReadU8(table, 0x09));
        Print(L"  Firmware Raw:      0x%08x\n", ReadU32(table, 0x0A));
        PrintStringOffset(table, L"  Description:      ", 0x12);
    }
    else
    {
        Print(L"  TPM SMBIOS entry:  not exposed\n");
    }

    CPanel(L"Expansion Slots");
    UINTN slotCount = 0;
    for (UINTN i = 0; i < 16; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_SYSTEM_SLOTS, i);
        if (!table.Raw)
            break;

        slotCount++;
        Print(L"  Slot #%d: usage %s, length %s, id 0x%04x\n",
              i + 1, SlotUsageName(ReadU8(table, 0x07)), SlotLengthName(ReadU8(table, 0x08)), ReadU16(table, 0x09));
        PrintStringOffset(table, L"    Designation:    ", 0x04);
    }

    if (!slotCount)
        Print(L"  No slot entries exposed through SMBIOS.\n");

    CPanel(L"Onboard Devices");
    UINTN onboardCount = 0;
    for (UINTN i = 0; i < 16; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION, i);
        if (!table.Raw)
            break;

        onboardCount++;
        UINT8 type = ReadU8(table, 0x05);
        Print(L"  Device #%d: %s, enabled %u, instance %u\n",
              i + 1, OnboardDeviceTypeName(type), (type & 0x80) ? 1 : 0, ReadU8(table, 0x06));
        PrintStringOffset(table, L"    Reference:      ", 0x04);
        if (HasBytes(table, 0x07, 4))
            Print(L"    PCI Path:       segment %u, bus %u, devfn 0x%02x\n",
                  ReadU16(table, 0x07), ReadU8(table, 0x09), ReadU8(table, 0x0A));
    }

    for (UINTN i = 0; i < 8; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION, i);
        if (!table.Raw)
            break;

        for (UINTN offset = 0x04; offset + 1 < table.Hdr->Length; offset += 2)
        {
            UINT8 type = ReadU8(table, offset);
            UINT8 str = ReadU8(table, offset + 1);
            onboardCount++;
            Print(L"  Legacy Device #%d: %s, enabled %u\n",
                  onboardCount, OnboardDeviceTypeName(type), (type & 0x80) ? 1 : 0);
            PrintStringField(table, L"    Reference:      ", str);
        }
    }

    if (!onboardCount)
        Print(L"  No onboard device entries exposed through SMBIOS.\n");

    CPanel(L"Power & Battery");
    UINTN powerCount = 0;
    for (UINTN i = 0; i < 4; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_PORTABLE_BATTERY, i);
        if (!table.Raw)
            break;

        powerCount++;
        Print(L"  Battery #%d\n", i + 1);
        PrintStringOffset(table, L"    Location:       ", 0x04);
        PrintStringOffset(table, L"    Manufacturer:   ", 0x05);
        PrintStringOffset(table, L"    Device Name:    ", 0x08);
        Print(L"    Chemistry:      0x%02x\n", ReadU8(table, 0x09));
        Print(L"    Design:         %u mWh, %u mV\n", ReadU16(table, 0x0A), ReadU16(table, 0x0C));
        Print(L"    Max Error:      %u%%\n", ReadU8(table, 0x0F));
    }

    for (UINTN i = 0; i < 4; i++)
    {
        table = FindTableByType(entry, SMBIOS_TYPE_SYSTEM_POWER_SUPPLY, i);
        if (!table.Raw)
            break;

        powerCount++;
        Print(L"  Power Supply #%d\n", i + 1);
        PrintStringOffset(table, L"    Location:       ", 0x05);
        PrintStringOffset(table, L"    Device Name:    ", 0x06);
        PrintStringOffset(table, L"    Manufacturer:   ", 0x07);
        PrintStringOffset(table, L"    Serial:         ", 0x08);
        PrintStringOffset(table, L"    Asset Tag:      ", 0x09);
        PrintStringOffset(table, L"    Model:          ", 0x0A);
        Print(L"    Max Power:      %u W\n", ReadU16(table, 0x0C));
        Print(L"    Characteristics:0x%04x\n", ReadU16(table, 0x0E));
    }

    if (!powerCount)
        Print(L"  No battery or power supply entries exposed through SMBIOS.\n");

    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

void DisplayMemoryMap(void)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - UEFI Memory Map");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    UINTN mapSize = 0;
    UINTN mapKey = 0;
    UINTN descSize = 0;
    UINT32 descVersion = 0;
    EFI_STATUS status = gBS->GetMemoryMap(&mapSize, NULL, &mapKey, &descSize, &descVersion);

    if (status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(status))
    {
        SetColor(COLOR_BAD);
        Print(L"  Unable to query memory map: %r\n", status);
        SetColor(COLOR_NORMAL);
        WaitForAnyKey();
        return;
    }

    if (descSize == 0)
        descSize = sizeof(EFI_MEMORY_DESCRIPTOR);

    UINTN bufferSize = mapSize + descSize * 8;
    EFI_MEMORY_DESCRIPTOR* map = NULL;
    status = gBS->AllocatePool(EfiLoaderData, bufferSize, (VOID**)&map);
    if (EFI_ERROR(status) || !map)
    {
        SetColor(COLOR_BAD);
        Print(L"  Unable to allocate memory map buffer: %r\n", status);
        SetColor(COLOR_NORMAL);
        WaitForAnyKey();
        return;
    }

    mapSize = bufferSize;
    status = gBS->GetMemoryMap(&mapSize, map, &mapKey, &descSize, &descVersion);
    if (EFI_ERROR(status))
    {
        SetColor(COLOR_BAD);
        Print(L"  Unable to read memory map: %r\n", status);
        SetColor(COLOR_NORMAL);
        gBS->FreePool(map);
        WaitForAnyKey();
        return;
    }

    UINT64 pages[16];
    UINTN descriptors[16];
    gBS->SetMem(pages, sizeof(pages), 0);
    gBS->SetMem(descriptors, sizeof(descriptors), 0);

    UINTN descriptorCount = mapSize / descSize;
    UINT64 highestEnd = 0;
    UINT64 largestPages = 0;
    UINT64 largestStart = 0;
    UINT32 largestType = 0;
    for (UINTN i = 0; i < descriptorCount; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + i * descSize);
        UINT32 type = desc->Type;
        UINT32 bucket = (type < 15) ? type : 15;
        UINT64 start = desc->PhysicalStart;
        UINT64 end = start + desc->NumberOfPages * 4096;

        pages[bucket] += desc->NumberOfPages;
        descriptors[bucket]++;

        if (end > highestEnd)
            highestEnd = end;

        if (desc->NumberOfPages > largestPages)
        {
            largestPages = desc->NumberOfPages;
            largestStart = start;
            largestType = type;
        }
    }

    UINT64 totalPages = 0;
    for (UINTN i = 0; i < 16; i++)
        totalPages += pages[i];

    CPanel(L"Summary");
    Print(L"  Descriptors:       %d\n", descriptorCount);
    Print(L"  Descriptor Size:   %d bytes\n", descSize);
    Print(L"  Descriptor Ver:    %u\n", descVersion);
    Print(L"  Total Mapped RAM:  %d MiB\n", PagesToMiB(totalPages));
    Print(L"  Conventional RAM:  %d MiB\n", PagesToMiB(pages[EfiConventionalMemory]));
    Print(L"  Runtime Memory:    %d MiB\n", PagesToMiB(pages[EfiRuntimeServicesCode] + pages[EfiRuntimeServicesData]));
    Print(L"  ACPI Memory:       %d MiB\n", PagesToMiB(pages[EfiACPIReclaimMemory] + pages[EfiACPIMemoryNVS]));
    Print(L"  Highest Address:   0x%08x\n", (UINTN)highestEnd);
    Print(L"  Largest Region:    %d MiB at 0x%08x (%s)\n",
          PagesToMiB(largestPages), (UINTN)largestStart, MemoryTypeName(largestType));

    CPanel(L"Memory Classes");
    for (UINTN i = 0; i < 16; i++)
    {
        if (!descriptors[i])
            continue;

        Print(L"  %s: %d MiB in %d descriptor(s)\n",
              MemoryTypeName((UINT32)i), PagesToMiB(pages[i]), descriptors[i]);
    }

    CPanel(L"First Memory Descriptors");
    UINTN shown = descriptorCount < 18 ? descriptorCount : 18;
    for (UINTN i = 0; i < shown; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + i * descSize);
        UINT64 end = desc->PhysicalStart + desc->NumberOfPages * 4096;
        Print(L"  %02d  %s  0x%08x - 0x%08x  %d MiB  attr 0x%08x\n",
              i, MemoryTypeName(desc->Type), (UINTN)desc->PhysicalStart, (UINTN)end,
              PagesToMiB(desc->NumberOfPages), (UINTN)desc->Attribute);
    }

    if (descriptorCount > shown)
        Print(L"  ... %d more descriptor(s)\n", descriptorCount - shown);

    gBS->FreePool(map);
    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

void DisplayBootEnvironment(void)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - Boot Environment");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    CPanel(L"UEFI Runtime");
    Print(L"  Firmware Vendor:   %s\n", gST->FirmwareVendor ? gST->FirmwareVendor : L"[unknown]");
    Print(L"  Firmware Revision: 0x%08x\n", gST->FirmwareRevision);
    Print(L"  UEFI Revision:     %u.%u\n",
          (gST->Hdr.Revision >> 16) & 0xFFFF, gST->Hdr.Revision & 0xFFFF);
    PrintStatus(L"GetVariable", gRT->GetVariable != NULL);
    PrintStatus(L"SetVariable", gRT->SetVariable != NULL);
    PrintStatus(L"ResetSystem", gRT->ResetSystem != NULL);
    PrintStatus(L"GetTime", gRT->GetTime != NULL);

    EFI_TIME now;
    EFI_TIME_CAPABILITIES caps;
    if (gRT->GetTime && !EFI_ERROR(gRT->GetTime(&now, &caps)))
    {
        Print(L"  Current Time:      %04u-%02u-%02u %02u:%02u:%02u\n",
              now.Year, now.Month, now.Day, now.Hour, now.Minute, now.Second);
        Print(L"  Clock Caps:        res %u, accuracy %u, sets-zero %u\n",
              caps.Resolution, caps.Accuracy, caps.SetsToZero ? 1 : 0);
    }

    CPanel(L"Boot Services");
    PrintStatus(L"LocateProtocol", gBS->LocateProtocol != NULL);
    PrintStatus(L"GetMemoryMap", gBS->GetMemoryMap != NULL);
    PrintStatus(L"AllocatePool", gBS->AllocatePool != NULL);
    PrintStatus(L"HandleProtocol", gBS->HandleProtocol != NULL);
    PrintStatus(L"OpenProtocol", gBS->OpenProtocol != NULL);

    CPanel(L"UEFI Table Headers");
    PrintTableHeaderInfo(L"System", &gST->Hdr);
    PrintTableHeaderInfo(L"Boot", &gBS->Hdr);
    PrintTableHeaderInfo(L"Runtime", &gRT->Hdr);

    CPanel(L"Loaded Image");
    RD_LOADED_IMAGE* loaded = NULL;
    if (g_ImageHandle && !EFI_ERROR(gBS->HandleProtocol(g_ImageHandle, &RdLoadedImageProtocolGuid, (VOID**)&loaded)) && loaded)
    {
        Print(L"  Image Handle:      0x%08x\n", (UINTN)g_ImageHandle);
        Print(L"  Parent Handle:     0x%08x\n", (UINTN)loaded->ParentHandle);
        Print(L"  Device Handle:     0x%08x\n", (UINTN)loaded->DeviceHandle);
        Print(L"  Image Base:        0x%08x\n", (UINTN)loaded->ImageBase);
        Print(L"  Image Size:        %d KiB\n", (UINTN)(loaded->ImageSize / 1024));
        Print(L"  Code/Data Type:    %s / %s\n",
              MemoryTypeName((UINT32)loaded->ImageCodeType),
              MemoryTypeName((UINT32)loaded->ImageDataType));
        Print(L"  Load Options:      %u bytes\n", loaded->LoadOptionsSize);
        PrintStatus(L"File path pointer", loaded->FilePath != NULL);
    }
    else
    {
        Print(L"  Loaded image protocol is unavailable.\n");
    }

    CPanel(L"Protocols");
    VOID* protocol = NULL;
    PrintStatus(L"Loaded Image Protocol", g_ImageHandle && !EFI_ERROR(gBS->HandleProtocol(g_ImageHandle, &RdLoadedImageProtocolGuid, &protocol)));
    PrintStatus(L"Device Path Protocol", !EFI_ERROR(gBS->LocateProtocol(&RdDevicePathProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Simple File System", !EFI_ERROR(gBS->LocateProtocol(&RdSimpleFileSystemProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Absolute Pointer", !EFI_ERROR(gBS->LocateProtocol(&RdAbsolutePointerProtocolGuid, NULL, &protocol)));
    GOP* gop = NULL;
    BOOLEAN hasGop = LocateGop(&gop) && gop && gop->Mode;
    PrintStatus(L"Graphics Output Protocol", hasGop);

    CPanel(L"Secure Boot Variables");
    UINT8 u8 = 0;
    if (ReadGlobalU8(L"SecureBoot", &u8))
        Print(L"  SecureBoot:        %u\n", u8);
    else
        Print(L"  SecureBoot:        unavailable\n");

    if (ReadGlobalU8(L"SetupMode", &u8))
        Print(L"  SetupMode:         %u\n", u8);
    else
        Print(L"  SetupMode:         unavailable\n");

    if (ReadGlobalU8(L"AuditMode", &u8))
        Print(L"  AuditMode:         %u\n", u8);
    else
        Print(L"  AuditMode:         unavailable\n");

    if (ReadGlobalU8(L"DeployedMode", &u8))
        Print(L"  DeployedMode:      %u\n", u8);
    else
        Print(L"  DeployedMode:      unavailable\n");

    UINT64 u64 = 0;
    if (ReadGlobalU64(L"OsIndicationsSupported", &u64))
        Print(L"  OsIndic Supported: 0x%08x%08x\n", (UINTN)(u64 >> 32), (UINTN)(u64 & 0xFFFFFFFF));
    else
        Print(L"  OsIndic Supported: unavailable\n");

    if (ReadGlobalU64(L"OsIndications", &u64))
        Print(L"  OsIndications:     0x%08x%08x\n", (UINTN)(u64 >> 32), (UINTN)(u64 & 0xFFFFFFFF));
    else
        Print(L"  OsIndications:     unavailable\n");

    UINTN varSize = 0;
    Print(L"  PK:                %s", GlobalVariableExists(L"PK", &varSize) ? L"present" : L"missing");
    if (varSize) Print(L" (%d bytes)", varSize);
    Print(L"\n");
    varSize = 0;
    Print(L"  KEK:               %s", GlobalVariableExists(L"KEK", &varSize) ? L"present" : L"missing");
    if (varSize) Print(L" (%d bytes)", varSize);
    Print(L"\n");
    varSize = 0;
    Print(L"  db:                %s", GlobalVariableExists(L"db", &varSize) ? L"present" : L"missing");
    if (varSize) Print(L" (%d bytes)", varSize);
    Print(L"\n");
    varSize = 0;
    Print(L"  dbx:               %s", GlobalVariableExists(L"dbx", &varSize) ? L"present" : L"missing");
    if (varSize) Print(L" (%d bytes)", varSize);
    Print(L"\n");

    CPanel(L"Boot Variables");
    UINT16 u16 = 0;
    if (ReadGlobalU16(L"BootCurrent", &u16))
        Print(L"  BootCurrent:       Boot%04x\n", u16);
    else
        Print(L"  BootCurrent:       unavailable\n");

    if (ReadGlobalU16(L"BootNext", &u16))
        Print(L"  BootNext:          Boot%04x\n", u16);
    else
        Print(L"  BootNext:          not set\n");

    if (ReadGlobalU16(L"Timeout", &u16))
        Print(L"  Timeout:           %u second(s)\n", u16);
    else
        Print(L"  Timeout:           unavailable\n");

    PrintAsciiVariable(L"PlatformLang");
    PrintAsciiVariable(L"Lang");

    UINT16 bootOrder[64];
    UINTN bootOrderSize = sizeof(bootOrder);
    if (ReadGlobalVariable(L"BootOrder", bootOrder, &bootOrderSize, NULL) && bootOrderSize >= sizeof(UINT16))
    {
        UINTN count = bootOrderSize / sizeof(UINT16);
        Print(L"  BootOrder Count:   %d\n", count);
        Print(L"  BootOrder:         ");
        for (UINTN i = 0; i < count && i < 16; i++)
            Print(L"Boot%04x ", bootOrder[i]);
        if (count > 16)
            Print(L"... ");
        Print(L"\n");

        CPanel(L"Boot Option Records");
        UINTN shown = count < 8 ? count : 8;
        for (UINTN i = 0; i < shown; i++)
            PrintBootOptionSummary(bootOrder[i]);
        if (count > shown)
            Print(L"  ... %d more boot option record(s)\n", count - shown);
    }
    else
    {
        Print(L"  BootOrder:         unavailable\n");
    }

    CPanel(L"Variable Inventory");
    PrintVariableMeta(L"ConIn");
    PrintVariableMeta(L"ConOut");
    PrintVariableMeta(L"ErrOut");
    PrintVariableMeta(L"BootCurrent");
    PrintVariableMeta(L"BootNext");
    PrintVariableMeta(L"BootOrder");
    PrintVariableMeta(L"Timeout");
    PrintVariableMeta(L"PlatformLangCodes");
    PrintVariableMeta(L"LangCodes");

    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

typedef struct
{
    UINT8 Signature[8];
    UINT8 Checksum;
    UINT8 OemId[6];
    UINT8 Revision;
    UINT32 RsdtAddress;
    UINT32 Length;
    UINT64 XsdtAddress;
    UINT8 ExtendedChecksum;
    UINT8 Reserved[3];
} RD_RSDP;

#define RD_RSDP_V2_LENGTH 36

typedef struct
{
    UINT8 Signature[4];
    UINT32 Length;
    UINT8 Revision;
    UINT8 Checksum;
    UINT8 OemId[6];
    UINT8 OemTableId[8];
    UINT32 OemRevision;
    UINT32 CreatorId;
    UINT32 CreatorRevision;
} RD_ACPI_HEADER;

static BOOLEAN SignatureEquals(const UINT8* sig, const char* value)
{
    return sig[0] == (UINT8)value[0]
        && sig[1] == (UINT8)value[1]
        && sig[2] == (UINT8)value[2]
        && sig[3] == (UINT8)value[3];
}

static const CHAR16* AcpiTableName(const UINT8* sig)
{
    if (SignatureEquals(sig, "FACP")) return L"Fixed ACPI Description";
    if (SignatureEquals(sig, "APIC")) return L"Interrupt Controller";
    if (SignatureEquals(sig, "HPET")) return L"High Precision Timer";
    if (SignatureEquals(sig, "MCFG")) return L"PCI Express Config";
    if (SignatureEquals(sig, "SSDT")) return L"Secondary AML";
    if (SignatureEquals(sig, "DSDT")) return L"Differentiated AML";
    if (SignatureEquals(sig, "TPM2")) return L"TPM 2.0";
    if (SignatureEquals(sig, "BGRT")) return L"Boot Graphics";
    if (SignatureEquals(sig, "SLIC")) return L"Licensing";
    if (SignatureEquals(sig, "MSDM")) return L"Microsoft Data";
    if (SignatureEquals(sig, "WAET")) return L"Windows Event Timer";
    if (SignatureEquals(sig, "SRAT")) return L"NUMA Affinity";
    if (SignatureEquals(sig, "SLIT")) return L"NUMA Distance";
    if (SignatureEquals(sig, "DMAR")) return L"DMA Remapping";
    if (SignatureEquals(sig, "IVRS")) return L"I/O Virtualization";
    return L"Generic ACPI";
}

static RD_ACPI_HEADER* BrowserAcpiRoot(BOOLEAN* xsdt)
{
    VOID* rsdpTable = NULL;
    *xsdt = FALSE;
    if (!HasConfigTable(&RdAcpi2TableGuid, &rsdpTable))
        HasConfigTable(&RdAcpiTableGuid, &rsdpTable);
    if (!rsdpTable) return NULL;

    RD_RSDP* rsdp = (RD_RSDP*)rsdpTable;
    RD_ACPI_HEADER* root = NULL;
    if (rsdp->Revision >= 2 && rsdp->XsdtAddress)
    {
        root = (RD_ACPI_HEADER*)(UINTN)rsdp->XsdtAddress;
        *xsdt = TRUE;
    }
    else if (rsdp->RsdtAddress)
    {
        root = (RD_ACPI_HEADER*)(UINTN)rsdp->RsdtAddress;
    }
    if (!root || root->Length < sizeof(RD_ACPI_HEADER))
        return NULL;
    return root;
}

static UINTN BrowserAcpiCount(void)
{
    BOOLEAN xsdt = FALSE;
    RD_ACPI_HEADER* root = BrowserAcpiRoot(&xsdt);
    if (!root) return 0;
    UINTN entrySize = xsdt ? sizeof(UINT64) : sizeof(UINT32);
    UINTN count = (root->Length - sizeof(RD_ACPI_HEADER)) / entrySize;
    if (count > 64) count = 64;
    return count;
}

static RD_ACPI_HEADER* BrowserAcpiTableAt(UINTN index)
{
    BOOLEAN xsdt = FALSE;
    RD_ACPI_HEADER* root = BrowserAcpiRoot(&xsdt);
    if (!root) return NULL;
    UINTN entrySize = xsdt ? sizeof(UINT64) : sizeof(UINT32);
    UINTN count = (root->Length - sizeof(RD_ACPI_HEADER)) / entrySize;
    if (index >= count || index >= 64) return NULL;
    UINT8* entries = ((UINT8*)root) + sizeof(RD_ACPI_HEADER);
    UINTN address = xsdt ? (UINTN)ReadLe64(entries + index * entrySize) : (UINTN)ReadLe32(entries + index * entrySize);
    RD_ACPI_HEADER* header = (RD_ACPI_HEADER*)address;
    if (!header || header->Length < sizeof(RD_ACPI_HEADER))
        return NULL;
    return header;
}

void DisplayAcpiTables(void)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - ACPI Tables");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    VOID* rsdpTable = NULL;
    BOOLEAN acpi2 = HasConfigTable(&RdAcpi2TableGuid, &rsdpTable);
    if (!acpi2)
        HasConfigTable(&RdAcpiTableGuid, &rsdpTable);

    if (!rsdpTable)
    {
        SetColor(COLOR_WARN);
        Print(L"  ACPI root pointer was not exposed through UEFI config tables.\n");
        SetColor(COLOR_NORMAL);
        WaitForAnyKey();
        return;
    }

    RD_RSDP* rsdp = (RD_RSDP*)rsdpTable;

    CPanel(L"Root Pointer");
    Print(L"  Address:           0x%08x\n", (UINTN)rsdp);
    Print(L"  Config Source:     %s\n", acpi2 ? L"ACPI 2.0" : L"ACPI 1.0");
    PrintAsciiFixed(L"Signature:         ", rsdp->Signature, 8);
    PrintAsciiFixed(L"OEM ID:            ", rsdp->OemId, 6);
    Print(L"  Revision:          %u\n", rsdp->Revision);
    Print(L"  RSDT Address:      0x%08x\n", (UINTN)rsdp->RsdtAddress);
    if (rsdp->Revision >= 2)
    {
        Print(L"  Length:            %u bytes\n", rsdp->Length);
        Print(L"  XSDT Address:      0x%08x\n", (UINTN)rsdp->XsdtAddress);
    }
    PrintStatus(L"RSDP checksum", ByteSum((const UINT8*)rsdp, 20) == 0);
    if (rsdp->Revision >= 2 && rsdp->Length >= RD_RSDP_V2_LENGTH)
        PrintStatus(L"RSDP extended checksum", ByteSum((const UINT8*)rsdp, rsdp->Length) == 0);

    RD_ACPI_HEADER* root = NULL;
    BOOLEAN xsdt = FALSE;
    if (rsdp->Revision >= 2 && rsdp->XsdtAddress)
    {
        root = (RD_ACPI_HEADER*)(UINTN)rsdp->XsdtAddress;
        xsdt = TRUE;
    }
    else if (rsdp->RsdtAddress)
    {
        root = (RD_ACPI_HEADER*)(UINTN)rsdp->RsdtAddress;
    }

    if (!root || root->Length < sizeof(RD_ACPI_HEADER))
    {
        SetColor(COLOR_WARN);
        Print(L"\n  ACPI root table address is invalid.\n");
        SetColor(COLOR_NORMAL);
        WaitForAnyKey();
        return;
    }

    CPanel(xsdt ? L"XSDT" : L"RSDT");
    PrintAsciiFixed(L"Signature:         ", root->Signature, 4);
    Print(L"  Revision:          %u\n", root->Revision);
    Print(L"  Length:            %u bytes\n", root->Length);
    PrintStatus(L"Table checksum", ByteSum((const UINT8*)root, root->Length) == 0);
    PrintAsciiFixed(L"OEM ID:            ", root->OemId, 6);
    PrintAsciiFixed(L"OEM Table ID:      ", root->OemTableId, 8);
    Print(L"  OEM Revision:      0x%08x\n", root->OemRevision);
    Print(L"  Creator ID:        ");
    PrintAsciiInline((const UINT8*)&root->CreatorId, 4);
    Print(L"  rev 0x%08x\n", root->CreatorRevision);

    CPanel(L"ACPI Directory");
    UINTN entrySize = xsdt ? sizeof(UINT64) : sizeof(UINT32);
    UINTN count = (root->Length - sizeof(RD_ACPI_HEADER)) / entrySize;
    UINT8* entries = ((UINT8*)root) + sizeof(RD_ACPI_HEADER);

    Print(L"  Entries:           %d\n", count);
    for (UINTN i = 0; i < count && i < 64; i++)
    {
        UINTN address = xsdt ? (UINTN)ReadLe64(entries + i * entrySize) : (UINTN)ReadLe32(entries + i * entrySize);
        RD_ACPI_HEADER* header = (RD_ACPI_HEADER*)address;

        if (!header || header->Length < sizeof(RD_ACPI_HEADER))
        {
            Print(L"  %02d  invalid table pointer 0x%08x\n", i, address);
            continue;
        }

        Print(L"  %02d  %c%c%c%c  rev %u  len %u  %s  checksum %s  0x%08x\n",
              i,
              (CHAR16)header->Signature[0], (CHAR16)header->Signature[1],
              (CHAR16)header->Signature[2], (CHAR16)header->Signature[3],
              header->Revision, header->Length, AcpiTableName(header->Signature),
              ByteSum((const UINT8*)header, header->Length) == 0 ? L"OK" : L"BAD",
              address);
        Print(L"      OEM ");
        PrintAsciiInline(header->OemId, 6);
        Print(L"  Table ");
        PrintAsciiInline(header->OemTableId, 8);
        Print(L"  Creator ");
        PrintAsciiInline((const UINT8*)&header->CreatorId, 4);
        Print(L"  rev 0x%08x\n", header->CreatorRevision);
        if (!PgCheck(1)) break;
    }

    if (count > 64)
        Print(L"  ... %d more ACPI table(s)\n", count - 64);

    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

typedef enum
{
    BROWSER_NONE,
    BROWSER_PREV_TOOL,
    BROWSER_NEXT_TOOL,
    BROWSER_UP,
    BROWSER_DOWN,
    BROWSER_PAGE_UP,
    BROWSER_PAGE_DOWN,
    BROWSER_HOME,
    BROWSER_END,
    BROWSER_SELECT,
    BROWSER_EXIT
} BrowserAction;

#define BROWSER_TOOL_COUNT 11

static UINTN BrowserRows(void)
{
    UINTN cols = 80;
    UINTN rows = 25;

    if (gST->ConOut && gST->ConOut->Mode)
        gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &cols, &rows);

    if (rows > 22)
        return rows - 13;

    return rows > 15 ? rows - 8 : 7;
}

static const CHAR16* BrowserTitle(UINTN tool)
{
    switch (tool)
    {
    case 0:
        return L"Live Dashboard";
    case 1:
        return L"SMBIOS Table Navigator";
    case 2:
        return L"SMBIOS Type Coverage";
    case 3:
        return L"UEFI Memory Pages";
    case 4:
        return L"ACPI Directory Navigator";
    case 5:
        return L"Boot Variable Records";
    case 6:
        return L"Protocol & Input Matrix";
    case 7:
        return L"Storage / Block I/O Devices";
    case 8:
        return L"PCI Express Device Map";
    case 9:
        return L"Network Interfaces (SNP)";
    case 10:
        return L"TPM & Security";
    default:
        return L"Analysis Browser";
    }
}

static const CHAR16* SmbiosTypeName(UINT8 type)
{
    switch (type)
    {
    case SMBIOS_TYPE_BIOS_INFORMATION: return L"BIOS";
    case SMBIOS_TYPE_SYSTEM_INFORMATION: return L"System";
    case SMBIOS_TYPE_BASEBOARD_INFORMATION: return L"Baseboard";
    case SMBIOS_TYPE_SYSTEM_ENCLOSURE: return L"Chassis";
    case SMBIOS_TYPE_PROCESSOR_INFORMATION: return L"Processor";
    case SMBIOS_TYPE_CACHE_INFORMATION: return L"Cache";
    case SMBIOS_TYPE_PORT_CONNECTOR_INFORMATION: return L"Port";
    case SMBIOS_TYPE_SYSTEM_SLOTS: return L"Slot";
    case SMBIOS_TYPE_ONBOARD_DEVICE_INFORMATION: return L"Onboard";
    case SMBIOS_TYPE_OEM_STRINGS: return L"OEM Strings";
    case SMBIOS_TYPE_SYSTEM_CONFIGURATION_OPTIONS: return L"Config Options";
    case SMBIOS_TYPE_BIOS_LANGUAGE_INFORMATION: return L"BIOS Language";
    case SMBIOS_TYPE_SYSTEM_EVENT_LOG: return L"Event Log";
    case SMBIOS_TYPE_PHYSICAL_MEMORY_ARRAY: return L"Memory Array";
    case SMBIOS_TYPE_MEMORY_DEVICE: return L"Memory Device";
    case SMBIOS_TYPE_MEMORY_ARRAY_MAPPED_ADDRESS: return L"Array Map";
    case SMBIOS_TYPE_MEMORY_DEVICE_MAPPED_ADDRESS: return L"Device Map";
    case SMBIOS_TYPE_PORTABLE_BATTERY: return L"Battery";
    case SMBIOS_TYPE_SYSTEM_RESET: return L"Reset";
    case SMBIOS_TYPE_HARDWARE_SECURITY: return L"Security";
    case SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION: return L"Boot Info";
    case SMBIOS_TYPE_IPMI_DEVICE_INFORMATION: return L"IPMI";
    case SMBIOS_TYPE_SYSTEM_POWER_SUPPLY: return L"Power Supply";
    case SMBIOS_TYPE_ONBOARD_DEVICES_EXTENDED_INFORMATION: return L"Onboard Ext";
    case SMBIOS_TYPE_MANAGEMENT_CONTROLLER_HOST_INTERFACE: return L"Host Interface";
    case SMBIOS_TYPE_TPM_DEVICE: return L"TPM";
    case SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION: return L"CPU Addl";
    default: return L"Generic";
    }
}

static UINTN SmbiosTableCount(const SMBIOS_STRUCTURE_TABLE* entry)
{
    return CountSmbiosTables(entry, NULL, 0);
}

static SMBIOS_STRUCTURE_POINTER SmbiosTableAt(const SMBIOS_STRUCTURE_TABLE* entry, UINTN index)
{
    SMBIOS_STRUCTURE_POINTER table;
    table.Raw = NULL;

    if (!entry || !entry->TableAddress)
        return table;

    table.Raw = (UINT8*)((UINTN)entry->TableAddress);
    SmbiosSetActiveTableBounds(entry);

    UINTN current = 0;
    UINTN remaining = SmbiosBytesRemaining(entry, table);
    while (table.Raw && remaining >= 4 && table.Hdr->Type != SMBIOS_TYPE_END_OF_TABLE && current < 512)
    {
        if (current == index)
            return table;

        UINTN length = SafeTableLength(table);
        if (!length || length > remaining)
            break;

        table.Raw += length;
        remaining = SmbiosBytesRemaining(entry, table);
        current++;
    }

    table.Raw = NULL;
    return table;
}

static UINTN SmbiosStringCount(SMBIOS_STRUCTURE_POINTER table)
{
    UINTN length = SafeTableLength(table);
    UINTN count = 0;

    if (!length || length <= table.Hdr->Length)
        return 0;

    const char* text = (const char*)(table.Raw + table.Hdr->Length);
    const char* end = (const char*)(table.Raw + length);

    while (text < end && *text)
    {
        count++;
        while (text < end && *text)
            text++;
        text++;
    }

    return count;
}

static BOOLEAN AllocateMemoryMapSnapshot(EFI_MEMORY_DESCRIPTOR** map, UINTN* mapSize, UINTN* descSize, UINT32* descVersion)
{
    UINTN key = 0;
    EFI_STATUS status;

    *map = NULL;
    *mapSize = 0;
    *descSize = 0;
    *descVersion = 0;

    status = gBS->GetMemoryMap(mapSize, NULL, &key, descSize, descVersion);
    if (status != EFI_BUFFER_TOO_SMALL && EFI_ERROR(status))
        return FALSE;

    if (*descSize == 0)
        *descSize = sizeof(EFI_MEMORY_DESCRIPTOR);

    UINTN bufferSize = *mapSize + (*descSize * 8);
    status = gBS->AllocatePool(EfiLoaderData, bufferSize, (VOID**)map);
    if (EFI_ERROR(status) || !*map)
        return FALSE;

    *mapSize = bufferSize;
    status = gBS->GetMemoryMap(mapSize, *map, &key, descSize, descVersion);
    if (EFI_ERROR(status))
    {
        gBS->FreePool(*map);
        *map = NULL;
        return FALSE;
    }

    return TRUE;
}

static UINTN BootOrderCount(void)
{
    UINT16 bootOrder[96];
    UINTN size = sizeof(bootOrder);

    if (ReadGlobalVariable(L"BootOrder", bootOrder, &size, NULL) && size >= sizeof(UINT16))
        return size / sizeof(UINT16);

    return 0;
}

static UINTN BrowserMaxOffset(UINTN tool, const SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN rows = BrowserRows();

    switch (tool)
    {
    case 1:
    {
        UINTN count = SmbiosTableCount(entry);
        return count > rows ? count - rows : 0;
    }
    case 2:
        return (SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION + 1) > rows
             ? (SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION + 1) - rows
             : 0;
    case 3:
    {
        EFI_MEMORY_DESCRIPTOR* map = NULL;
        UINTN mapSize = 0;
        UINTN descSize = 0;
        UINT32 descVersion = 0;
        UINTN count = 0;

        if (AllocateMemoryMapSnapshot(&map, &mapSize, &descSize, &descVersion))
        {
            count = descSize ? mapSize / descSize : 0;
            gBS->FreePool(map);
        }
        return count > rows ? count - rows : 0;
    }
    case 4:
    {
        UINTN count = BrowserAcpiCount();
        return count > rows ? count - rows : 0;
    }
    case 5:
    {
        UINTN count = BootOrderCount();
        return count > rows ? count - rows : 0;
    }
    case 7:
    {
        UINTN handleCount = 0;
        EFI_HANDLE* handles = NULL;
        UINTN count = 0;
        if (!EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, &RdBlockIoProtocolGuid, NULL, &handleCount, &handles)))
        {
            count = handleCount;
            gBS->FreePool(handles);
        }
        return count > rows ? count - rows : 0;
    }
    case 8:
    {
        UINTN devs = 0;
        VOID* mcfg = NULL;
        RD_ACPI_HEADER* hdr = NULL;
        BOOLEAN xsdt = FALSE;
        RD_ACPI_HEADER* root = BrowserAcpiRoot(&xsdt);
        if (!root) return 0;
        UINTN entrySize = xsdt ? 8 : 4;
        UINTN acpiCount = (root->Length - sizeof(RD_ACPI_HEADER)) / entrySize;
        UINT8* entries = ((UINT8*)root) + sizeof(RD_ACPI_HEADER);
        for (UINTN i = 0; i < acpiCount; i++)
        {
            UINTN addr = xsdt ? (UINTN)ReadLe64(entries + i * 8) : (UINTN)ReadLe32(entries + i * 4);
            hdr = (RD_ACPI_HEADER*)addr;
            if (hdr && hdr->Length >= sizeof(RD_ACPI_HEADER) && SignatureEquals(hdr->Signature, "MCFG"))
            { mcfg = hdr; break; }
        }
        if (!mcfg) return 0;
        UINTN mcfgLen = hdr->Length;
        UINTN allocCount = (mcfgLen - sizeof(RD_ACPI_HEADER)) / 16;
        for (UINTN a = 0; a < allocCount && devs < 512; a++)
        {
            UINT8* alloc = (UINT8*)mcfg + sizeof(RD_ACPI_HEADER) + a * 16;
            UINT8 startBus = alloc[8];
            UINT8 endBus = alloc[9];
            for (UINTN b = startBus; b <= endBus && devs < 512; b++)
                for (UINTN d = 0; d < 32 && devs < 512; d++)
                    for (UINTN f = 0; f < 8 && devs < 512; f++)
                        devs++;
        }
        if (devs < 16) devs = 0;
        return devs > rows ? devs - rows : 0;
    }
    default:
        return 0;
    }
}

static void BrowserHeader(UINTN tool, UINTN offset, UINTN maxOffset)
{
    gST->ConOut->ClearScreen(gST->ConOut);

    SetColor(COLOR_TITLE);
    Print(L"\n  Rainbow Dragon - Interactive Analysis Browser\n");
    Rule();
    SetColor(COLOR_OK);
    Print(L"  Created by Leksa667 - Netari\n");
    SetColor(COLOR_SELECTED);
    Print(L"  %u/%u  %s", tool + 1, BROWSER_TOOL_COUNT, BrowserTitle(tool));
    if (maxOffset)
        Print(L"  page %u/%u", offset + 1, maxOffset + 1);
    Print(L"\n");
    SetColor(COLOR_NORMAL);
}

static void BrowserFooter(void)
{
    Print(L"\n");
    SetColor(COLOR_DIM);
    Print(L"  Left/Right: tools   Up/Down: scroll   PgUp/PgDn: page   Home/End   Enter: select   Q/Esc: back\n");
    SetColor(COLOR_NORMAL);
}

static BrowserAction ReadBrowserAction(void)
{
    UINTN index = 0;

    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &index);

    EFI_INPUT_KEY key;
    if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &key)))
        return BROWSER_NONE;

    if (key.UnicodeChar == L'q' || key.UnicodeChar == L'Q')
        return BROWSER_EXIT;
    if (key.UnicodeChar == L'a' || key.UnicodeChar == L'A')
        return BROWSER_PREV_TOOL;
    if (key.UnicodeChar == L'\r')
        return BROWSER_SELECT;
    if (key.UnicodeChar == L'd' || key.UnicodeChar == L'D')
        return BROWSER_NEXT_TOOL;
    if (key.UnicodeChar == L'w' || key.UnicodeChar == L'W')
        return BROWSER_UP;
    if (key.UnicodeChar == L's' || key.UnicodeChar == L'S')
        return BROWSER_DOWN;

    switch (key.ScanCode)
    {
    case RD_SCAN_LEFT: return BROWSER_PREV_TOOL;
    case RD_SCAN_RIGHT: return BROWSER_NEXT_TOOL;
    case RD_SCAN_UP: return BROWSER_UP;
    case RD_SCAN_DOWN: return BROWSER_DOWN;
    case RD_SCAN_PAGE_UP: return BROWSER_PAGE_UP;
    case RD_SCAN_PAGE_DOWN: return BROWSER_PAGE_DOWN;
    case RD_SCAN_HOME: return BROWSER_HOME;
    case RD_SCAN_END: return BROWSER_END;
    case RD_SCAN_ESC: return BROWSER_EXIT;
    default: return BROWSER_NONE;
    }
}

static BOOLEAN IsDefaultString(const char* s)
{
    if (!s) return FALSE;
    static const char* DEFAULTS[] = {
        "To Be Filled", "Default string", "O.E.M.", "None", "Unknown", "Empty", "Serial"
    };
    for (UINTN i = 0; i < sizeof(DEFAULTS)/sizeof(DEFAULTS[0]); i++)
    {
        const char* d = DEFAULTS[i];
        const char* p = s;
        while (*p)
        {
            const char* p1 = p;
            const char* d1 = d;
            while (*p1 && *d1 && ((*p1 == *d1) || (*p1 + 32 == *d1) || (*p1 - 32 == *d1)))
            { p1++; d1++; }
            if (!*d1) return TRUE;
            p++;
        }
    }
    return FALSE;
}

static void RenderBrowserDashboard(const SMBIOS_STRUCTURE_TABLE* entry)
{
    Panel(L"Core State");
    PrintStatus(L"UEFI system table", gST != NULL);
    PrintStatus(L"Boot services", gBS != NULL);
    PrintStatus(L"Runtime services", gRT != NULL);
    PrintStatus(L"SMBIOS table", entry && entry->TableAddress);

    GOP* gop = NULL;
    if (LocateGop(&gop) && gop->Mode && gop->Mode->Info)
        Print(L"  GOP:               %ux%u, %s\n", gop->Mode->Info->HRes, gop->Mode->Info->VRes, PixelFormatName(gop->Mode->Info->Fmt));
    else
        PrintStatus(L"Graphics Output Protocol", FALSE);

    VOID* protocol = NULL;
    PrintStatus(L"Absolute Pointer", !EFI_ERROR(gBS->LocateProtocol(&RdAbsolutePointerProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Simple File System", !EFI_ERROR(gBS->LocateProtocol(&RdSimpleFileSystemProtocolGuid, NULL, &protocol)));

    Panel(L"Quick Counts");
    if (entry && entry->TableAddress)
    {
        UINTN counts[SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION + 1];
        gBS->SetMem(counts, sizeof(counts), 0);
        UINTN total = CountSmbiosTables(entry, counts, sizeof(counts) / sizeof(counts[0]));
        Print(L"  SMBIOS tables:     %d\n", total);
        Print(L"  CPU/RAM/Slots:     %d / %d / %d\n",
              counts[SMBIOS_TYPE_PROCESSOR_INFORMATION],
              counts[SMBIOS_TYPE_MEMORY_DEVICE],
              counts[SMBIOS_TYPE_SYSTEM_SLOTS]);
        Print(L"  ACPI tables:       %d\n", BrowserAcpiCount());
        Print(L"  Boot options:      %d\n", BootOrderCount());
    }
    else
    {
        Print(L"  SMBIOS counts unavailable.\n");
    }

    Panel(L"Spoofing Analysis");
    BOOLEAN dirty = FALSE;
    if (entry && entry->TableAddress)
    {
        for (UINTN i = 0; i < 32; i++)
        {
            SMBIOS_STRUCTURE_POINTER t = SmbiosTableAt(entry, i);
            if (!t.Raw) break;
            for (UINT8 s = 1; s <= SmbiosStringCount(t); s++)
            {
                const char* str = GetStringAtIndex(t, s);
                if (IsDefaultString(str)) { dirty = TRUE; break; }
            }
            if (dirty) break;
        }
    }
    PrintStatus(L"Default SMBIOS strings detected", dirty);
    
    UINTN sz = sizeof(UINT8);
    UINT8 val = 0;
    if (ReadGlobalVariable(L"SecureBoot", &val, &sz, NULL))
        PrintStatus(L"Secure Boot Active", val != 0);
    else
        PrintStatus(L"Secure Boot State Unknown", TRUE);
}

static void RenderSmbiosTableBrowser(const SMBIOS_STRUCTURE_TABLE* entry, UINTN offset)
{
    Panel(L"Table Pages");

    if (!entry || !entry->TableAddress)
    {
        Print(L"  SMBIOS table is unavailable.\n");
        return;
    }

    UINTN rows = BrowserRows();
    UINTN total = SmbiosTableCount(entry);
    Print(L"  Table bytes:       %d\n", SmbiosEntryTableLength(entry));
    Print(L"  Showing:           %d to %d of %d\n", offset + 1, offset + rows < total ? offset + rows : total, total);

    for (UINTN i = 0; i < rows; i++)
    {
        UINTN index = offset + i;
        SMBIOS_STRUCTURE_POINTER table = SmbiosTableAt(entry, index);
        if (!table.Raw)
            break;

        UINTN fullLength = SafeTableLength(table);
        Print(L"  %03d  type %02u  %s  handle 0x%04x  hdr %u  full %u  str %u  0x%08x\n",
              index,
              table.Hdr->Type,
              SmbiosTypeName(table.Hdr->Type),
              ReadU16(table, 0x02),
              table.Hdr->Length,
              fullLength,
              SmbiosStringCount(table),
              (UINTN)table.Raw);
    }
}

static void RenderSmbiosCoverage(const SMBIOS_STRUCTURE_TABLE* entry, UINTN offset)
{
    Panel(L"Type Coverage");

    if (!entry || !entry->TableAddress)
    {
        Print(L"  SMBIOS table is unavailable.\n");
        return;
    }

    UINTN counts[SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION + 1];
    gBS->SetMem(counts, sizeof(counts), 0);
    CountSmbiosTables(entry, counts, sizeof(counts) / sizeof(counts[0]));

    UINTN rows = BrowserRows();
    for (UINTN i = 0; i < rows; i++)
    {
        UINTN type = offset + i;
        if (type > SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION)
            break;

        SetColor(counts[type] ? COLOR_OK : COLOR_DIM);
        Print(L"  Type %02u  %s  %d table(s)\n", type, SmbiosTypeName((UINT8)type), counts[type]);
        SetColor(COLOR_NORMAL);
    }
}

static void RenderMemoryBrowser(UINTN offset)
{
    EFI_MEMORY_DESCRIPTOR* map = NULL;
    UINTN mapSize = 0;
    UINTN descSize = 0;
    UINT32 descVersion = 0;

    Panel(L"Memory Descriptor Pages");

    if (!AllocateMemoryMapSnapshot(&map, &mapSize, &descSize, &descVersion))
    {
        Print(L"  Unable to read memory map.\n");
        return;
    }

    UINTN descriptorCount = descSize ? mapSize / descSize : 0;
    UINTN rows = BrowserRows();
    UINT64 totalPages = 0;
    for (UINTN i = 0; i < descriptorCount; i++)
    {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + i * descSize);
        totalPages += desc->NumberOfPages;
    }

    Print(L"  Descriptors:       %d, size %d, version %u\n", descriptorCount, descSize, descVersion);
    Print(L"  Total mapped:      %d MiB\n", PagesToMiB(totalPages));

    for (UINTN i = 0; i < rows; i++)
    {
        UINTN index = offset + i;
        if (index >= descriptorCount)
            break;

        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)map + index * descSize);
        UINT64 end = desc->PhysicalStart + desc->NumberOfPages * 4096;
        Print(L"  %03d  %s  0x%08x - 0x%08x  %d MiB  attr 0x%08x\n",
              index, MemoryTypeName(desc->Type), (UINTN)desc->PhysicalStart,
              (UINTN)end, PagesToMiB(desc->NumberOfPages), (UINTN)desc->Attribute);
    }

    gBS->FreePool(map);
}

static void RenderAcpiBrowser(UINTN offset, UINTN cursor)
{
    Panel(L"ACPI Table Pages");

    UINTN total = BrowserAcpiCount();
    if (!total)
    {
        Print(L"  ACPI directory is unavailable.\n");
        return;
    }

    UINTN rows = BrowserRows();
    Print(L"  Showing:           %d to %d of %d\n", offset + 1, offset + rows < total ? offset + rows : total, total);

    for (UINTN i = 0; i < rows; i++)
    {
        UINTN index = offset + i;
        RD_ACPI_HEADER* header = BrowserAcpiTableAt(index);
        if (!header)
            break;

        if (i == cursor)
            SetColor(COLOR_SELECTED);

        Print(L"  %03d  %c%c%c%c  rev %u  len %u  checksum %s  %s\n",
              index,
              (CHAR16)header->Signature[0], (CHAR16)header->Signature[1],
              (CHAR16)header->Signature[2], (CHAR16)header->Signature[3],
              header->Revision,
              header->Length,
              ByteSum((const UINT8*)header, header->Length) == 0 ? L"OK" : L"BAD",
              AcpiTableName(header->Signature));
        Print(L"       OEM ");
        PrintAsciiInline(header->OemId, 6);
        Print(L"  Table ");
        PrintAsciiInline(header->OemTableId, 8);
        Print(L"  Creator ");
        PrintAsciiInline((const UINT8*)&header->CreatorId, 4);
        Print(L"\n");

        if (i == cursor)
            SetColor(COLOR_NORMAL);
    }
}

static void RenderBootVariableBrowser(UINTN offset)
{
    UINT16 bootOrder[96];
    UINTN size = sizeof(bootOrder);
    UINTN rows = BrowserRows();

    Panel(L"Boot Variables");
    UINT16 u16 = 0;
    if (ReadGlobalU16(L"BootCurrent", &u16))
        Print(L"  BootCurrent:       Boot%04x\n", u16);
    if (ReadGlobalU16(L"BootNext", &u16))
        Print(L"  BootNext:          Boot%04x\n", u16);
    if (ReadGlobalU16(L"Timeout", &u16))
        Print(L"  Timeout:           %u second(s)\n", u16);

    if (!ReadGlobalVariable(L"BootOrder", bootOrder, &size, NULL) || size < sizeof(UINT16))
    {
        Print(L"  BootOrder unavailable.\n");
        PrintVariableMeta(L"ConIn");
        PrintVariableMeta(L"ConOut");
        PrintVariableMeta(L"ErrOut");
        return;
    }

    UINTN count = size / sizeof(UINT16);
    Print(L"  BootOrder Count:   %d\n", count);
    for (UINTN i = 0; i < rows; i++)
    {
        UINTN index = offset + i;
        if (index >= count)
            break;
        PrintBootOptionSummary(bootOrder[index]);
    }
}

static void RenderProtocolBrowser(void)
{
    Panel(L"Protocols");

    VOID* protocol = NULL;
    PrintStatus(L"Loaded Image", g_ImageHandle && !EFI_ERROR(gBS->HandleProtocol(g_ImageHandle, &RdLoadedImageProtocolGuid, &protocol)));
    PrintStatus(L"Device Path", !EFI_ERROR(gBS->LocateProtocol(&RdDevicePathProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Simple File System", !EFI_ERROR(gBS->LocateProtocol(&RdSimpleFileSystemProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Absolute Pointer", !EFI_ERROR(gBS->LocateProtocol(&RdAbsolutePointerProtocolGuid, NULL, &protocol)));

    GOP* gop = NULL;
    if (LocateGop(&gop) && gop->Mode && gop->Mode->Info)
    {
        PrintStatus(L"Graphics Output Protocol", TRUE);
        Print(L"  GOP Mode:          %u / %u\n", gop->Mode->Mode, gop->Mode->Max);
        Print(L"  Resolution:        %ux%u\n", gop->Mode->Info->HRes, gop->Mode->Info->VRes);
    }
    else
    {
        PrintStatus(L"Graphics Output Protocol", FALSE);
    }

    Panel(L"Variables");
    PrintVariableMeta(L"SecureBoot");
    PrintVariableMeta(L"SetupMode");
    PrintVariableMeta(L"BootOrder");
    PrintVariableMeta(L"PlatformLang");
    PrintVariableMeta(L"Lang");
}

static void RenderStorageBrowser(UINTN offset);
static void RenderPciBrowser(UINTN offset);
static void RenderNetworkBrowser(void);
static void RenderTpmSecurityBrowser(const SMBIOS_STRUCTURE_TABLE* entry);

static void RenderBrowserTool(const SMBIOS_STRUCTURE_TABLE* entry, UINTN tool, UINTN offset, UINTN maxOffset, UINTN cursor)
{
    BrowserHeader(tool, offset, maxOffset);

    switch (tool)
    {
    case 0:
        RenderBrowserDashboard(entry);
        break;
    case 1:
        RenderSmbiosTableBrowser(entry, offset);
        break;
    case 2:
        RenderSmbiosCoverage(entry, offset);
        break;
    case 3:
        RenderMemoryBrowser(offset);
        break;
    case 4:
        RenderAcpiBrowser(offset, cursor);
        break;
    case 5:
        RenderBootVariableBrowser(offset);
        break;
    case 6:
        RenderProtocolBrowser();
        break;
    case 7:
        RenderStorageBrowser(offset);
        break;
    case 8:
        RenderPciBrowser(offset);
        break;
    case 9:
        RenderNetworkBrowser();
        break;
    case 10:
        RenderTpmSecurityBrowser(entry);
        break;
    default:
        break;
    }

    BrowserFooter();
}

static void RenderStorageBrowser(UINTN offset)
{
    Panel(L"Storage / Block I/O Devices");

    UINTN handleCount = 0;
    EFI_HANDLE* handles = NULL;
    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, &RdBlockIoProtocolGuid, NULL, &handleCount, &handles)) || !handleCount)
    {
        Print(L"  No Block I/O handles found.\n");
        return;
    }

    UINTN rows = BrowserRows();
    Print(L"  Total Block I/O Handles: %d\n", handleCount);

    for (UINTN i = 0; i < rows; i++)
    {
        UINTN index = offset + i;
        if (index >= handleCount) break;

        RD_BLOCK_IO* bio = NULL;
        if (EFI_ERROR(gBS->HandleProtocol(handles[index], &RdBlockIoProtocolGuid, (VOID**)&bio)) || !bio || !bio->Media)
        {
            Print(L"  %03d  protocol error\n", index);
            continue;
        }

        RD_BLOCK_IO_MEDIA* m = bio->Media;
        UINTN miB = (UINTN)((m->LastBlock + 1) * m->BlockSize / (1024 * 1024));
        Print(L"  %03d  %s  %s  %s  %s  %d MiB  blk %u  id %u\n",
              index,
              m->RemovableMedia ? L"REM" : L"FIX",
              m->MediaPresent ? L"PRES" : L"---",
              m->ReadOnly ? L"RO" : L"RW",
              m->LogicalPartition ? L"PART" : L"RAW",
              miB, m->BlockSize, (UINTN)m->MediaId);
    }

    gBS->FreePool(handles);
}

static const CHAR16* PciClassName(UINT8 cls, UINT8 sub)
{
    (VOID)sub;
    switch (cls)
    {
    case 0x01: return L"Mass Storage";
    case 0x02: return L"Network";
    case 0x03: return L"Display";
    case 0x04: return L"Multimedia";
    case 0x06: return L"Bridge";
    case 0x0C: return L"Serial Bus";
    default: return L"Other";
    }
}

static void RenderPciBrowser(UINTN offset)
{
    Panel(L"PCI Express Devices");

    VOID* mcfg = NULL;
    RD_ACPI_HEADER* hdr = NULL;
    BOOLEAN xsdt = FALSE;
    RD_ACPI_HEADER* root = BrowserAcpiRoot(&xsdt);
    if (!root)
    {
        Print(L"  ACPI root unavailable.\n");
        return;
    }

    UINTN entrySize = xsdt ? 8 : 4;
    UINTN acpiCount = (root->Length - sizeof(RD_ACPI_HEADER)) / entrySize;
    UINT8* entries = ((UINT8*)root) + sizeof(RD_ACPI_HEADER);
    for (UINTN i = 0; i < acpiCount; i++)
    {
        UINTN addr = xsdt ? (UINTN)ReadLe64(entries + i * 8) : (UINTN)ReadLe32(entries + i * 4);
        hdr = (RD_ACPI_HEADER*)addr;
        if (hdr && hdr->Length >= sizeof(RD_ACPI_HEADER) && SignatureEquals(hdr->Signature, "MCFG"))
        { mcfg = hdr; break; }
    }

    if (!mcfg)
    {
        Print(L"  MCFG table not found. PCIe config space unavailable.\n");
        return;
    }

    UINTN mcfgLen = hdr->Length;
    UINTN allocCount = (mcfgLen - sizeof(RD_ACPI_HEADER)) / 16;
    UINTN shown = 0;
    UINTN rows = BrowserRows();
    UINTN skipped = 0;

    for (UINTN a = 0; a < allocCount; a++)
    {
        UINT8* alloc = (UINT8*)mcfg + sizeof(RD_ACPI_HEADER) + a * 16;
        UINT64 baseAddr = ReadLe64(alloc);
        UINT8 startBus = alloc[8];
        UINT8 endBus = alloc[9];

        for (UINTN b = startBus; b <= endBus; b++)
        {
            for (UINTN d = 0; d < 32; d++)
            {
                for (UINTN f = 0; f < 8; f++)
                {
                    UINTN addr = (UINTN)baseAddr + ((b - startBus) << 20) + (d << 15) + (f << 12);
                    UINT16 vid = ReadLe16((UINT8*)addr);
                    if (vid == 0xFFFF || vid == 0x0000) continue;

                    if (skipped < offset)
                    {
                        skipped++;
                        continue;
                    }

                    if (shown >= rows) break;

                    UINT16 did = ReadLe16((UINT8*)addr + 2);
                    UINT8 cls = ((UINT8*)addr)[0x0B];
                    UINT8 sub = ((UINT8*)addr)[0x0A];

                    Print(L"  %02x:%02x.%x  %04x:%04x  cls %02x sub %02x  %s\n",
                          b, d, f, vid, did, cls, sub, PciClassName(cls, sub));
                    shown++;
                }
                if (shown >= rows) break;
            }
            if (shown >= rows) break;
        }
        if (shown >= rows) break;
    }

    if (shown == 0)
        Print(L"  No PCI devices found in this range.\n");
}

static void RenderNetworkBrowser(void)
{
    Panel(L"Network Interfaces (SNP)");

    UINTN handleCount = 0;
    EFI_HANDLE* handles = NULL;
    if (EFI_ERROR(gBS->LocateHandleBuffer(ByProtocol, &RdSimpleNetworkProtocolGuid, NULL, &handleCount, &handles)) || !handleCount)
    {
        Print(L"  No Simple Network Protocol handles found.\n");
        Print(L"  The firmware does not expose network interfaces via SNP.\n");
        return;
    }

    Print(L"  SNP Handles Found: %d\n", handleCount);
    for (UINTN i = 0; i < handleCount; i++)
    {
        RD_SIMPLE_NETWORK* snp = NULL;
        if (EFI_ERROR(gBS->HandleProtocol(handles[i], &RdSimpleNetworkProtocolGuid, (VOID**)&snp)) || !snp || !snp->Mode)
        {
            Print(L"  %03d  protocol error\n", i);
            continue;
        }

        RD_SNP_MODE* mode = snp->Mode;
        Print(L"  %03d  state %u  type %u  hwaddr ",
              i, mode->State, mode->IfType);
        for (UINTN j = 0; j < mode->HwAddressSize && j < 8; j++)
            Print(L"%02x", mode->MacAddress[j]);
        Print(L"  permanent ");
        for (UINTN j = 0; j < mode->HwAddressSize && j < 8; j++)
            Print(L"%02x", mode->PermanentAddress[j]);
        Print(L"\n");

        Print(L"       media %s  bitrate %s\n",
              mode->MediaPresentSupported ? (mode->MediaPresent ? L"up" : L"down") : L"n/a",
              mode->MaxBitRateSupported ? L"yes" : L"no");
    }

    gBS->FreePool(handles);
}

static void RenderTpmSecurityBrowser(const SMBIOS_STRUCTURE_TABLE* entry)
{
    Panel(L"TPM & Security");

    UINT8 u8 = 0;
    if (ReadGlobalU8(L"SecureBoot", &u8))
        PrintStatus(L"SecureBoot", u8 != 0);
    else
        Print(L"  SecureBoot:          variable unavailable\n");

    if (ReadGlobalU8(L"SetupMode", &u8))
        Print(L"  SetupMode:           %u\n", u8);
    if (ReadGlobalU8(L"AuditMode", &u8))
        Print(L"  AuditMode:           %u\n", u8);
    if (ReadGlobalU8(L"DeployedMode", &u8))
        Print(L"  DeployedMode:        %u\n", u8);

    UINTN varSize = 0;
    Print(L"  PK:                  %s", GlobalVariableExists(L"PK", &varSize) ? L"present" : L"missing");
    if (varSize) Print(L" (%d bytes)", varSize);
    Print(L"\n");
    varSize = 0;
    Print(L"  KEK:                 %s", GlobalVariableExists(L"KEK", &varSize) ? L"present" : L"missing");
    if (varSize) Print(L" (%d bytes)", varSize);
    Print(L"\n");

    VOID* tcg2 = NULL;
    BOOLEAN hasTcg2 = !EFI_ERROR(gBS->LocateProtocol(&RdTcg2ProtocolGuid, NULL, &tcg2));
    PrintStatus(L"TCG2 Protocol (TPM2)", hasTcg2);

    if (entry && entry->TableAddress)
    {
        Print(L"\n");
        Panel(L"SMBIOS TPM");
        SMBIOS_STRUCTURE_POINTER tpmTable = FindTableByType(entry, SMBIOS_TYPE_TPM_DEVICE, 0);
        if (tpmTable.Raw)
        {
            if (HasBytes(tpmTable, 0x04, 4))
                Print(L"  Vendor:            %c%c%c%c\n",
                      (CHAR16)ReadU8(tpmTable, 0x04), (CHAR16)ReadU8(tpmTable, 0x05),
                      (CHAR16)ReadU8(tpmTable, 0x06), (CHAR16)ReadU8(tpmTable, 0x07));
            Print(L"  Spec:              %u.%u\n", ReadU8(tpmTable, 0x08), ReadU8(tpmTable, 0x09));
            Print(L"  Firmware:          0x%08x\n", ReadU32(tpmTable, 0x0A));
        }
        else
        {
            Print(L"  SMBIOS TPM entry:  not exposed\n");
        }
    }
}

static void ShowAcpiTableDetail(UINTN index)
{
    RD_ACPI_HEADER* header = BrowserAcpiTableAt(index);
    if (!header) return;

    BOOLEAN done = FALSE;
    UINTN dumpOffset = 0;

    while (!done)
    {
        gST->ConOut->ClearScreen(gST->ConOut);
        SetColor(COLOR_TITLE);
        Print(L"\n  Rainbow Dragon - ACPI Table Detail\n");
        Rule();

        SetColor(COLOR_OK);
        Print(L"  Table #%d  ", index);
        PrintAsciiInline(header->Signature, 4);
        Print(L"\n");
        SetColor(COLOR_DIM);
        Print(L"  Address:           0x%08x\n", (UINTN)header);
        SetColor(COLOR_NORMAL);

        Print(L"  Length:            %u bytes\n", header->Length);
        Print(L"  Revision:          %u\n", header->Revision);
        Print(L"  Checksum:          0x%02x (%s)\n", header->Checksum,
              ByteSum((const UINT8*)header, header->Length) == 0 ? L"OK" : L"BAD");
        PrintAsciiFixed(L"OEM ID:            ", header->OemId, 6);
        PrintAsciiFixed(L"OEM Table ID:      ", header->OemTableId, 8);
        Print(L"  OEM Revision:      0x%08x\n", header->OemRevision);
        Print(L"  Creator ID:        ");
        PrintAsciiInline((const UINT8*)&header->CreatorId, 4);
        Print(L"  rev 0x%08x\n", header->CreatorRevision);
        Print(L"  Type:              %s\n", AcpiTableName(header->Signature));
        Rule();

        UINTN rowsLeft = 25 - 14;
        UINTN available = header->Length;

        while (dumpOffset < available && rowsLeft > 0)
        {
            Print(L"  %04x:", dumpOffset);
            for (UINTN b = 0; b < 16; b++)
            {
                if ((b % 8) == 0) Print(L" ");
                if (dumpOffset + b < available)
                    Print(L" %02x", ((UINT8*)header)[dumpOffset + b]);
                else
                    Print(L"   ");
            }
            Print(L"  |");
            for (UINTN b = 0; b < 16 && dumpOffset + b < available; b++)
            {
                UINT8 c = ((UINT8*)header)[dumpOffset + b];
                Print(L"%c", (CHAR16)((c >= 32 && c <= 126) ? c : '.'));
            }
            Print(L"|\n");
            dumpOffset += 16;
            rowsLeft--;
        }

        Print(L"\n");
        if (dumpOffset < available)
        {
            SetColor(COLOR_DIM);
            Print(L"  SPACE: next page   Q/ESC: back");
        }
        else
        {
            SetColor(COLOR_DIM);
            Print(L"  Q/ESC: back");
            dumpOffset = 0;
        }
        SetColor(COLOR_NORMAL);

        UINTN idx = 0;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);
        EFI_INPUT_KEY key;
        if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &key)))
            continue;

        if (key.UnicodeChar == L'q' || key.UnicodeChar == L'Q' || key.ScanCode == RD_SCAN_ESC)
            done = TRUE;
        else if (key.UnicodeChar == L' ' && dumpOffset < available)
            continue;
        else if (key.ScanCode == RD_SCAN_PAGE_DOWN && dumpOffset < available)
            continue;
        else if (key.ScanCode == RD_SCAN_PAGE_UP)
        {
            UINTN pageSize = (25 - 14) * 16;
            dumpOffset = dumpOffset > pageSize * 2 ? dumpOffset - pageSize * 2 : 0;
        }
        else
            done = TRUE;
    }
}

void DisplayAnalysisBrowser(const SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN tool = 0;
    UINTN offset = 0;
    UINTN cursor = 0;

    while (TRUE)
    {
        UINTN maxOffset = BrowserMaxOffset(tool, entry);
        UINTN rows = BrowserRows();
        if (offset > maxOffset)
            offset = maxOffset;
        if (cursor >= rows)
            cursor = rows > 0 ? rows - 1 : 0;

        RenderBrowserTool(entry, tool, offset, maxOffset, cursor);
        BrowserAction action = ReadBrowserAction();
        UINTN total = 0;

        switch (action)
        {
        case BROWSER_PREV_TOOL:
            tool = (tool == 0) ? BROWSER_TOOL_COUNT - 1 : tool - 1;
            offset = 0;
            cursor = 0;
            break;
        case BROWSER_NEXT_TOOL:
            tool = (tool + 1) % BROWSER_TOOL_COUNT;
            offset = 0;
            cursor = 0;
            break;
        case BROWSER_UP:
            if (offset + cursor > 0)
            {
                if (cursor > 0)
                    cursor--;
                else
                    offset--;
            }
            break;
        case BROWSER_DOWN:
            if (tool == 4) total = BrowserAcpiCount();
            else if (tool == 1) total = SmbiosTableCount(entry);
            else if (tool == 5) total = BootOrderCount();
            if (offset + cursor + 1 < total)
            {
                if (cursor + 1 < rows)
                    cursor++;
                else
                    offset++;
            }
            break;
        case BROWSER_PAGE_UP:
            if (offset > rows)
                offset -= rows;
            else
                offset = 0;
            break;
        case BROWSER_PAGE_DOWN:
            offset = offset + rows < maxOffset ? offset + rows : maxOffset;
            break;
        case BROWSER_HOME:
            offset = 0;
            cursor = 0;
            break;
        case BROWSER_END:
            offset = maxOffset;
            cursor = rows > 0 ? rows - 1 : 0;
            break;
        case BROWSER_SELECT:
            if (tool == 4)
            {
                UINTN index = offset + cursor;
                UINTN count = BrowserAcpiCount();
                if (index < count)
                    ShowAcpiTableDetail(index);
            }
            break;
        case BROWSER_EXIT:
            return;
        default:
            break;
        }
    }
}

void DisplayFirmwareAndGraphics(const SMBIOS_STRUCTURE_TABLE* entry)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - Firmware & Display");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    CPanel(L"Firmware");
    Print(L"  Vendor:            %s\n", gST->FirmwareVendor ? gST->FirmwareVendor : L"[unknown]");
    Print(L"  Firmware Revision: 0x%08x\n", gST->FirmwareRevision);
    Print(L"  UEFI Revision:     0x%08x\n", gST->Hdr.Revision);
    Print(L"  Config Tables:     %d\n", gST->NumberOfTableEntries);

    VOID* table = NULL;
    PrintStatus(L"ACPI 2.0 configuration table", HasConfigTable(&RdAcpi2TableGuid, &table));
    PrintStatus(L"ACPI 1.0 configuration table", HasConfigTable(&RdAcpiTableGuid, &table));
    PrintStatus(L"SMBIOS entry", entry != NULL);

    CPanel(L"System Table Headers");
    PrintTableHeaderInfo(L"System", &gST->Hdr);
    PrintTableHeaderInfo(L"Boot", &gBS->Hdr);
    PrintTableHeaderInfo(L"Runtime", &gRT->Hdr);

    CPanel(L"Console Output");
    if (gST->ConOut && gST->ConOut->Mode)
    {
        Print(L"  Max Text Modes:    %d\n", gST->ConOut->Mode->MaxMode);
        Print(L"  Current Mode:      %d\n", gST->ConOut->Mode->Mode);
        Print(L"  Attribute:         0x%02x\n", gST->ConOut->Mode->Attribute);
        for (UINTN i = 0; i < (UINTN)gST->ConOut->Mode->MaxMode && i < 12; i++)
        {
            UINTN cols = 0;
            UINTN rows = 0;
            EFI_STATUS status = gST->ConOut->QueryMode(gST->ConOut, i, &cols, &rows);
            if (!EFI_ERROR(status))
                Print(L"  Text Mode %02d:     %d x %d\n", i, cols, rows);
        }
    }

    CPanel(L"Configuration Table Directory");
    UINTN configCount = gST->NumberOfTableEntries;
    Print(L"  Entries exposed:   %d\n", configCount);
    for (UINTN i = 0; i < configCount && i < 16; i++)
    {
        Print(L"  %02d  ", i);
        PrintGuidValue(&gST->ConfigurationTable[i].VendorGuid);
        Print(L"  -> 0x%08x\n", (UINTN)gST->ConfigurationTable[i].VendorTable);
    }
    if (configCount > 16)
        Print(L"  ... %d more configuration table(s)\n", configCount - 16);

    CPanel(L"Graphics");
    GOP* gop = NULL;
    if (LocateGop(&gop) && gop->Mode && gop->Mode->Info)
    {
        Print(L"  Resolution:        %ux%u\n", gop->Mode->Info->HRes, gop->Mode->Info->VRes);
        Print(L"  Pixels Per Line:   %u\n", gop->Mode->Info->PSL);
        Print(L"  Pixel Format:      %s\n", PixelFormatName(gop->Mode->Info->Fmt));
        Print(L"  Current Mode:      %u / %u\n", gop->Mode->Mode, gop->Mode->Max);
        Print(L"  Framebuffer:       0x%08x\n", (UINTN)gop->Mode->FB);
        Print(L"  Framebuffer Bytes: %d\n", gop->Mode->FBSz);
        Print(L"  Framebuffer Size:  %d MiB\n", (UINTN)(gop->Mode->FBSz / (1024 * 1024)));
        Print(L"  Pixel Bitmasks:    R 0x%08x G 0x%08x B 0x%08x X 0x%08x\n",
              gop->Mode->Info->Info.RedMask,
              gop->Mode->Info->Info.GreenMask,
              gop->Mode->Info->Info.BlueMask,
              gop->Mode->Info->Info.ReservedMask);

        if (gop->QMode)
        {
            CPanel(L"Graphics Modes");
            UINT32 max = gop->Mode->Max;
            for (UINT32 i = 0; i < max && i < 24; i++)
            {
                UINTN infoSize = 0;
                GOP_MI* info = NULL;
                EFI_STATUS status = gop->QMode(gop, i, &infoSize, &info);
                if (!EFI_ERROR(status) && info)
                    Print(L"  Mode %02u: %ux%u  %s  line %u\n",
                          i, info->HRes, info->VRes, PixelFormatName(info->Fmt), info->PSL);
            }
            if (max > 24)
                Print(L"  ... %u more mode(s)\n", max - 24);
        }
    }
    else
    {
        SetColor(COLOR_WARN);
        Print(L"  Graphics Output Protocol is unavailable.\n");
        SetColor(COLOR_NORMAL);
        Print(L"  Rainbow Dragon will fall back to a text menu on this machine.\n");
    }

    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

void DisplayCompatibilityMatrix(const SMBIOS_STRUCTURE_TABLE* entry)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    PgInit();

    Print(L"\n");
    SetColor(COLOR_TITLE);
    PgCenter(L"Rainbow Dragon - Compatibility Matrix");
    Print(L"\n");
    Rule();
    BrandLine();
    SetColor(COLOR_NORMAL);
    PgCheck(4);

    CPanel(L"Core Availability");
    PrintStatus(L"UEFI system table", gST != NULL);
    PrintStatus(L"UEFI boot services", gBS != NULL);
    PrintStatus(L"UEFI runtime services", gRT != NULL);
    PrintStatus(L"SMBIOS entry point", entry != NULL && entry->TableAddress != 0);
    Print(L"  System CRC32:      0x%08x\n", gST->Hdr.CRC32);
    Print(L"  Boot CRC32:        0x%08x\n", gBS->Hdr.CRC32);
    Print(L"  Runtime CRC32:     0x%08x\n", gRT->Hdr.CRC32);

    VOID* table = NULL;
    PrintStatus(L"ACPI 2.0 root pointer", HasConfigTable(&RdAcpi2TableGuid, &table));
    PrintStatus(L"ACPI 1.0 root pointer", HasConfigTable(&RdAcpiTableGuid, &table));

    GOP* gop = NULL;
    BOOLEAN hasGop = LocateGop(&gop) && gop && gop->Mode && gop->Mode->Info;
    PrintStatus(L"Graphics Output Protocol", hasGop);
    PrintStatus(L"GOP Blt renderer", hasGop && gop->Blt != NULL);
    PrintStatus(L"GOP mode query", hasGop && gop->QMode != NULL);

    CPanel(L"Protocol Coverage");
    VOID* protocol = NULL;
    PrintStatus(L"Loaded Image", g_ImageHandle && !EFI_ERROR(gBS->HandleProtocol(g_ImageHandle, &RdLoadedImageProtocolGuid, &protocol)));
    PrintStatus(L"Device Path", !EFI_ERROR(gBS->LocateProtocol(&RdDevicePathProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Simple File System", !EFI_ERROR(gBS->LocateProtocol(&RdSimpleFileSystemProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Absolute Pointer", !EFI_ERROR(gBS->LocateProtocol(&RdAbsolutePointerProtocolGuid, NULL, &protocol)));
    PrintStatus(L"Text Output", gST->ConOut != NULL);
    PrintStatus(L"Text Input", gST->ConIn != NULL);

    CPanel(L"Variable Access");
    PrintStatus(L"SecureBoot", GlobalVariableExists(L"SecureBoot", NULL));
    PrintStatus(L"SetupMode", GlobalVariableExists(L"SetupMode", NULL));
    PrintStatus(L"BootCurrent", GlobalVariableExists(L"BootCurrent", NULL));
    PrintStatus(L"BootOrder", GlobalVariableExists(L"BootOrder", NULL));
    PrintStatus(L"BootNext", GlobalVariableExists(L"BootNext", NULL));

    CPanel(L"SMBIOS Coverage");
    if (entry && entry->TableAddress)
    {
        UINTN counts[SMBIOS_TYPE_PROCESSOR_ADDITIONAL_INFORMATION + 1];
        gBS->SetMem(counts, sizeof(counts), 0);
        UINTN total = CountSmbiosTables(entry, counts, sizeof(counts) / sizeof(counts[0]));

        Print(L"  Tables scanned:    %d\n", total);
        PrintStatus(L"BIOS information", counts[SMBIOS_TYPE_BIOS_INFORMATION] > 0);
        PrintStatus(L"System identity", counts[SMBIOS_TYPE_SYSTEM_INFORMATION] > 0);
        PrintStatus(L"Baseboard identity", counts[SMBIOS_TYPE_BASEBOARD_INFORMATION] > 0);
        PrintStatus(L"Processor entries", counts[SMBIOS_TYPE_PROCESSOR_INFORMATION] > 0);
        PrintStatus(L"Memory devices", counts[SMBIOS_TYPE_MEMORY_DEVICE] > 0);
        PrintStatus(L"TPM device", counts[SMBIOS_TYPE_TPM_DEVICE] > 0);
        PrintStatus(L"Expansion slots", counts[SMBIOS_TYPE_SYSTEM_SLOTS] > 0);
    }
    else
    {
        Print(L"  SMBIOS coverage unavailable.\n");
    }

    CPanel(L"Runtime Notes");
    Print(L"  Active menu:       read-only diagnostics\n");
    Print(L"  Graphics fallback: text menu available\n");
    Print(L"  Build target:      %s\n", BuildArch());
    Print(L"  Output binary:     DragonTool.efi\n");

    if (!g_PgExit) {
        Print(L"\n");
        WaitForAnyKey();
    }
}

// ─── Early Boot Scanner ─────────────────────────────────────────────────
// Enumerates all UEFI/DXE drivers loaded before Windows starts.
// Windows kernel-mode drivers (vgk.sys, ipt.sys, etc.) load after UEFI
// hands off to the OS and are NOT visible at this level.

typedef struct {
    EFI_HANDLE       Handle;
    RD_LOADED_IMAGE *Image;
    CHAR16          *Name;
} DRV_ENTRY;

static const CHAR16* DrvImageTypeName(EFI_MEMORY_TYPE type)
{
    switch (type)
    {
    case EfiLoaderCode:          return L"Loader";
    case EfiBootServicesCode:    return L"Boot";
    case EfiRuntimeServicesCode: return L"Runtime";
    case EfiPalCode:             return L"PAL";
    default:                     return L"Other";
    }
}

static UINT16 DpNodeLen(EFI_DEVICE_PATH_PROTOCOL *n)
{
    return (UINT16)(n->Length[0] | (n->Length[1] << 8));
}

static CHAR16* ExtractFileName(EFI_DEVICE_PATH_PROTOCOL *dp)
{
    if (!dp) return NULL;
    EFI_DEVICE_PATH_PROTOCOL *node = dp;
    CHAR16 *filePart = NULL;
    while (node->Type < 0x7F)
    {
        if (node->Type == 4 && node->SubType == 4)
            filePart = (CHAR16*)((UINT8*)node + 4);
        UINT16 len = DpNodeLen(node);
        if (len < 4) break;
        node = (EFI_DEVICE_PATH_PROTOCOL*)((UINT8*)node + len);
    }
    if (!filePart) return NULL;
    CHAR16 *lastComp = filePart;
    for (CHAR16 *p = filePart; *p; p++)
        if (*p == L'\\') lastComp = p + 1;
    UINTN len = 0;
    while (lastComp[len]) len++;
    if (len == 0) return NULL;
    CHAR16 *copy = AllocatePool((len + 1) * sizeof(CHAR16));
    if (copy) CopyMem(copy, lastComp, (len + 1) * sizeof(CHAR16));
    return copy;
}

void ShowBootDriverScanner(void)
{
    EFI_STATUS status;
    EFI_HANDLE *handleBuf = NULL;
    UINTN handleCount = 0;

    status = gBS->LocateHandleBuffer(ByProtocol, &RdLoadedImageProtocolGuid, NULL, &handleCount, &handleBuf);
    if (EFI_ERROR(status) || handleCount == 0)
    {
        gST->ConOut->ClearScreen(gST->ConOut);
        Print(L"\n  [FAIL] No loaded images found.\n\n  Press any key...");
        UINTN evt; gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &evt); gST->ConIn->ReadKeyStroke(gST->ConIn, (void*)&evt);
        return;
    }

    DRV_ENTRY *entries = AllocateZeroPool(handleCount * sizeof(DRV_ENTRY));
    if (!entries) { FreePool(handleBuf); return; }

    UINTN valid = 0;
    for (UINTN i = 0; i < handleCount; i++)
    {
        RD_LOADED_IMAGE *img = NULL;
        if (EFI_ERROR(gBS->HandleProtocol(handleBuf[i], &RdLoadedImageProtocolGuid, (VOID**)&img)) || !img)
            continue;
        entries[valid].Handle = handleBuf[i];
        entries[valid].Image  = img;
        entries[valid].Name   = ExtractFileName((EFI_DEVICE_PATH_PROTOCOL*)img->FilePath);
        valid++;
    }
    FreePool(handleBuf);

    if (valid == 0)
    {
        FreePool(entries);
        gST->ConOut->ClearScreen(gST->ConOut);
        Print(L"\n  [INFO] No file-backed drivers found.\n\n  Press any key...");
        UINTN evt; gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &evt); gST->ConIn->ReadKeyStroke(gST->ConIn, (void*)&evt);
        return;
    }

    UINTN cursor = 0, offset = 0, pageSize = 20;
    BOOLEAN run = TRUE;

    while (run)
    {
        gST->ConOut->ClearScreen(gST->ConOut);
        UINTN cols = 80, rows = 25;
        if (gST->ConOut && gST->ConOut->Mode)
            gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &cols, &rows);
        pageSize = rows > 8 ? rows - 8 : 10;

        SetColor(COLOR_TITLE);
        Print(L"\n");
        UINTN tlen = 38;
        UINTN pad = (tlen < cols) ? (cols - tlen) / 2 : 0;
        for (UINTN i = 0; i < pad; i++) Print(L" ");
        Print(L"Rainbow Dragon - Early Boot Scanner\n");
        SetColor(COLOR_NORMAL);

        SetColor(COLOR_DIM);
        for (UINTN i = 0; i < cols - 2; i++) Print(L"-");
        Print(L"\n  %d EFI driver(s) loaded before Windows starts", (UINTN)valid);
        SetColor(COLOR_WARN);
        Print(L"  [vk.sys / ipt.sys NOT visible from UEFI]\n");
        SetColor(COLOR_NORMAL);

        for (UINTN i = 0; i < pageSize && offset + i < valid; i++)
        {
            UINTN idx = offset + i;
            BOOLEAN cur = (idx == cursor);
            CHAR16 *name = entries[idx].Name ? entries[idx].Name : L"(unnamed)";
            UINTN base = entries[idx].Image ? (UINTN)entries[idx].Image->ImageBase : 0;
            UINTN sz   = entries[idx].Image ? (UINTN)entries[idx].Image->ImageSize : 0;
            const CHAR16 *type = entries[idx].Image ? DrvImageTypeName(entries[idx].Image->ImageCodeType) : L"?";

            if (cur) SetColor(COLOR_SELECTED);
            Print(L" %c [%3d] %-40s 0x%08x  %4dK  %s\n",
                  cur ? L'>' : L' ', (UINTN)idx + 1, name, (UINTN)base, (UINTN)(sz / 1024), type);
            if (cur) SetColor(COLOR_NORMAL);
        }

        SetColor(COLOR_DIM);
        for (UINTN i = 0; i < cols - 2; i++) Print(L"-");
        Print(L"\n  Page %d/%d  |  Driver %d/%d\n",
              (UINTN)(offset / pageSize) + 1, (UINTN)((valid + pageSize - 1) / pageSize),
              (UINTN)cursor + 1, (UINTN)valid);
        Print(L"  UP/DOWN: navigate | PgUp/PgDn: page | Enter: detail | Q/ESC: quit\n");
        SetColor(COLOR_NORMAL);

        UINTN evt;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &evt);
        EFI_INPUT_KEY key;
        if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &key)))
            continue;

        if (key.ScanCode == RD_SCAN_UP || key.UnicodeChar == L'w' || key.UnicodeChar == L'W')
        {
            if (cursor > 0) cursor--;
            if (cursor < offset) offset = cursor;
        }
        else if (key.ScanCode == RD_SCAN_DOWN || key.UnicodeChar == L's' || key.UnicodeChar == L'S')
        {
            if (cursor < valid - 1) cursor++;
            if (cursor >= offset + pageSize) offset = cursor - pageSize + 1;
        }
        else if (key.ScanCode == RD_SCAN_PAGE_UP)
        {
            if (offset >= pageSize) offset -= pageSize; else offset = 0;
            cursor = offset;
        }
        else if (key.ScanCode == RD_SCAN_PAGE_DOWN)
        {
            offset += pageSize;
            if (offset + pageSize > valid) offset = valid > pageSize ? valid - pageSize : 0;
            cursor = offset;
        }
        else if (key.UnicodeChar == L'\r')
        {
            UINTN sel = cursor;
            if (sel >= valid || !entries[sel].Image) continue;

            RD_LOADED_IMAGE *img = entries[sel].Image;
            gST->ConOut->ClearScreen(gST->ConOut);
            Print(L"\n");
            SetColor(COLOR_TITLE);
            Print(L"  ── Driver Detail ────────────────────────────────────────────\n");
            SetColor(COLOR_NORMAL);

            CHAR16 *name = entries[sel].Name ? entries[sel].Name : L"(unnamed)";
            Print(L"  Name:              %s\n", name);
            Print(L"  Handle:            0x%08x\n", (UINTN)entries[sel].Handle);
            Print(L"  Image Base:        0x%08x\n", (UINTN)img->ImageBase);
            Print(L"  Image Size:        %d bytes (%d KiB)\n", (UINTN)img->ImageSize, (UINTN)(img->ImageSize / 1024));
            Print(L"  Code Type:         %s\n", DrvImageTypeName(img->ImageCodeType));
            Print(L"  Data Type:         %s\n", DrvImageTypeName(img->ImageDataType));
            Print(L"  Parent Handle:     0x%08x\n", (UINTN)img->ParentHandle);
            Print(L"  Device Handle:     0x%08x\n", (UINTN)img->DeviceHandle);

            if (img->LoadOptions && img->LoadOptionsSize > 0)
            {
                Print(L"  Load Options:      ");
                for (UINTN b = 0; b < img->LoadOptionsSize && b < 64; b++)
                    Print(L"%02x ", (unsigned)((UINT8*)img->LoadOptions)[b]);
                Print(L" (%d bytes)\n", (UINTN)img->LoadOptionsSize);
            }

            if (img->ImageBase && img->ImageSize > 0)
            {
                Print(L"\n  ── Hex Dump (first 128 bytes) ────────────────────────────\n");
                UINT8 *base = (UINT8*)img->ImageBase;
                UINTN dumpSz = img->ImageSize < 128 ? (UINTN)img->ImageSize : 128;
                for (UINTN row = 0; row < dumpSz; row += 16)
                {
                    Print(L"  0x%08x  ", (UINTN)(base + row));
                    for (UINTN b = 0; b < 16; b++)
                    {
                        if (row + b < dumpSz)
                            Print(L"%02x ", (unsigned)base[row + b]);
                        else
                            Print(L"   ");
                        if (b == 7) Print(L" ");
                    }
                    Print(L" |");
                    for (UINTN b = 0; b < 16 && row + b < dumpSz; b++)
                    {
                        UINT8 c = base[row + b];
                        Print(L"%c", (c >= 32 && c <= 126) ? (CHAR16)c : L'.');
                    }
                    Print(L"|\n");
                }
            }

            Print(L"\n  Press any key to return...");
            UINTN evt2; gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &evt2);
            gST->ConIn->ReadKeyStroke(gST->ConIn, &key);
        }
        else if (key.UnicodeChar == L'q' || key.UnicodeChar == L'Q' || key.ScanCode == RD_SCAN_ESC)
        {
            run = FALSE;
        }
    }

    for (UINTN i = 0; i < valid; i++)
        if (entries[i].Name) FreePool(entries[i].Name);
    FreePool(entries);
}

static const UINTN LEKSA_COLORS[] = {
    0x04, 0x06, 0x0E, 0x0A, 0x02,
    0x0B, 0x03, 0x09, 0x01, 0x0D
};
#define LEKSA_COLOR_COUNT 10

void ShowLetsaMod(void)
{
    const CHAR16* items[] = {
        L"Smart Spoof All SMBIOS",
        L"ACPI DSDT/SSDT Spoof",
        L"Early Boot Scanner",
        L"Backup CPU Speed Raw Fields",
        L"Restore CPU Speed Raw Fields",
        L"Reload Default HWID",
        L"Rainbow Test Pattern",
        L"Leksa Trivia",
        L"Back to Main Menu"
    };
    const UINTN itemCount = sizeof(items) / sizeof(items[0]);
    UINTN selected = 0;

    while (TRUE)
    {
        gST->ConOut->ClearScreen(gST->ConOut);

        UINTN cols = 80, rows = 25;
        if (gST->ConOut && gST->ConOut->Mode)
            gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &cols, &rows);

        Print(L"\n\n");
        const CHAR16* title = L"L E K S A   M O D";
        UINTN tlen = 0;
        while (title[tlen]) tlen++;
        UINTN pad = (tlen < cols) ? (cols - tlen) / 2 : 0;
        for (UINTN i = 0; i < pad; i++) Print(L" ");
        for (UINTN i = 0; title[i]; i++)
        {
            SetColor(LEKSA_COLORS[i % LEKSA_COLOR_COUNT]);
            Print(L"%c", title[i]);
        }
        SetColor(COLOR_NORMAL);
        Print(L"\n\n");

        for (UINTN i = 0; i < itemCount; i++)
        {
            UINTN w = 0;
            while (items[i][w]) w++;
            UINTN ipad = (w + 4 < cols) ? (cols - w - 4) / 2 : 0;
            for (UINTN j = 0; j < ipad; j++) Print(L" ");

            if (i == selected)
                SetColor(COLOR_SELECTED);
            else
                SetColor(0x0E);

            Print(L"%c ", i == selected ? L'>' : L' ');
            SetColor(i == selected ? COLOR_SELECTED : COLOR_NORMAL);
            Print(L"%s\n", items[i]);
            SetColor(COLOR_NORMAL);
        }

        Print(L"\n");
        SetColor(COLOR_DIM);
        pad = (46 < cols) ? (cols - 46) / 2 : 0;
        for (UINTN i = 0; i < pad; i++) Print(L" ");
        Print(L"Arrows or WASD to navigate   Enter to select\n");
        SetColor(COLOR_NORMAL);

        UINTN idx = 0;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);
        EFI_INPUT_KEY key;
        if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &key)))
            continue;

        if (key.ScanCode == RD_SCAN_UP || key.UnicodeChar == L'w' || key.UnicodeChar == L'W')
            selected = (selected == 0) ? itemCount - 1 : selected - 1;
        else if (key.ScanCode == RD_SCAN_DOWN || key.UnicodeChar == L's' || key.UnicodeChar == L'S')
            selected = (selected + 1) % itemCount;
        else if (key.ScanCode == RD_SCAN_ESC)
            return;
        else if (key.UnicodeChar == L'\r')
        {
            if (items[selected] == items[itemCount - 1])
                return;
            if (selected == 0)
            {
                SMBIOS_STRUCTURE_TABLE* entry = FindEntry();
                if (entry && entry->TableAddress)
                {
                    PatchAll(entry);
                }
                else
                {
                    SetColor(COLOR_WARN);
                    Print(L"\n  [FAIL] No SMBIOS table available!\n");
                    SetColor(COLOR_NORMAL);
                }
            }
            else if (selected == 1)
            {
                PatchAllAcpi();
            }
            else if (selected == 2)
            {
                ShowBootDriverScanner();
            }
            else if (selected == 3)
            {
                SMBIOS_STRUCTURE_TABLE* entry = FindEntry();
                if (entry && entry->TableAddress)
                {
                    SaveCpuSpeedBackup(entry);
                }
                else
                {
                    SetColor(COLOR_WARN);
                    Print(L"\n  [FAIL] No SMBIOS table available!\n");
                    SetColor(COLOR_NORMAL);
                }
            }
            else if (selected == 4)
            {
                SMBIOS_STRUCTURE_TABLE* entry = FindEntry();
                if (entry && entry->TableAddress)
                {
                    RestoreCpuSpeedBackup(entry);
                }
                else
                {
                    SetColor(COLOR_WARN);
                    Print(L"\n  [FAIL] No SMBIOS table available!\n");
                    SetColor(COLOR_NORMAL);
                }
            }
            else if (selected == 5)
            {
                SMBIOS_STRUCTURE_TABLE* entry = FindEntry();
                if (entry && entry->TableAddress)
                {
                    RestoreSmbiosDefaults(entry);
                }
                else
                {
                    SetColor(COLOR_WARN);
                    Print(L"\n  [FAIL] No SMBIOS table available!\n");
                    SetColor(COLOR_NORMAL);
                }
            }
            Print(L"\n  Press any key to continue...");
            UINTN idx;
            gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &idx);
            EFI_INPUT_KEY k;
            gST->ConIn->ReadKeyStroke(gST->ConIn, &k);
        }
    }
}
