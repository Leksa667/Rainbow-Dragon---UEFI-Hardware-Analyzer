#include "general.h"
#include "patch.h"
#include "smbios.h"
#include "utils.h"

/* ── String helpers ──────────────────────────────────── */

static UINTN AStrLen(const char* s)
{
    UINTN n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static BOOLEAN AStrEq(const char* a, const char* b)
{
    if (!a || !b) return FALSE;
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

/* ── Print ASCII string via Print(L"%a", ...) ────────── */
/* %a is supported by GNU EFI's Print() for char*       */

/* ── Data pools ──────────────────────────────────────── */

/* Type 0 */
static const char* POOL_BIOS_VENDOR[] = {
    "American Megatrends Inc.", "Phoenix Technologies Ltd.",
    "Insyde Corp.", "Dell Inc.", "Hewlett-Packard Company",
    "Lenovo Group Ltd.", "ASUSTeK Computer Inc.",
    "Micro-Star International Co., Ltd.", "Gigabyte Technology Co., Ltd.",
    "Intel Corporation"
};

static const char* POOL_BIOS_VERSION[] = {
    "F.20", "1.15.0", "5.14", "P3.40", "1.4.1",
    "0401", "2.4.0", "A.10", "1.2.3", "3.01",
    "F.32", "1.7.5", "080012", "P2.10", "1.20.0"
};

static const char* POOL_BIOS_DATE[] = {
    "03/15/2023", "06/22/2022", "11/08/2023", "01/30/2024",
    "09/12/2022", "04/18/2023", "07/04/2024", "02/14/2023"
};

/* Type 1 + 3 */
static const char* POOL_SYS_MFR[] = {
    "Dell Inc.", "HP Inc.", "Lenovo", "ASUS", "Acer",
    "MSI", "Gigabyte Technology Co., Ltd.", "ASRock",
    "Intel Corporation", "Supermicro", "Clevo Co."
};

static const char* POOL_SYS_VERSION[] = {
    "Rev 1.0", "Rev 2.0", "Rev A", "Rev B", "Rev 1.1",
    "1.0", "2.0", "A", "B", "C", "1.01", "v1.0"
};

/* Type 2 */
static const char* POOL_MB_MFR[] = {
    "ASUSTeK Computer Inc.", "MSI", "Gigabyte Technology Co., Ltd.",
    "ASRock", "Intel Corporation", "Dell Inc.", "HP Inc.",
    "Lenovo", "Supermicro", "Biostar", "EVGA"
};

static const char* POOL_MB_PROD[] = {
    "ROG STRIX Z790-E GAMING WIFI", "PRIME X670-P",
    "MAG Z790 TOMAHAWK WIFI", "MPG B760I EDGE WIFI",
    "Z790 AORUS MASTER", "B650E AORUS PRO X USB4",
    "X670E Taichi", "NUC13DBBi9",
    "0WR19J", "0P2K3Y", "LNVNB161216"
};

static const char* POOL_MB_VER[] = {
    "Rev 1.xx", "Rev 2.xx", "1.0", "A01", "B02", "A00"
};

/* Type 4 */
static const char* POOL_CPU_SOCKET[] = {
    "LGA1700", "LGA1200", "LGA1151", "LGA2066",
    "AM5", "AM4", "sTR5", "sWRX8", "BGA1744"
};

typedef struct { const char* match; const char* replace; } CpuSwap;
static const CpuSwap POOL_CPU_SWAP[] = {
    {"i9-13900K","i7-13700K"},{"i9-13900KF","i7-13700KF"},
    {"i9-13900","i7-13700"},{"i9-12900K","i7-12700K"},
    {"i9-12900KF","i7-12700KF"},{"i9-12900","i7-12700"},
    {"i9-14900K","i7-14700K"},{"i9-14900KF","i7-14700KF"},
    {"i9-14900","i7-14700"},{"i7-13700K","i5-13600K"},
    {"i7-13700KF","i5-13600KF"},{"i7-13700","i5-13500"},
    {"i7-12700K","i5-12600K"},{"i7-12700KF","i5-12600KF"},
    {"i7-12700","i5-12500"},{"i7-14700K","i5-14600K"},
    {"i7-14700KF","i5-14600KF"},{"i7-14700","i5-14500"},
    {"i5-13600K","i3-13100"},{"i5-12600K","i3-12100"},
    {"Ryzen 9 7950X","Ryzen 7 7800X3D"},{"Ryzen 9 7950X3D","Ryzen 7 7800X3D"},
    {"Ryzen 9 7900X","Ryzen 7 7700X"},{"Ryzen 9 7900","Ryzen 7 7700"},
    {"Ryzen 9 5950X","Ryzen 7 5800X"},{"Ryzen 9 5900X","Ryzen 7 5800X"},
    {"Ryzen 7 7800X3D","Ryzen 5 7600X"},{"Ryzen 7 7700X","Ryzen 5 7600X"},
    {"Ryzen 7 7700","Ryzen 5 7600"},{"Ryzen 7 5800X","Ryzen 5 5600X"},
    {"Ryzen 7 5700X","Ryzen 5 5600X"},
    {"Threadripper 7980X","Ryzen 9 7950X"},{"Threadripper 7970X","Ryzen 9 7950X"},
    {"Threadripper 5995WX","Ryzen 9 5950X"},
};
#define CPU_SWAP_COUNT (sizeof(POOL_CPU_SWAP) / sizeof(POOL_CPU_SWAP[0]))

static const UINT16 POOL_CPU_SPEED[] = {
    2100,2200,2400,2500,2600,2700,2800,2900,3000,3100,3200,3300,
    3400,3500,3600,3700,3800,3900,4000,4100,4200,4300,4400,4500,
    4600,4700,4800,4900,5000,5100,5200,5300,5400
};

/* Type 7 */
static const char* POOL_CACHE_SOCKET[] = {
    "L1 Cache", "L2 Cache", "L3 Cache", "L4 Cache",
    "Processor Internal L1", "Processor Internal L2",
    "On Chip L3", "On Chip L3 Cache"
};

/* Type 8 */
static const char* POOL_PORT_INT[] = {
    "USB 3.2 Gen1", "USB 3.2 Gen2", "USB-C 3.2 Gen2x1",
    "HDMI", "DisplayPort", "3.5mm Audio Jack", "RJ-45 GbE",
    "SATA 6Gb/s", "M.2 PCIe 4.0", "Thunderbolt 4"
};
static const char* POOL_PORT_EXT[] = {
    "USB31_A", "USB32_A", "USBC_C1", "HDMI_OUT",
    "DP_OUT", "AUDIO_IO", "LAN_GBE",
    "SATA_0", "M2_1", "TB4_C2"
};

/* Type 9 */
static const char* POOL_SLOT_DESIGNATION[] = {
    "PCIe x16 Slot 1", "PCIe x16 Slot 2", "PCIe x8 Slot 3",
    "PCIe x4 Slot 4", "PCIe x1 Slot 5", "M.2 Slot 1",
    "M.2 Slot 2", "DIMM_A1", "DIMM_A2", "DIMM_B1", "DIMM_B2"
};

/* Type 10 */
static const char* POOL_ONBOARD_DEV[] = {
    "Onboard LAN", "Onboard Audio", "Onboard SATA Controller",
    "Onboard NVMe Controller", "Onboard Wireless",
    "Onboard Bluetooth", "Onboard USB Controller",
    "Onboard Thunderbolt Controller"
};

/* Type 11 */
static const char* POOL_OEM_STR[] = {
    "Assembled in China", "RoHS Compliant",
    "ENERGY STAR Certified", "FCC Class B",
    "CE Marked", "WEEE Compliant",
    "BIOS Revision 2.4", "EC Firmware 1.2.3"
};

/* Type 13 */
static const char* POOL_LANG_INSTALL[] = {
    "en|fr|de|es|it|pt|nl|sv|da|fi|nb|pl|cs|hu|ro|bg|ru|ar|ja|ko|zh",
    "en|fr|de|es|it|pt|nl|sv|da|fi|nb|pl|cs|hu|ro|bg|ru|ar|ja|ko|zh|tr",
    "en|fr|de|es|ja|zh"
};
static const char* POOL_LANG_CURRENT[] = {
    "en", "en-US", "en-GB", "fr", "de", "es", "ja", "zh"
};

/* Type 17 */
static const char* POOL_MEM_MFR[] = {
    "Kingston", "Corsair", "G.Skill", "Samsung", "SK Hynix",
    "Micron", "Crucial", "ADATA", "TeamGroup", "Patriot"
};
static const char* POOL_MEM_PART[] = {
    "KVR32N22S8/16", "CMK32GX4M2E3200", "F4-3200C16D-32",
    "M378A1G43AB", "HMA81GU6CJR8N", "CT16G4SFRA32A",
    "DDR5-4800", "AX5U5200C3816"
};
static const char* POOL_MEM_LOCATOR[] = {
    "DIMM_A1", "DIMM_A2", "DIMM_B1", "DIMM_B2",
    "ChannelA-DIMM0", "ChannelA-DIMM1", "ChannelB-DIMM0"
};
static const char* POOL_MEM_BANK[] = {
    "BANK 0", "BANK 1", "BANK 2", "BANK 3",
    "P0 CHANNEL A", "P0 CHANNEL B",
    "Node0_Channel0_Dimm0", "Node0_Channel1_Dimm0"
};

/* Type 22 */
static const char* POOL_BAT_MFR[] = {
    "LGC", "Samsung SDI", "Panasonic", "Sony",
    "BYD", "Dynapack", "Celxpert", "Simplo"
};
static const char* POOL_BAT_DATE[] = {
    "2023-03-15", "2022-11-08", "2024-01-30", "2023-09-12",
    "2022/06/22", "2024/07/04", "2023/05/09", "2022/12/01"
};
static const char* POOL_BAT_NAME[] = {
    "Li-Ion 11.4V 52Wh", "Li-Poly 15.4V 96Wh",
    "Li-Ion 11.4V 68Wh", "Li-Ion 7.6V 41Wh",
    "Li-Poly 15.4V 82Wh", "Li-Ion 11.4V 56Wh"
};
static const char* POOL_BAT_CHEM[] = {
    "Li-Ion", "Li-Poly", "LiFePO4", "NiMH", "Lead Acid"
};

/* Type 39 */
static const char* POOL_PSU_MFR[] = {
    "SeaSonic", "Corsair", "EVGA", "be quiet!", "Cooler Master",
    "Thermaltake", "FSP Group", "SilverStone", "Super Flower"
};
static const char* POOL_PSU_LOCATION[] = {
    "Internal", "Rear Panel", "Bottom Mount",
    "PSU Bay 1", "PSU Bay 2", "Redundant Bay A"
};
static const char* POOL_PSU_DEVNAME[] = {
    "System Power Supply", "PSU 1", "PSU 2",
    "Redundant PSU Bay A", "Redundant PSU Bay B"
};
static const char* POOL_PSU_MODEL[] = {
    "FOCUS GX-750", "RM750x", "SuperNOVA 850 G7",
    "Dark Power 13 850W", "V850 Platinum", "Toughpower GF3 750",
    "Hydro G Pro 850", "Strider 700W", "Leadex III 850W"
};

/* Type 41 */
static const char* POOL_ONBOARD_REF[] = {
    "LAN Controller", "Audio Codec", "SATA Controller",
    "NVMe Controller", "Wi-Fi Module", "Bluetooth Module",
    "USB 3.2 Hub", "Thunderbolt Controller"
};

/* Type 43 */
static const char* POOL_TPM_DESC[] = {
    "TPM 2.0 Security Module",
    "Discrete TPM 2.0",
    "Firmware TPM 2.0",
    "Intel PTT",
    "AMD PSP fTPM"
};

/* ── New pools for extra fields ──────────────────────── */

/* Type 1 + 3 — SKU */
static const char* POOL_SKU[] = {
    "SKU-001", "SKU-002", "SKU-003", "SKU-A01", "SKU-B02",
    "CFT-0001", "CFT-0002", "LEN-001", "LEN-002", "DSA-101",
    "PRO-0001", "PRO-0002", "BAS-0001", "BAS-0002"
};

/* Type 1 — Family */
static const char* POOL_FAMILY[] = {
    "ThinkPad T-Series", "ThinkPad X-Series", "ThinkPad P-Series",
    "Latitude 5000-Series", "Latitude 7000-Series", "Precision",
    "EliteBook 800-Series", "ProBook 400-Series", "Spectre x360",
    "ROG Strix", "TUF Gaming", "ZenBook", "Vivobook",
    "Inspiron 3000-Series", "XPS 13-Plus", "Surface Pro",
    "MacBook Pro", "MacBook Air"
};

/* Type 2 + 3 — Asset Tag */
static const char* POOL_ASSET_TAG[] = {
    "AT-0001", "AT-0002", "AT-0003",
    "No Asset Tag", "INVALID", "TAG-001", "TAG-002",
    "CN-0001", "CN-0002"
};

/* Type 2 — Location in Chassis */
static const char* POOL_CHASSIS_LOC[] = {
    "Part of System Board", "Mid-Unit", "Main Board",
    "CPU Board", "Baseboard", "System Board",
    "Planar Board", "Logic Board"
};

/* Type 22 — Location */
static const char* POOL_BAT_LOCATION[] = {
    "Internal", "Left Rear", "Right Rear", "Under Keyboard",
    "Back Cover", "Bay 1", "Bay 2", "Main"
};

/* Type 39 — Revision */
static const char* POOL_PSU_REVISION[] = {
    "Rev 1.0", "Rev 2.0", "Rev 1.1", "Rev 2.1",
    "A01", "A02", "B01", "B02", "1.00", "2.00", "1.10"
};

/* Type 4 — CPU Asset Tag (not common, but safe to spoof) */
static const char* POOL_CPU_ASSET[] = {
    "CPU-TAG-0001", "CPU-TAG-0002", "CPU-TAG-0003",
    "NO TAG", "PROC-TAG-001", "FILLER"
};

/* ── Serial number generation ────────────────────────── */

static void GenSerial(char* out, UINTN minLen)
{
    static const char* PREFIXES[] = {
        "SN-","WD-","XX-","Dell","HP-","LEN","MSI","AS-","MB-"
    };
    int pi = RandomNumber(0, 8);
    const char* pfx = PREFIXES[pi];
    UINTN pl = AStrLen(pfx);
    UINTN need = minLen > pl + 4 ? minLen - pl : 6;
    if (need > 128) need = 6;
    UINTN i;
    for (i = 0; i < need; i++)
    {
        int r = RandomNumber(0, 35);
        out[i] = r < 26 ? ('A' + r) : ('0' + r - 26);
    }
    out[i] = '\0';
}

/* ── Spoofing helpers ────────────────────────────────── */

static void SpoofField(SMBIOS_STRUCTURE_POINTER table, SMBIOS_STRING* field,
                       const CHAR16* label, const char** pool, UINTN poolCount)
{
    if (!table.Raw || !field) return;
    const char* orig = GetStringAtIndex(table, *field);
    if (!orig) { Print(L"  %-22s [empty]\n", label); return; }

    char oldBuf[128];
    UINTN ol = AStrLen(orig);
    if (ol >= sizeof(oldBuf)) ol = sizeof(oldBuf) - 1;
    CopyMem(oldBuf, (VOID*)orig, ol);
    oldBuf[ol] = '\0';

    int pick = RandomNumber(0, (int)poolCount - 1);
    const char* repl = pool[pick];
    UINTN rl = AStrLen(repl);

    if (rl < ol)
    {
        EditString(table, field, repl);
    }
    else
    {
        char padded[256];
        UINTN copyLen = ol;
        if (copyLen > sizeof(padded) - 1) copyLen = sizeof(padded) - 1;
        CopyMem(padded, (VOID*)repl, copyLen);
        padded[copyLen] = '\0';
        EditString(table, field, padded);
    }

    Print(L"  %-22s '%a' → '%a'\n", label, oldBuf, repl);
}

static void SpoofAtOffset(SMBIOS_STRUCTURE_POINTER table, UINTN off,
                          const CHAR16* label, const char** pool, UINTN poolCount)
{
    if (!table.Raw || off >= table.Hdr->Length) return;
    SpoofField(table, (SMBIOS_STRING*)(table.Raw + off), label, pool, poolCount);
}

static void SpoofSerial(SMBIOS_STRUCTURE_POINTER table, SMBIOS_STRING* field,
                        const CHAR16* label)
{
    if (!table.Raw || !field) return;
    const char* orig = GetStringAtIndex(table, *field);
    if (!orig) { Print(L"  %-22s [empty]\n", label); return; }

    char oldBuf[128];
    UINTN ol = AStrLen(orig);
    if (ol >= sizeof(oldBuf)) ol = sizeof(oldBuf) - 1;
    CopyMem(oldBuf, (VOID*)orig, ol);
    oldBuf[ol] = '\0';

    if (ol > 200) return;
    char buf[256];
    GenSerial(buf, ol);
    UINTN rl = AStrLen(buf);

    if (rl < ol)
    {
        EditString(table, field, buf);
    }
    else
    {
        char padded[256];
        UINTN copyLen = ol;
        if (copyLen > sizeof(padded) - 1) copyLen = sizeof(padded) - 1;
        CopyMem(padded, (VOID*)buf, copyLen);
        padded[copyLen] = '\0';
        EditString(table, field, padded);
    }

    Print(L"  %-22s '%a' → '%a'\n", label, oldBuf, buf);
}

static void SpoofSerialAtOffset(SMBIOS_STRUCTURE_POINTER table, UINTN off,
                                const CHAR16* label)
{
    if (!table.Raw || off >= table.Hdr->Length) return;
    SpoofSerial(table, (SMBIOS_STRING*)(table.Raw + off), label);
}

/* ── Type-by-type spoofers ───────────────────────────── */

/* Helper: spoof a string located at a fixed 1-based string index
   (used when the table has no byte-offset field pointing to it). */
static void SpoofStringIndex(SMBIOS_STRUCTURE_POINTER t, UINT8 strIdx,
                             const CHAR16 *label, const char **pool, UINTN poolCount)
{
    SMBIOS_STRING *fp = (SMBIOS_STRING*)&strIdx;
    SpoofField(t, fp, label, pool, poolCount);
}

void PatchType0(SMBIOS_STRUCTURE_TABLE* entry)
{
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 0, 0);
    Print(L"\n  ── BIOS Information (Type 0) ──\n");
    if (!t.Raw) { Print(L"  [not found]\n"); return; }
    SpoofField(t, &t.Type0->Vendor, L"Vendor", POOL_BIOS_VENDOR, sizeof(POOL_BIOS_VENDOR)/sizeof(POOL_BIOS_VENDOR[0]));
    SpoofField(t, &t.Type0->BiosVersion, L"BIOS Version", POOL_BIOS_VERSION, sizeof(POOL_BIOS_VERSION)/sizeof(POOL_BIOS_VERSION[0]));
    SpoofField(t, &t.Type0->BiosReleaseDate, L"Release Date", POOL_BIOS_DATE, sizeof(POOL_BIOS_DATE)/sizeof(POOL_BIOS_DATE[0]));
}

void PatchType1(SMBIOS_STRUCTURE_TABLE* entry)
{
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 1, 0);
    Print(L"\n  ── System Information (Type 1) ──\n");
    if (!t.Raw) { Print(L"  [not found]\n"); return; }
    SpoofField(t, &t.Type1->Manufacturer, L"Manufacturer", POOL_SYS_MFR, sizeof(POOL_SYS_MFR)/sizeof(POOL_SYS_MFR[0]));
    SpoofField(t, &t.Type1->ProductName, L"Product Name", POOL_SYS_MFR, sizeof(POOL_SYS_MFR)/sizeof(POOL_SYS_MFR[0]));
    SpoofField(t, &t.Type1->Version, L"Version", POOL_SYS_VERSION, sizeof(POOL_SYS_VERSION)/sizeof(POOL_SYS_VERSION[0]));
    SpoofSerial(t, &t.Type1->SerialNumber, L"Serial Number");
    if (t.Hdr->Length >= 0x09)
    {
        SpoofAtOffset(t, 0x08, L"SKU", POOL_SKU, sizeof(POOL_SKU)/sizeof(POOL_SKU[0]));
        SpoofAtOffset(t, 0x09, L"Family", POOL_FAMILY, sizeof(POOL_FAMILY)/sizeof(POOL_FAMILY[0]));
    }
}

