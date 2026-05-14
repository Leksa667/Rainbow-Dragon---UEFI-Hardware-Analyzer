#include "general.h"
#include "acpi_patch.h"

#define COLOR_NORMAL   0x07
#define COLOR_OK       0x0A
#define COLOR_WARN     0x0E
#define COLOR_DIM      0x08
#define COLOR_BAD      0x0C

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

static EFI_GUID gAcpiTableGuid  = { 0xEB9D2D30, 0x2D88, 0x11D3, { 0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D } };
static EFI_GUID gAcpi2TableGuid = { 0x8868E871, 0xE4F1, 0x11D3, { 0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81 } };

static void SetColor(UINTN attr)
{
    gST->ConOut->SetAttribute(gST->ConOut, attr);
}

static BOOLEAN SigEq(const UINT8* sig, const char* val)
{
    return sig[0] == (UINT8)val[0] && sig[1] == (UINT8)val[1]
        && sig[2] == (UINT8)val[2] && sig[3] == (UINT8)val[3];
}

static UINT32 ReadLe32(const UINT8* data)
{
    return (UINT32)data[0] | ((UINT32)data[1] << 8)
         | ((UINT32)data[2] << 16) | ((UINT32)data[3] << 24);
}

static UINT64 ReadLe64(const UINT8* data)
{
    UINT64 v = 0;
    for (UINTN i = 0; i < 8; i++)
        v |= ((UINT64)data[i]) << (i * 8);
    return v;
}

static UINT8 AcpiChecksum(const UINT8* data, UINTN length)
{
    UINT8 sum = 0;
    for (UINTN i = 0; i < length; i++)
        sum = (UINT8)(sum + data[i]);
    return sum;
}

static void FixChecksum(RD_ACPI_HEADER* hdr)
{
    hdr->Checksum = 0;
    UINT8 sum = AcpiChecksum((const UINT8*)hdr, hdr->Length);
    hdr->Checksum = (UINT8)(0x100 - sum);
}

static void RandomFill(UINT8* buf, UINTN len)
{
    static const char DIGITS[] = "0123456789ABCDEF";
    for (UINTN i = 0; i < len; i++)
    {
        UINTN r = (UINTN)(UINT64)buf + i + (UINTN)gST->FirmwareRevision + (UINTN)gRT;
        buf[i] = (UINT8)DIGITS[(r + (i * 7)) & 0x0F];
    }
}

static void PatchAcpiHeader(RD_ACPI_HEADER* hdr, const CHAR16* label)
{
    if (!hdr || hdr->Length < sizeof(RD_ACPI_HEADER))
        return;

    UINT8 oldOem[6];
    UINT8 oldTable[8];
    UINT32 oldRev = hdr->OemRevision;

    CopyMem(oldOem, hdr->OemId, 6);
    CopyMem(oldTable, hdr->OemTableId, 8);

    RandomFill(hdr->OemId, 6);
    hdr->OemId[0] = (UINT8)'O'; hdr->OemId[1] = (UINT8)'E'; hdr->OemId[2] = (UINT8)'M';

    RandomFill(hdr->OemTableId, 8);
    hdr->OemRevision = (UINT32)((UINTN)gRT ^ (UINTN)hdr ^ 0xAC91);

    FixChecksum(hdr);

    SetColor(COLOR_OK);
    Print(L"  %s  OEM ", label);
    SetColor(COLOR_DIM);
    for (UINTN i = 0; i < 6; i++) Print(L"%c", (CHAR16)oldOem[i]);
    SetColor(COLOR_NORMAL);
    Print(L" \x1E ");
    SetColor(COLOR_OK);
    for (UINTN i = 0; i < 6; i++) Print(L"%c", (CHAR16)hdr->OemId[i]);
    SetColor(COLOR_NORMAL);
    Print(L"  Table ");
    SetColor(COLOR_DIM);
    for (UINTN i = 0; i < 8; i++) Print(L"%c", (CHAR16)oldTable[i]);
    SetColor(COLOR_NORMAL);
    Print(L" \x1E ");
    SetColor(COLOR_OK);
    for (UINTN i = 0; i < 8; i++) Print(L"%c", (CHAR16)hdr->OemTableId[i]);
    SetColor(COLOR_NORMAL);
    Print(L"  Rev 0x%08x \x1E 0x%08x  checksum %s\n",
          oldRev, hdr->OemRevision,
          AcpiChecksum((const UINT8*)hdr, hdr->Length) == 0 ? L"OK" : L"BAD");
}

