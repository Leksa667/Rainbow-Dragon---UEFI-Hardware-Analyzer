#include "general.h"
#include "finder.h"
#include "smbios.h"
#include "diagnostics.h"
#include "display.h"
#include "menu.h"

#define COLOR_NORMAL   0x07
#define COLOR_SUCCESS  0x0A
#define COLOR_WARNING  0x0C

static void SetColor(UINTN attr)
{
    gST->ConOut->SetAttribute(gST->ConOut, attr);
}

static SMBIOS_STRUCTURE_TABLE* g_SmbiosEntry = NULL;

static void SetBestConsoleMode(void)
{
    UINTN bestMode = 0;
    UINTN bestCols = 80;
    UINTN mode = 0;

    while (TRUE)
    {
        UINTN cols, rows;
        EFI_STATUS status = gST->ConOut->QueryMode(gST->ConOut, mode, &cols, &rows);

        if (EFI_ERROR(status))
            break;

        if (cols > bestCols)
        {
            bestCols = cols;
            bestMode = mode;
        }

        mode++;
    }

    gST->ConOut->SetMode(gST->ConOut, bestMode);
}

static SMBIOS_STRUCTURE_TABLE* GetSmbiosEntry(void)
{
    if (!g_SmbiosEntry)
    {
        SetColor(COLOR_SUCCESS);
        Print(L"[*] Scanning for SMBIOS tables...\n");
        SetColor(COLOR_NORMAL);
        g_SmbiosEntry = FindEntry();
    }

    return g_SmbiosEntry;
}

static void RunWithRestart(void (*fn)(void))
{
    do {
        PgReset();
        fn();
    } while (WasPgRestarted());
}

static void RunWithRestartEntry(void (*fn)(const SMBIOS_STRUCTURE_TABLE*), SMBIOS_STRUCTURE_TABLE* entry)
{
    do {
        PgReset();
        fn(entry);
    } while (WasPgRestarted());
}

static void ShowOverview(void)
{
    SMBIOS_STRUCTURE_TABLE* entry = GetSmbiosEntry();
    RunWithRestartEntry(DisplayOverview, entry);
}

static void ShowHardwareReport(void)
{
    SMBIOS_STRUCTURE_TABLE* entry = GetSmbiosEntry();
    RunWithRestartEntry(DisplayHardwareReport, entry);
}

static void ShowSmbiosDetails(void)
{
    SMBIOS_STRUCTURE_TABLE* entry = GetSmbiosEntry();

    if (!g_SmbiosEntry)
    {
        SetColor(COLOR_WARNING);
        Print(L"[!] No SMBIOS tables found on this system\n");
        SetColor(COLOR_NORMAL);
        Print(L"[i] This system may not expose SMBIOS via standard UEFI interfaces\n");
        WaitForAnyKey();
        return;
    }

    SetColor(COLOR_SUCCESS);
    Print(L"[+] SMBIOS v%u.%u at 0x%08x\n",
          entry->MajorVersion, entry->MinorVersion,
          (UINTN)entry->TableAddress);
    SetColor(COLOR_NORMAL);

    RunWithRestartEntry(DisplayAllTables, entry);
}

static void ShowMemoryMap(void)
{
    RunWithRestart(DisplayMemoryMap);
}

static void ShowBootEnvironment(void)
{
    RunWithRestart(DisplayBootEnvironment);
}

static void ShowAcpiTables(void)
{
    RunWithRestart(DisplayAcpiTables);
}

static void ShowFirmwareDetails(void)
{
    SMBIOS_STRUCTURE_TABLE* entry = GetSmbiosEntry();
    RunWithRestartEntry(DisplayFirmwareAndGraphics, entry);
}

static void ShowCompatibility(void)
{
    SMBIOS_STRUCTURE_TABLE* entry = GetSmbiosEntry();
    RunWithRestartEntry(DisplayCompatibilityMatrix, entry);
}

#if 0
/* Spoofing is intentionally disabled; kept only as archived code. */
static void SpoofAll(void)
{
    if (!g_SmbiosEntry)
    {
        SetColor(COLOR_SUCCESS);
        Print(L"[*] Scanning for SMBIOS tables...\n");
        SetColor(COLOR_NORMAL);
        g_SmbiosEntry = FindEntry();
    }

    if (!g_SmbiosEntry)
    {
        SetColor(COLOR_WARNING);
        Print(L"[!] No SMBIOS tables found. Spoofing impossible on this system.\n");
        SetColor(COLOR_NORMAL);
        WaitForAnyKey();
        return;
    }

    if (!ConfirmSpoof())
    {
        Print(L"[i] Spoof cancelled\n");
        return;
    }

    PatchAll(g_SmbiosEntry);
    Print(L"\n");
    WaitForAnyKey();
}
#endif

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);
    DiagnosticsSetImageHandle(ImageHandle);

    SetBestConsoleMode();
    SetColor(COLOR_NORMAL);

    while (TRUE)
    {
        MenuChoice choice = ShowMainMenu();

        switch (choice)
        {
        case MENU_OVERVIEW:
            ShowOverview();
            break;

        case MENU_ANALYSIS_BROWSER:
            DisplayAnalysisBrowser(GetSmbiosEntry());
            break;

        case MENU_HARDWARE:
            ShowHardwareReport();
            break;

        case MENU_SMBIOS:
            ShowSmbiosDetails();
            break;

        case MENU_MEMORY:
            ShowMemoryMap();
            break;

        case MENU_BOOT:
            ShowBootEnvironment();
            break;

        case MENU_ACPI:
            ShowAcpiTables();
            break;

        case MENU_FIRMWARE:
            ShowFirmwareDetails();
            break;

        case MENU_COMPATIBILITY:
            ShowCompatibility();
            break;

        case MENU_LEKSA:
            ShowLetsaMod();
            break;

        case MENU_EXIT:
        {
            SetColor(COLOR_NORMAL);
            Print(L"\n  Rebooting to Windows...\n\n");
            gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
            break;
        }

        default:
            break;
        }
    }
}