void PatchType2(SMBIOS_STRUCTURE_TABLE* entry)
{
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 2, 0);
    Print(L"\n  ── Baseboard Information (Type 2) ──\n");
    if (!t.Raw) { Print(L"  [not found]\n"); return; }
    SpoofField(t, &t.Type2->Manufacturer, L"Manufacturer", POOL_MB_MFR, sizeof(POOL_MB_MFR)/sizeof(POOL_MB_MFR[0]));
    SpoofField(t, &t.Type2->ProductName, L"Product Name", POOL_MB_PROD, sizeof(POOL_MB_PROD)/sizeof(POOL_MB_PROD[0]));
    SpoofField(t, &t.Type2->Version, L"Version", POOL_MB_VER, sizeof(POOL_MB_VER)/sizeof(POOL_MB_VER[0]));
    SpoofSerial(t, &t.Type2->SerialNumber, L"Serial Number");
    if (t.Hdr->Length >= 0x09)
    {
        SpoofAtOffset(t, 0x08, L"Asset Tag", POOL_ASSET_TAG, sizeof(POOL_ASSET_TAG)/sizeof(POOL_ASSET_TAG[0]));
        SpoofAtOffset(t, 0x09, L"Location In Chassis", POOL_CHASSIS_LOC, sizeof(POOL_CHASSIS_LOC)/sizeof(POOL_CHASSIS_LOC[0]));
    }
}