void PatchAllAcpi(void)
{
    gST->ConOut->ClearScreen(gST->ConOut);
    SetColor(COLOR_OK);
    Print(L"\n  [WORK] Scanning ACPI tables for spoofing...\n");
    SetColor(COLOR_NORMAL);

    VOID* rsdpTable = NULL;
    if (EFI_ERROR(LibGetSystemConfigurationTable(&gAcpi2TableGuid, &rsdpTable)))
        LibGetSystemConfigurationTable(&gAcpiTableGuid, &rsdpTable);

    if (!rsdpTable)
    {
        SetColor(COLOR_WARN);
        Print(L"\n  [FAIL] ACPI RSDP not found via config tables.\n");
        SetColor(COLOR_NORMAL);
        return;
    }

    RD_RSDP* rsdp = (RD_RSDP*)rsdpTable;
    BOOLEAN xsdt = FALSE;
    RD_ACPI_HEADER* root = NULL;

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
        Print(L"\n  [FAIL] ACPI root table (RSDT/XSDT) invalid.\n");
        SetColor(COLOR_NORMAL);
        return;
    }

    SetColor(COLOR_DIM);
    Print(L"\n  Root: %c%c%c%c at 0x%08x, %u entries\n",
          (CHAR16)root->Signature[0], (CHAR16)root->Signature[1],
          (CHAR16)root->Signature[2], (CHAR16)root->Signature[3],
          (UINTN)root, root->Length);
    SetColor(COLOR_NORMAL);

    UINTN entrySize = xsdt ? sizeof(UINT64) : sizeof(UINT32);
    UINTN count = (root->Length - sizeof(RD_ACPI_HEADER)) / entrySize;
    UINT8* entries = ((UINT8*)root) + sizeof(RD_ACPI_HEADER);

    UINTN patched = 0;

    /* Patch DSDT and SSDT tables */
    for (UINTN i = 0; i < count && i < 128; i++)
    {
        UINTN address = xsdt ? (UINTN)ReadLe64(entries + i * entrySize)
                             : (UINTN)ReadLe32(entries + i * entrySize);
        RD_ACPI_HEADER* hdr = (RD_ACPI_HEADER*)address;
        if (!hdr || hdr->Length < sizeof(RD_ACPI_HEADER))
            continue;

        if (SigEq(hdr->Signature, "DSDT") || SigEq(hdr->Signature, "SSDT"))
        {
            CHAR16 label[16];
            if (SigEq(hdr->Signature, "DSDT"))
                SPrint(label, 16, L"  [DSDT]");
            else
            {
                UINTN ssi = 0;
                for (UINTN j = 0; j < i; j++)
                {
                    UINTN aj = xsdt ? (UINTN)ReadLe64(entries + j * entrySize)
                                    : (UINTN)ReadLe32(entries + j * entrySize);
                    RD_ACPI_HEADER* hj = (RD_ACPI_HEADER*)aj;
                    if (hj && hj->Length >= sizeof(RD_ACPI_HEADER) && SigEq(hj->Signature, "SSDT"))
                        ssi++;
                }
                SPrint(label, 16, L"  [SSDT#%d]", ssi);
            }

            PatchAcpiHeader(hdr, label);
            patched++;
        }
    }

    SetColor(COLOR_OK);
    Print(L"\n  [OK] %d ACPI table(s) patched (DSDT + SSDT%s)\n",
          patched, patched > 0 ? L" + checksums fixed" : L"");
    SetColor(COLOR_NORMAL);

    if (patched == 0)
    {
        SetColor(COLOR_WARN);
        Print(L"\n  [INFO] No DSDT or SSDT tables found in ACPI directory.\n");
        SetColor(COLOR_NORMAL);
    }
}
