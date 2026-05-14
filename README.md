# Rainbow Dragon - UEFI Hardware Analyzer

  Rainbow Dragon is a UEFI hardware and firmware analysis utility written in C using GNU-EFI.
  It runs directly in the pre-boot UEFI environment and provides detailed visibility into low-level system information before the operating system starts.

  The tool is focused on firmware research, hardware diagnostics, SMBIOS/ACPI inspection, boot environment analysis, and UEFI compatibility testing.

  ## Overview

  Rainbow Dragon is built as a standalone UEFI application: `DragonTool.efi`.

  Once launched from an EFI shell or bootable FAT partition, it opens an interactive text-based interface that allows the user to inspect firmware-exposed hardware data directly from UEFI.

  The project is read-only by default in its current interface and is mainly intended as an analysis and diagnostic tool. Some SMBIOS patching/research code exists in the source tree, but it is not exposed as the main public workflow.
  
  ## Credits

  Rainbow Dragon was built from an initial foundation based on the Negative Spoofer project by Samuel Tulach.

  Special thanks to Samuel Tulach for the original Negative Spoofer project, which provided the base work.

  Original project: https://github.com/SamuelTulach/negativespoofer
  
  ## Features

  - Interactive UEFI menu interface
  - System overview screen
  - SMBIOS table detection and navigation
  - Detailed SMBIOS hardware report
  - ACPI table scanning and inspection
  - UEFI memory map viewer
  - Boot environment diagnostics
  - Firmware, graphics, GOP and runtime service information
  - Compatibility and firmware capability checks
  - Hardware identity inspection
  - GNU-EFI based build system
  - Docker build support for easier compilation on Windows/Linux hosts
  - Boot helper script via `startup.nsh`

  ## What It Can Inspect

  Rainbow Dragon can display and analyze information such as:

  - BIOS and firmware vendor data
  - System manufacturer, product name and UUID
  - Baseboard information
  - Chassis information
  - CPU information
  - Memory devices and memory arrays
  - TPM SMBIOS entries
  - PCI/slot information
  - ACPI root tables
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

  The project can be built with GNU-EFI using the included `Makefile`.

  ```sh
  make

  The output file is generated as:

  build/DragonTool.efi

  A Docker-based build helper is also included:

  .\scripts\docker-build.ps1

  ## Boot Usage

  Copy the generated DragonTool.efi to a FAT32 EFI partition or USB drive.

  Example layout:

  /
  ├── DragonTool.efi
  └── startup.nsh

  The included startup.nsh launches:

  DragonTool.efi

  Then boot into an EFI shell and run the tool.

  ## Disclaimer

  Rainbow Dragon is provided for educational, diagnostic, and firmware research purposes only.

  Use it only on systems you own or are explicitly authorized to test. Low-level firmware tools can expose sensitive hardware information and may behave differently depending on the motherboard, firmware version, Secure Boot state, and UEFI implementation.

  The author is not responsible for misuse, data loss, hardware issues, firmware problems, or violations of third-party terms of service.