void PatchType3(SMBIOS_STRUCTURE_TABLE* entry)
{
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 3, 0);
    Print(L"\n  ── System Enclosure (Type 3) ──\n");
    if (!t.Raw) { Print(L"  [not found]\n"); return; }
    SpoofField(t, &t.Type3->Manufacturer, L"Manufacturer", POOL_SYS_MFR, sizeof(POOL_SYS_MFR)/sizeof(POOL_SYS_MFR[0]));
    SpoofField(t, &t.Type3->Version, L"Version", POOL_SYS_VERSION, sizeof(POOL_SYS_VERSION)/sizeof(POOL_SYS_VERSION[0]));
    SpoofSerial(t, &t.Type3->SerialNumber, L"Serial Number");
    SpoofSerial(t, &t.Type3->AssetTag, L"Asset Tag");
    if (t.Hdr->Length >= 0x09)
        SpoofAtOffset(t, 0x08, L"SKU", POOL_SKU, sizeof(POOL_SKU)/sizeof(POOL_SKU[0]));
}

void PatchType4(SMBIOS_STRUCTURE_TABLE* entry)
{
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 4, 0);
    Print(L"\n  ── Processor Information (Type 4) ──\n");
    if (!t.Raw) { Print(L"  [not found]\n"); return; }

    SpoofField(t, &t.Type4->Socket, L"Socket", POOL_CPU_SOCKET, sizeof(POOL_CPU_SOCKET)/sizeof(POOL_CPU_SOCKET[0]));

    SpoofField(t, &t.Type4->ProcessorManufacture, L"Manufacturer", POOL_SYS_MFR, sizeof(POOL_SYS_MFR)/sizeof(POOL_SYS_MFR[0]));

    const char* origVer = GetStringAtIndex(t, t.Type4->ProcessorVersion);
    if (origVer)
    {
        char oldBuf[128];
        UINTN ol = AStrLen(origVer);
        if (ol >= sizeof(oldBuf)) ol = sizeof(oldBuf) - 1;
        CopyMem(oldBuf, (VOID*)origVer, ol);
        oldBuf[ol] = '\0';

        BOOLEAN matched = FALSE;
        for (UINTN i = 0; i < CPU_SWAP_COUNT; i++)
        {
            if (AStrEq(POOL_CPU_SWAP[i].match, origVer))
            {
                const char* repl = POOL_CPU_SWAP[i].replace;
                UINTN rl = AStrLen(repl);
                if (rl < ol)
                    EditString(t, &t.Type4->ProcessorVersion, repl);
                else
                {
                    char padded[256];
                    UINTN copyLen = ol < sizeof(padded)-1 ? ol : sizeof(padded)-1;
                    CopyMem(padded, (VOID*)repl, copyLen);
                    padded[copyLen] = '\0';
                    EditString(t, &t.Type4->ProcessorVersion, padded);
                }
                Print(L"  %-22s '%a' → '%a'\n", L"Processor Version", oldBuf, repl);
                matched = TRUE;
                break;
            }
        }
        if (!matched)
            Print(L"  %-22s '%a' [no downgrade match]\n", L"Processor Version", oldBuf);
    }

    UINT16 origMax = 0;
    if (t.Raw && t.Hdr->Length >= 0x15)
        origMax = *(UINT16*)(t.Raw + 0x14);
    if (origMax)
    {
        UINTN sc = sizeof(POOL_CPU_SPEED)/sizeof(POOL_CPU_SPEED[0]);
        UINT16 ns = POOL_CPU_SPEED[RandomNumber(0, (int)sc - 1)];
        UINTN safety = 0;
        while (ns >= origMax && safety < 20)
            { ns = POOL_CPU_SPEED[RandomNumber(0, (int)sc - 1)]; safety++; }
        if (ns < origMax)
        {
            Print(L"  %-22s %u MHz → %u MHz\n", L"Max Speed", origMax, ns);
            *(UINT16*)(t.Raw + 0x14) = ns;
            if (t.Hdr->Length >= 0x17)
            {
                UINT16 cur = *(UINT16*)(t.Raw + 0x16);
                if (cur)
                {
                    UINT16 cs = POOL_CPU_SPEED[RandomNumber(0, (int)sc - 1)];
                    safety = 0;
                    while (cs >= cur && safety < 20)
                        { cs = POOL_CPU_SPEED[RandomNumber(0, (int)sc - 1)]; safety++; }
                    if (cs < cur)
                    {
                        Print(L"  %-22s %u MHz → %u MHz\n", L"Current Speed", cur, cs);
                        *(UINT16*)(t.Raw + 0x16) = cs;
                    }
                }
            }
        }
    }

    SpoofSerialAtOffset(t, 0x20, L"Serial Number");
    if (t.Hdr->Length >= 0x22)
        SpoofAtOffset(t, 0x21, L"Asset Tag", POOL_CPU_ASSET, sizeof(POOL_CPU_ASSET)/sizeof(POOL_CPU_ASSET[0]));
    SpoofSerialAtOffset(t, 0x22, L"Part Number");
}

