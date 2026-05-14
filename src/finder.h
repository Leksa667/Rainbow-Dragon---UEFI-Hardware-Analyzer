#ifndef FINDER_H
#define FINDER_H

#include "smbios.h"

void* FindByHob(void);
void* FindByConfig(void);
SMBIOS_STRUCTURE_TABLE* FindEntry(void);

#endif
