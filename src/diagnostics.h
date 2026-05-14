#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "general.h"
#include "smbios.h"

void DiagnosticsSetImageHandle(EFI_HANDLE imageHandle);
void DisplayOverview(const SMBIOS_STRUCTURE_TABLE* entry);
void DisplayAnalysisBrowser(const SMBIOS_STRUCTURE_TABLE* entry);
void DisplayHardwareReport(const SMBIOS_STRUCTURE_TABLE* entry);
void DisplayMemoryMap(void);
void DisplayBootEnvironment(void);
void DisplayAcpiTables(void);
void DisplayFirmwareAndGraphics(const SMBIOS_STRUCTURE_TABLE* entry);
void DisplayCompatibilityMatrix(const SMBIOS_STRUCTURE_TABLE* entry);

void PgReset(void);
BOOLEAN WasPgRestarted(void);
void ShowLetsaMod(void);
void ShowBootDriverScanner(void);

#endif