void PatchType7(SMBIOS_STRUCTURE_TABLE* entry)
{
    for (UINTN idx = 0; idx < 4; idx++)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 7, idx);
        if (!t.Raw || t.Hdr->Type != 7) break;
        Print(L"\n  ── Cache Information (Type 7) #%d ──\n", idx);
        SpoofAtOffset(t, 0x04, L"Socket", POOL_CACHE_SOCKET, sizeof(POOL_CACHE_SOCKET)/sizeof(POOL_CACHE_SOCKET[0]));
    }
}

void PatchType8(SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN idx = 0;
    while (idx < 16)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 8, idx);
        if (!t.Raw || t.Hdr->Type != 8) break;
        Print(L"\n  ── Port Connector (Type 8) #%d ──\n", idx);
        SpoofAtOffset(t, 0x04, L"Internal Ref", POOL_PORT_INT, sizeof(POOL_PORT_INT)/sizeof(POOL_PORT_INT[0]));
        SpoofAtOffset(t, 0x06, L"External Ref", POOL_PORT_EXT, sizeof(POOL_PORT_EXT)/sizeof(POOL_PORT_EXT[0]));
        idx++;
    }
}

void PatchType9(SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN idx = 0;
    while (idx < 16)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 9, idx);
        if (!t.Raw || t.Hdr->Type != 9) break;
        Print(L"\n  ── System Slots (Type 9) #%d ──\n", idx);
        SpoofAtOffset(t, 0x04, L"Designation", POOL_SLOT_DESIGNATION, sizeof(POOL_SLOT_DESIGNATION)/sizeof(POOL_SLOT_DESIGNATION[0]));
        idx++;
    }
}

