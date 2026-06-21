# Rainbow Dragon - UEFI Hardware Analyzer

Rainbow Dragon is a UEFI hardware and firmware analysis utility written in C using GNU-EFI.
It runs directly in the pre-boot UEFI environment and provides low-level visibility into firmware-exposed system data before the operating system starts.

The project focuses on firmware research, hardware diagnostics, SMBIOS/ACPI inspection, UEFI compatibility testing, and controlled in-memory SMBIOS/ACPI testing with backup, diff, confirmation, and rollback safeguards.

## Preview



<img width="874" height="1180" alt="rainbowdragon" src="https://github.com/user-attachments/assets/2544c888-b226-48fc-9412-3f6a17711bdb" />



## Overview

Rainbow Dragon builds as a standalone UEFI application:

```txt
DragonTool.efi
```

Once launched from an EFI shell or a bootable FAT32 EFI partition, it opens an interactive text-based interface for inspecting system firmware data directly from UEFI.

The tool includes read-focused diagnostics as well as controlled in-memory SMBIOS and ACPI test actions. Mutable actions require confirmation and are designed to keep an original backup before changing supported fields.

## Features

- Interactive UEFI menu interface
- System overview screen
- SMBIOS table detection, browsing, and detailed reporting
- ACPI table scanning and inspection
- UEFI memory map viewer
- Boot environment diagnostics
- Firmware, GOP graphics, runtime, and boot service information
- Compatibility and firmware capability checks
- Early UEFI/DXE loaded image scanner
- SMBIOS in-memory test patching with confirmation
- Original SMBIOS backup stored before the first spoof action
- System UUID backup, diff, spoof, and restore support
- CPU speed raw-field backup and restore support
- Before/after SMBIOS diff view
- Automatic rollback when an internal SMBIOS patch error is detected
- ACPI DSDT/SSDT OEM header patching with checksum validation and immediate rollback on checksum failure
- GNU-EFI Makefile build
- Docker build support for Windows/Linux hosts
- Boot helper script via `startup.nsh`

## Recent Changes

- Added confirmation prompts before SMBIOS and ACPI spoof actions.
- Added SMBIOS backup validation before patching.
- Added System UUID backup, spoof, diff, restore, and rollback support.
- Added CPU speed raw-field backup/restore integration into default restore.
- Added before/after diff output against saved defaults.
- Added automatic SMBIOS rollback when an internal patch error is detected.
- Added ACPI checksum validation with immediate header rollback on failure.
- Improved UI readability by keeping text bright while leaving separators dark.

## Current SMBIOS Coverage

Rainbow Dragon can inspect and, for supported in-memory test actions, patch selected fields from:

- Type 0: BIOS information
- Type 1: System information, including System UUID
- Type 2: Baseboard information
- Type 3: Chassis information
- Type 4: Processor information and selected CPU speed fields
- Type 7: Cache socket strings
- Type 8: Port connector strings
- Type 9: System slot designation strings
- Type 10: Onboard device strings
- Type 11: OEM strings
- Type 13: BIOS language strings
- Type 17: Memory device string fields
- Type 22: Portable battery string fields
- Type 39: Power supply string fields
- Type 41: Onboard extended device references
- Type 43: TPM device description

## Safety Behavior

Rainbow Dragon applies several safeguards before and after mutable actions:

- Smart SMBIOS spoof requires explicit confirmation.
- ACPI DSDT/SSDT spoof requires explicit confirmation.
- Original SMBIOS string values are saved before the first SMBIOS patch.
- Existing backups are kept and are not overwritten by later runs.
- System UUID has its own raw 16-byte backup and restore path.
- CPU speed raw fields have their own backup and restore path.
- The `Show HWID Diff From Backup` menu compares current values against saved defaults.
- If an internal SMBIOS patch error is detected, Rainbow Dragon restores saved defaults immediately.
- ACPI table headers are checksum-validated after patching; failed checksum validation restores the original header fields immediately.
- Changes are in-memory and should reset after reboot, except backup variables stored in UEFI NVRAM for restore support.

## What It Can Inspect

Rainbow Dragon can display and analyze information such as:

- BIOS and firmware vendor data
- System manufacturer, product name, serial, SKU, family, and UUID
- Baseboard information
- Chassis information
- CPU information
- Memory devices and memory arrays
- TPM SMBIOS entries
- PCI and slot information exposed through SMBIOS
- ACPI root tables and DSDT/SSDT headers
- UEFI system table data
- UEFI runtime and boot service availability
- Graphics Output Protocol information
- UEFI memory descriptors
- Boot path and loaded image information

## Use Cases

This project can be useful for:

- Firmware and UEFI research
- Debugging SMBIOS or ACPI exposure
- Inspecting hardware identity data before OS boot
- Testing UEFI compatibility on different systems
- Learning how firmware tables are exposed through UEFI
- Building custom pre-boot diagnostics tools
- Comparing firmware behavior between machines or BIOS versions

## Build

### Docker Build

On Windows with Docker Desktop:

```powershell
.\scripts\docker-build.ps1
```

The output file is generated as:

```txt
build\DragonTool.efi
```

### Native GNU-EFI Build

On a Linux environment with GNU-EFI installed:

```sh
make
```

The output file is generated as:

```txt
build/DragonTool.efi
```

## Boot Usage

Copy the generated `DragonTool.efi` to a FAT32 EFI partition or USB drive.

Example layout:

```txt
/
|- DragonTool.efi
`- startup.nsh
```

The included `startup.nsh` launches:

```txt
DragonTool.efi
```

Then boot into an EFI shell and run the tool.

## Credits

Rainbow Dragon was built from an initial foundation based on the Negative Spoofer project by Samuel Tulach.

Special thanks to Samuel Tulach for the original Negative Spoofer project, which provided the base work.

Original project:

```txt
https://github.com/SamuelTulach/negativespoofer
```

## Disclaimer

Rainbow Dragon is provided for educational, diagnostic, and firmware research purposes only.

Use it only on systems you own or are explicitly authorized to test. Low-level firmware tools can expose sensitive hardware information and may behave differently depending on the motherboard, firmware version, Secure Boot state, and UEFI implementation.

The author is not responsible for misuse, data loss, hardware issues, firmware problems, or violations of third-party terms of service.
