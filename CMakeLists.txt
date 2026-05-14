cmake_minimum_required(VERSION 3.16)
project(rainbow_dragon LANGUAGES C)

set(EFIBIN "DragonTool.efi")

include_directories(/usr/include/efi)
include_directories(/usr/include/efi/x86_64)
include_directories(/usr/include/efi/protocol)

link_directories(/usr/lib)

set(COMMONFLAGS "-DGNU_EFI_USE_MS_ABI -fno-stack-protector -fpic -fshort-wchar -mno-red-zone -Wall -Werror -Wextra")
set(LDFLAGS "-Wl,-nostdlib -Wl,-znocombreloc -Wl,-T,/usr/lib/elf_x86_64_efi.lds -Wl,-shared -Wl,-Bsymbolic,/usr/lib/crt0-efi-x86_64.o -Wl,-lefi -Wl,-lgnuefi")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c17 ${COMMONFLAGS}")

file(GLOB_RECURSE sourcefiles
    "*.h"
    "*.c"
)

add_library(rainbow_dragon MODULE ${sourcefiles})
target_link_libraries(rainbow_dragon ${LDFLAGS})

# CMake adds -fPIC, -shared, -soname automatically for SHARED libs.
# MODULE avoids those flags since UEFI has its own linking rules.

set_target_properties(rainbow_dragon PROPERTIES
    PREFIX ""
    SUFFIX ".so"
)

add_custom_command(
    TARGET rainbow_dragon POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
        -I elf64-x86-64 -O pei-x86-64 --subsystem=10
        -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym
        -j .rel -j .rela -j .reloc
        rainbow_dragon.so ${EFIBIN}
)