void PatchType10(SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN idx = 0;
    while (idx < 16)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 10, idx);
        if (!t.Raw || t.Hdr->Type != 10) break;
        Print(L"\n  ── Onboard Device (Type 10) #%d ──\n", idx);
        for (UINTN off = 0x04; off + 1 < t.Hdr->Length; off += 2)
        {
            CHAR16 lbl[32];
            SPrint(lbl, 32, L"Description @0x%x", off + 1);
            SMBIOS_STRING* sf = (SMBIOS_STRING*)(t.Raw + off + 1);
            const char* orig = GetStringAtIndex(t, *sf);
            if (orig)
            {
                CHAR16 label[32];
                SPrint(label, 32, L"Device %d", idx);
                SpoofField(t, sf, label, POOL_ONBOARD_DEV, sizeof(POOL_ONBOARD_DEV)/sizeof(POOL_ONBOARD_DEV[0]));
            }
        }
        idx++;
    }
}

void PatchType11(SMBIOS_STRUCTURE_TABLE* entry)
{
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 11, 0);
    Print(L"\n  ── OEM Strings (Type 11) ──\n");
    if (!t.Raw || t.Hdr->Type != 11) { Print(L"  [not found]\n"); return; }
    UINTN sc = sizeof(POOL_OEM_STR)/sizeof(POOL_OEM_STR[0]);
    for (UINT8 si = 1; ; si++)
    {
        const char* s = GetStringAtIndex(t, si);
        if (!s) break;
        CHAR16 lbl[32];
        SPrint(lbl, 32, L"String %d", si);
        SpoofField(t, (SMBIOS_STRING*)&si, lbl, POOL_OEM_STR, sc);
    }
}

void PatchType13(SMBIOS_STRUCTURE_TABLE* entry)
{
    /* Type 13 has no string-index fields in the header.
       Strings are addressed directly by index 1 (installable list)
       and index 2 (current language). */
    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 13, 0);
    Print(L"\n  ── BIOS Language (Type 13) ──\n");
    if (!t.Raw || t.Hdr->Type != 13) { Print(L"  [not found]\n"); return; }
    SpoofStringIndex(t, 1, L"Installable", POOL_LANG_INSTALL, sizeof(POOL_LANG_INSTALL)/sizeof(POOL_LANG_INSTALL[0]));
    SpoofStringIndex(t, 2, L"Current", POOL_LANG_CURRENT, sizeof(POOL_LANG_CURRENT)/sizeof(POOL_LANG_CURRENT[0]));
}

void PatchType17(SMBIOS_STRUCTURE_TABLE* entry)
{
    /* String offsets verified against SMBIOS spec v3.7 Type 17: */
    UINTN idx = 0;
    while (idx < 8)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 17, idx);
        if (!t.Raw || t.Hdr->Type != 17) break;
        Print(L"\n  ── Memory Device (Type 17) #%d ──\n", idx);
        /* 0x10 = Device Locator (SMBIOS_STRING) */
        SpoofAtOffset(t, 0x10, L"Device Locator", POOL_MEM_LOCATOR, sizeof(POOL_MEM_LOCATOR)/sizeof(POOL_MEM_LOCATOR[0]));
        /* 0x11 = Bank Locator (SMBIOS_STRING) */
        SpoofAtOffset(t, 0x11, L"Bank Locator", POOL_MEM_BANK, sizeof(POOL_MEM_BANK)/sizeof(POOL_MEM_BANK[0]));
        /* Fields below require SMBIOS 2.6+ (Length >= 0x1B) */
        if (t.Hdr->Length >= 0x1B)
        {
            SpoofAtOffset(t, 0x17, L"Manufacturer", POOL_MEM_MFR, sizeof(POOL_MEM_MFR)/sizeof(POOL_MEM_MFR[0]));
            SpoofSerialAtOffset(t, 0x18, L"Serial Number");
            SpoofSerialAtOffset(t, 0x19, L"Asset Tag");
            SpoofAtOffset(t, 0x1A, L"Part Number", POOL_MEM_PART, sizeof(POOL_MEM_PART)/sizeof(POOL_MEM_PART[0]));
        }
        idx++;
    }
}

