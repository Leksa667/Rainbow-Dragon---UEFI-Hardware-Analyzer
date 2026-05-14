#ifndef MENU_H
#define MENU_H

#include "general.h"

typedef enum
{
    MENU_EXIT = 0,
    MENU_OVERVIEW,
    MENU_ANALYSIS_BROWSER,
    MENU_HARDWARE,
    MENU_SMBIOS,
    MENU_MEMORY,
    MENU_BOOT,
    MENU_ACPI,
    MENU_FIRMWARE,
    MENU_COMPATIBILITY,
    MENU_LEKSA,
#if 0
    /* Spoofing is intentionally disabled; kept only as archived code. */
    MENU_SPOOF_ALL,
#endif
    MENU_INVALID = 0xFF
} MenuChoice;

MenuChoice ShowMainMenu(void);
#if 0
/* Spoofing is intentionally disabled; kept only as archived code. */
BOOLEAN ConfirmSpoof(void);
#endif

#endif
