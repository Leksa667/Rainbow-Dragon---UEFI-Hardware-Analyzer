#include "general.h"
#include "utils.h"

static UINTN g_RngState = 0;
static EFI_GUID gRngProtocolGuid = { 0x3152bca5, 0xe561, 0x4351, { 0x82, 0x19, 0x24, 0x1d, 0x80, 0x34, 0x3d, 0xe4 } };

static void SeedRngIfNeeded(void)
{
    if (g_RngState != 0)
        return;

    EFI_RNG_PROTOCOL *Rng = NULL;
    if (!EFI_ERROR(gBS->LocateProtocol(&gRngProtocolGuid, NULL, (VOID**)&Rng)) && Rng)
    {
        UINT64 val = 0;
        if (!EFI_ERROR(Rng->GetRNG(Rng, NULL, sizeof(val), (UINT8*)&val)))
        {
            g_RngState = (UINTN)val;
            return;
        }
    }

    EFI_TIME time;
    EFI_TIME_CAPABILITIES caps;
    gRT->GetTime(&time, &caps);

    g_RngState = (UINTN)time.Nanosecond
               ^ ((UINTN)time.Day    << 16)
               ^ ((UINTN)time.Hour   <<  8)
               ^ ((UINTN)time.Minute <<  4)
               ^ (UINTN)time.Second;
}

int RandomNumber(int lower, int upper)
{
    SeedRngIfNeeded();

    g_RngState = g_RngState * 6364136223846793005ULL + 1442695040888963407ULL;

    UINTN range = (UINTN)(upper - lower + 1);
    return lower + (int)(g_RngState % range);
}

void RandomText(char* buffer, int length)
{
    if (length < 1)
        return;

    for (int i = 0; i < length; i++)
    {
        buffer[i] = (char)RandomNumber(49, 90);
    }

    buffer[length] = '\0';
}

static const char ALPHANUM[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

void RandomTextAlphanumeric(char* buffer, int length)
{
    if (length < 1)
        return;

    int charsetSize = (int)(sizeof(ALPHANUM) - 1);

    for (int i = 0; i < length; i++)
    {
        buffer[i] = ALPHANUM[RandomNumber(0, charsetSize - 1)];
    }

    buffer[length] = '\0';
}