void PatchType22(SMBIOS_STRUCTURE_TABLE* entry)
{
    /* Offsets 0x04..0x09 verified against SMBIOS spec v3.7 Type 22: */
    UINTN idx = 0;
    while (idx < 4)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 22, idx);
        if (!t.Raw || t.Hdr->Type != 22) break;
        Print(L"\n  ── Portable Battery (Type 22) #%d ──\n", idx);
        SpoofAtOffset(t, 0x04, L"Location", POOL_BAT_LOCATION, sizeof(POOL_BAT_LOCATION)/sizeof(POOL_BAT_LOCATION[0]));
        SpoofAtOffset(t, 0x05, L"Manufacturer", POOL_BAT_MFR, sizeof(POOL_BAT_MFR)/sizeof(POOL_BAT_MFR[0]));
        SpoofAtOffset(t, 0x06, L"Manufacture Date", POOL_BAT_DATE, sizeof(POOL_BAT_DATE)/sizeof(POOL_BAT_DATE[0]));
        SpoofSerialAtOffset(t, 0x07, L"Serial Number");
        SpoofAtOffset(t, 0x08, L"Device Name", POOL_BAT_NAME, sizeof(POOL_BAT_NAME)/sizeof(POOL_BAT_NAME[0]));
        if (t.Hdr->Length >= 0x0A)
            SpoofAtOffset(t, 0x09, L"Chemistry", POOL_BAT_CHEM, sizeof(POOL_BAT_CHEM)/sizeof(POOL_BAT_CHEM[0]));
        idx++;
    }
}

void PatchType39(SMBIOS_STRUCTURE_TABLE* entry)
{
    /* Offset 0x04 is Power Unit Group (UINT8, not a string!).
       Strings start at 0x05. */
    UINTN idx = 0;
    while (idx < 4)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 39, idx);
        if (!t.Raw || t.Hdr->Type != 39) break;
        Print(L"\n  ── Power Supply (Type 39) #%d ──\n", idx);
        if (t.Hdr->Length >= 0x06)
            SpoofAtOffset(t, 0x05, L"Location", POOL_PSU_LOCATION, sizeof(POOL_PSU_LOCATION)/sizeof(POOL_PSU_LOCATION[0]));
        if (t.Hdr->Length >= 0x07)
            SpoofAtOffset(t, 0x06, L"Device Name", POOL_PSU_DEVNAME, sizeof(POOL_PSU_DEVNAME)/sizeof(POOL_PSU_DEVNAME[0]));
        if (t.Hdr->Length >= 0x08)
            SpoofAtOffset(t, 0x07, L"Manufacturer", POOL_PSU_MFR, sizeof(POOL_PSU_MFR)/sizeof(POOL_PSU_MFR[0]));
        if (t.Hdr->Length >= 0x09)
            SpoofSerialAtOffset(t, 0x08, L"Serial Number");
        if (t.Hdr->Length >= 0x0A)
            SpoofSerialAtOffset(t, 0x09, L"Asset Tag");
        if (t.Hdr->Length >= 0x0B)
            SpoofAtOffset(t, 0x0A, L"Model Part Number", POOL_PSU_MODEL, sizeof(POOL_PSU_MODEL)/sizeof(POOL_PSU_MODEL[0]));
        if (t.Hdr->Length >= 0x0C)
            SpoofAtOffset(t, 0x0B, L"Revision", POOL_PSU_REVISION, sizeof(POOL_PSU_REVISION)/sizeof(POOL_PSU_REVISION[0]));
        idx++;
    }
}

void PatchType41(SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN idx = 0;
    while (idx < 16)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 41, idx);
        if (!t.Raw || t.Hdr->Type != 41) break;
        Print(L"\n  ── Onboard Device Extended (Type 41) #%d ──\n", idx);
        SpoofAtOffset(t, 0x04, L"Reference", POOL_ONBOARD_REF, sizeof(POOL_ONBOARD_REF)/sizeof(POOL_ONBOARD_REF[0]));
        idx++;
    }
}

void PatchType43(SMBIOS_STRUCTURE_TABLE* entry)
{
    UINTN idx = 0;
    while (idx < 2)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 43, idx);
        if (!t.Raw || t.Hdr->Type != 43) break;
        Print(L"\n  ── TPM Device (Type 43) #%d ──\n", idx);
        if (t.Hdr->Length >= 0x15)
            SpoofAtOffset(t, 0x14, L"Description", POOL_TPM_DESC, sizeof(POOL_TPM_DESC)/sizeof(POOL_TPM_DESC[0]));
        idx++;
    }
}

/* ── Mini color helpers (standalone, no diagnostics.h dep) ── */

#define BK_COLOR_OK      0x0A
#define BK_COLOR_DIM     0x08
#define BK_COLOR_NORMAL  0x07

static void BkSetColor(UINTN a) { gST->ConOut->SetAttribute(gST->ConOut, a); }

/* ── Persistent HWID backup (UEFI NV variable) ───────── */
// At first spoof ever, all original SMBIOS string values
// are captured and stored in EFI variable "RdSmbiosOrig".
// "Reload Default HWID" reads the variable and writes
// every original string back into the in-memory tables.

#pragma pack(1)
typedef struct {
    UINT16 Type;
    UINT16 Handle;
    UINT8  StringIndex;
    CHAR8  Value[128];
} RD_BACKUP_ENTRY;
#pragma pack()

#define BACKUP_SIG   0x52445342  /* 'RDSB' */
#define BACKUP_VAR   L"RdSmbiosOrig"

static EFI_GUID gRdBackupGuid =
    { 0x726f7567, 0x616d, 0x2044,
      { 0x72, 0x61, 0x67, 0x6f, 0x6e, 0x42, 0x6b, 0x70 } };

/* ── Descriptor tables ───────────────────────────────── */

typedef struct {
    UINT8  Type;
    UINT8  MaxInst;
    UINT8  Offsets[16];
} FIELD_DESC;

/* The exact fields each PatchTypeX touches.
   Offsets are BYTE offsets in the SMBIOS table where the
   SMBIOS_STRING (UINT8 string-index) lives. Verifed against
   SMBIOS spec v3.7. */
static const FIELD_DESC SPOOF_TYPES[] = {
    /* Type 0: Vendor(04), BiosVersion(05), BiosReleaseDate(08)  */
    { 0, 1,  {0x04,0x05,0x08,           0xFF} },
    /* Type 1: Mfr(04), Product(05), Version(06), Serial(07), SKU(08), Family(09) */
    { 1, 1,  {0x04,0x05,0x06,0x07,0x08,0x09,0xFF} },
    /* Type 2: Mfr(04), Product(05), Version(06), Serial(07), AssetTag(08), Location(09) */
    { 2, 1,  {0x04,0x05,0x06,0x07,0x08,0x09,0xFF} },
    /* Type 3: Mfr(04), Version(05), Serial(06), AssetTag(07), SKU(08) */
    { 3, 1,  {0x04,0x05,0x06,0x07,0x08,0xFF} },
    /* Type 4: Socket(04), Mfr(07), Version(10), Serial(20), AssetTag(21), PartNumber(22) */
    { 4, 1,  {0x04,0x07,0x10,0x20,0x21,0x22,0xFF} },
    /* Type 7: Socket(04) */
    { 7, 4,  {0x04,                    0xFF} },
    /* Type 8: InternalRef(04), ExternalRef(06) */
    { 8, 16, {0x04,0x06,              0xFF} },
    /* Type 9: Designation(04) */
    { 9, 16, {0x04,                   0xFF} },
    /* Type 10: handled separately */
    /* Type 11: handled separately */
    /* Type 13: handled separately (strings are at fixed indices 1,2) */
    {13, 1,  {0xFF} }, /* no byte-offset string fields */
    /* Type 17: DeviceLocator(10), BankLocator(11), Mfr(17), Serial(18), AssetTag(19), PartNumber(1A) */
    {17, 8,  {0x10,0x11,0x17,0x18,0x19,0x1A,0xFF} },
    /* Type 22: Location(04), Mfr(05), Date(06), Serial(07), DeviceName(08) */
    {22, 4,  {0x04,0x05,0x06,0x07,0x08,0xFF} },
    /* Type 39: Location(05), DevName(06), Mfr(07), Serial(08), AssetTag(09),
                ModelPart(0A), Revision(0B)  note: 04 is PowerUnitGroup (UINT8, not string) */
    {39, 4,  {0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0xFF} },
    {41, 16, {0x04,                   0xFF} },
    {43, 2,  {0x14,                   0xFF} },
};

