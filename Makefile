ARCH     ?= x86_64
CC       := gcc
OBJCOPY  := objcopy
EFIBIN   := DragonTool.efi

EFI_LDS_x86_64      := /usr/lib/elf_x86_64_efi.lds
EFI_CRT_x86_64      := /usr/lib/crt0-efi-x86_64.o
EFI_INPUT_x86_64    := elf64-x86-64
EFI_OBJCOPY_x86_64  := pei-x86-64
EFI_CFLAGS_x86_64   := -mno-red-zone -fno-asynchronous-unwind-tables

EFI_LDS_aarch64     := /usr/lib/elf_aarch64_efi.lds
EFI_CRT_aarch64     := /usr/lib/crt0-efi-aarch64.o
EFI_INPUT_aarch64   := elf64-littleaarch64
EFI_OBJCOPY_aarch64 := pei-aarch64-little
EFI_CFLAGS_aarch64  :=

COMMON_CFLAGS := --std=c17 -DGNU_EFI_USE_MS_ABI -fno-stack-protector -fpic -fshort-wchar -Wall -Werror -Wextra
CFLAGS        := $(COMMON_CFLAGS) $(EFI_CFLAGS_$(ARCH))
LDFLAGS       := -Wl,-nostdlib -Wl,-znocombreloc -Wl,-T,$(EFI_LDS_$(ARCH)) -Wl,-shared -Wl,-Bsymbolic,$(EFI_CRT_$(ARCH)) -Wl,-lefi -Wl,-lgnuefi
INCDIRS       := -I/usr/include/efi -I/usr/include/efi/$(ARCH) -I/usr/include/efi/protocol

SRCDIR   := src
BUILDDIR := build
SRCS     := $(wildcard $(SRCDIR)/*.c)
OBJS     := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean

all: $(BUILDDIR)/$(EFIBIN)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) $(INCDIRS) -c $< -o $@

$(BUILDDIR)/DragonTool.so: $(OBJS)
	$(CC) -nostdlib -shared $^ $(LDFLAGS) -o $@

$(BUILDDIR)/$(EFIBIN): $(BUILDDIR)/DragonTool.so
	$(OBJCOPY) -I $(EFI_INPUT_$(ARCH)) -O $(EFI_OBJCOPY_$(ARCH)) --subsystem=10 -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym -j .rel -j .rela -j .reloc $< $@
	@python3 -c "import struct; d=bytearray(open('$@','rb').read()); p=struct.unpack_from('<I',d,0x3C)[0]; struct.pack_into('<H',d,p+22,0x2022); open('$@','wb').write(d)"
	@echo "[OK] Built $@"

clean:
	rm -rf $(BUILDDIR)