static BOOLEAN AddEntry(UINT8 *buf, UINTN *off, UINTN bufSize,
                        UINT16 type, UINT16 handle, UINT8 strIdx,
                        const char *str)
{
    if (*off + sizeof(RD_BACKUP_ENTRY) > bufSize) return FALSE;
    RD_BACKUP_ENTRY *e = (RD_BACKUP_ENTRY*)(buf + *off);
    e->Type       = type;
    e->Handle     = handle;
    e->StringIndex= strIdx;
    UINTN sl = 0;
    while (str[sl] && sl < 127) sl++;
    CopyMem(e->Value, (VOID*)str, sl);
    e->Value[sl] = '\0';
    *off += sizeof(RD_BACKUP_ENTRY);
    return TRUE;
}

BOOLEAN HasSmbiosBackup(void)
{
    UINTN vs = 0;
    EFI_STATUS st = gRT->GetVariable(BACKUP_VAR, &gRdBackupGuid, NULL, &vs, NULL);
    return (st == EFI_SUCCESS || st == EFI_BUFFER_TOO_SMALL);
}

void SaveSmbiosDefaults(SMBIOS_STRUCTURE_TABLE *entry)
{
    if (!entry || !entry->TableAddress) return;
    if (HasSmbiosBackup()) return;

    UINTN bufSize = 8 + 300 * sizeof(RD_BACKUP_ENTRY);
    UINT8 *buf = AllocatePool(bufSize);
    if (!buf) return;

    UINTN off = 8; /* skip header */
    UINT32 cnt = 0;

    SmbiosSetActiveTableBounds(entry);

    /* ── Regular types (table-driven) ── */
    for (UINTN ti = 0; ti < sizeof(SPOOF_TYPES)/sizeof(SPOOF_TYPES[0]); ti++)
    {
        UINT8 type = SPOOF_TYPES[ti].Type;
        for (UINTN inst = 0; inst < SPOOF_TYPES[ti].MaxInst; inst++)
        {
            SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, type, inst);
            if (!t.Raw || t.Hdr->Type != type) break;
            UINT16 handle = *(UINT16*)(t.Raw + 2);

            for (UINTN fi = 0; SPOOF_TYPES[ti].Offsets[fi] != 0xFF; fi++)
            {
                UINT8 foff = SPOOF_TYPES[ti].Offsets[fi];
                if (foff >= t.Hdr->Length) continue;
                UINT8 strIdx = *(UINT8*)(t.Raw + foff);
                const char *str = GetStringAtIndex(t, strIdx);
                if (!str) continue;
                if (!AddEntry(buf, &off, bufSize, type, handle, strIdx, str))
                    goto done;
                cnt++;
            }
        }
    }

    /* ── Type 10 (Onboard Device) ── */
    {
        UINT8 type = 10;
        for (UINTN inst = 0; inst < 16; inst++)
        {
            SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, type, inst);
            if (!t.Raw || t.Hdr->Type != type) break;
            UINT16 handle = *(UINT16*)(t.Raw + 2);
            for (UINTN foff = 0x04; foff + 1 < t.Hdr->Length; foff += 2)
            {
                UINT8 strIdx = *(UINT8*)(t.Raw + foff + 1);
                const char *str = GetStringAtIndex(t, strIdx);
                if (!str) continue;
                if (!AddEntry(buf, &off, bufSize, type, handle, strIdx, str))
                    goto done;
                cnt++;
            }
        }
    }

    /* ── Type 11 (OEM Strings) ── */
    {
        UINT8 type = 11;
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, type, 0);
        if (t.Raw && t.Hdr->Type == type)
        {
            UINT16 handle = *(UINT16*)(t.Raw + 2);
            for (UINT8 si = 1; ; si++)
            {
                const char *str = GetStringAtIndex(t, si);
                if (!str) break;
                if (!AddEntry(buf, &off, bufSize, type, handle, si, str))
                    goto done;
                cnt++;
            }
        }
    }

    /* ── Type 13 (BIOS Language) — fixed string indices 1,2 ── */
    {
        UINT8 type = 13;
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, type, 0);
        if (t.Raw && t.Hdr->Type == type)
        {
            UINT16 handle = *(UINT16*)(t.Raw + 2);
            for (UINT8 si = 1; si <= 2; si++)
            {
                const char *str = GetStringAtIndex(t, si);
                if (!str) break;
                if (!AddEntry(buf, &off, bufSize, type, handle, si, str))
                    goto done;
                cnt++;
            }
        }
    }

    /* ── Write header and save ── */
    *(UINT32*)(buf + 0) = BACKUP_SIG;
    *(UINT32*)(buf + 4) = cnt;

    gRT->SetVariable(BACKUP_VAR, &gRdBackupGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        off, buf);

done:
    FreePool(buf);
}

static SMBIOS_STRUCTURE_POINTER FindByHandle(const SMBIOS_STRUCTURE_TABLE *entry,
                                             UINT8 type, UINT16 handle)
{
    for (UINTN inst = 0; inst < 256; inst++)
    {
        SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, type, inst);
        if (!t.Raw || t.Hdr->Type != type) break;
        if (*(UINT16*)(t.Raw + 2) == handle) return t;
    }
    SMBIOS_STRUCTURE_POINTER nul = { .Raw = NULL };
    return nul;
}

void RestoreSmbiosDefaults(SMBIOS_STRUCTURE_TABLE *entry)
{
    if (!entry || !entry->TableAddress) { Print(L"\n[FAIL] No SMBIOS table\n"); return; }
    if (!HasSmbiosBackup()) { Print(L"\n[INFO] No backup found — spoof at least once first\n"); return; }

    UINTN vs = 0;
    EFI_STATUS st = gRT->GetVariable(BACKUP_VAR, &gRdBackupGuid, NULL, &vs, NULL);
    if (st == EFI_NOT_FOUND || vs < 8) return;

    UINT8 *buf = AllocatePool(vs);
    if (!buf) return;
    st = gRT->GetVariable(BACKUP_VAR, &gRdBackupGuid, NULL, &vs, buf);
    if (EFI_ERROR(st) || *(UINT32*)buf != BACKUP_SIG) { FreePool(buf); return; }

    UINT32 cnt = *(UINT32*)(buf + 4);
    UINTN  off = 8;

    Print(L"\n[WORK] Restoring original HWID from backup (%d fields)...\n", cnt);

    SmbiosSetActiveTableBounds(entry);
    UINTN restored = 0, skipped = 0;

    for (UINT32 i = 0; i < cnt; i++)
    {
        if (off + sizeof(RD_BACKUP_ENTRY) > vs) break;
        RD_BACKUP_ENTRY *e = (RD_BACKUP_ENTRY*)(buf + off);
        off += sizeof(RD_BACKUP_ENTRY);

        SMBIOS_STRUCTURE_POINTER t = FindByHandle(entry, (UINT8)e->Type, e->Handle);
        if (!t.Raw) { skipped++; continue; }

        const char *cur = GetStringAtIndex(t, e->StringIndex);
        if (!cur) { skipped++; continue; }

        Print(L"  %-22s ", L"");
        BkSetColor(BK_COLOR_OK);
        Print(L"'%a'", cur);
        BkSetColor(BK_COLOR_NORMAL);
        Print(L" → ");
        BkSetColor(BK_COLOR_DIM);
        Print(L"'%a'", (const char*)e->Value);
        BkSetColor(BK_COLOR_NORMAL);
        Print(L"\n");

        /* EditString needs a pointer to a byte containing the string index */
        UINT8 idxHolder = e->StringIndex;
        SMBIOS_STRING *field = (SMBIOS_STRING*)&idxHolder;

        UINTN curLen = StringLength(cur, 0);
        UINTN bakLen = StringLength((const char*)e->Value, 128);

        if (bakLen < curLen)
        {
            char padded[256];
            UINTN copyLen = curLen < sizeof(padded)-1 ? curLen : sizeof(padded)-1;
            CopyMem(padded, (VOID*)e->Value, copyLen);
            padded[copyLen] = '\0';
            EditString(t, field, padded);
        }
        else
        {
            EditString(t, field, (const char*)e->Value);
        }
        restored++;
    }

    Print(L"\n[OK] %d fields restored, %d skipped (table not found)\n", restored, skipped);
    FreePool(buf);
}

/* ── CPU speed raw-field backup (Type 4 offsets 0x14/0x16) ── */

#define CPU_SPEED_VAR  L"RdCpuSpeed"
static EFI_GUID gRdCpuSpeedGuid =
    { 0x63707573, 0x7065, 0x6564,
      { 0x42, 0x6b, 0x70, 0x52, 0x61, 0x69, 0x6e, 0x62 } };

BOOLEAN HasCpuSpeedBackup(void)
{
    UINTN vs = 0;
    EFI_STATUS st = gRT->GetVariable(CPU_SPEED_VAR, &gRdCpuSpeedGuid, NULL, &vs, NULL);
    return (st == EFI_SUCCESS || st == EFI_BUFFER_TOO_SMALL);
}

void SaveCpuSpeedBackup(SMBIOS_STRUCTURE_TABLE* entry)
{
    if (!entry || !entry->TableAddress || HasCpuSpeedBackup())
        return;

    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 4, 0);
    if (!t.Raw || t.Hdr->Length < 0x17)
        return;

    UINT16 backup[2];
    backup[0] = *(UINT16*)(t.Raw + 0x14);
    backup[1] = *(UINT16*)(t.Raw + 0x16);

    gRT->SetVariable(CPU_SPEED_VAR, &gRdCpuSpeedGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        sizeof(backup), backup);

    Print(L"  [BACKUP] CPU Max Speed: %u MHz, Current Speed: %u MHz\n",
          backup[0], backup[1]);
}

void RestoreCpuSpeedBackup(SMBIOS_STRUCTURE_TABLE* entry)
{
    if (!entry || !entry->TableAddress)
    {
        Print(L"\n[FAIL] No SMBIOS table\n");
        return;
    }
    if (!HasCpuSpeedBackup())
    {
        Print(L"\n[INFO] No CPU speed backup found — spoof at least once first\n");
        return;
    }

    UINTN vs = sizeof(UINT16) * 2;
    UINT16 backup[2];
    EFI_STATUS st = gRT->GetVariable(CPU_SPEED_VAR, &gRdCpuSpeedGuid, NULL, &vs, backup);
    if (EFI_ERROR(st) || vs != sizeof(UINT16) * 2)
    {
        Print(L"\n[FAIL] CPU speed backup corrupted\n");
        return;
    }

    SMBIOS_STRUCTURE_POINTER t = FindTableByType(entry, 4, 0);
    if (!t.Raw || t.Hdr->Length < 0x17)
    {
        Print(L"\n[FAIL] CPU Type 4 table not found or too short\n");
        return;
    }

    UINT16 curMax = *(UINT16*)(t.Raw + 0x14);
    UINT16 curCur = *(UINT16*)(t.Raw + 0x16);

    *(UINT16*)(t.Raw + 0x14) = backup[0];
    if (t.Hdr->Length >= 0x17)
        *(UINT16*)(t.Raw + 0x16) = backup[1];

    Print(L"\n[OK] CPU Speed restored: Max %u → %u MHz, Current %u → %u MHz\n",
          curMax, backup[0], curCur, backup[1]);
}

/* ── Entry point ─────────────────────────────────────── */

void PatchAll(SMBIOS_STRUCTURE_TABLE* entry)
{
    if (!entry || !entry->TableAddress)
    {
        Print(L"\n[FAIL] No SMBIOS table available\n");
        return;
    }

    /* Save original values to NV variable on first run */
    SaveSmbiosDefaults(entry);
    SaveCpuSpeedBackup(entry);

    Print(L"\n[WORK] Smart-spoofing all SMBIOS tables...\n");

    PatchType0(entry);
    PatchType1(entry);
    PatchType2(entry);
    PatchType3(entry);
    PatchType4(entry);
    PatchType7(entry);
    PatchType8(entry);
    PatchType9(entry);
    PatchType10(entry);
    PatchType11(entry);
    PatchType13(entry);
    PatchType17(entry);
    PatchType22(entry);
    PatchType39(entry);
    PatchType41(entry);
    PatchType43(entry);

    Print(L"\n[OK] Smart spoofing complete — all changes are in-memory only\n");
}
