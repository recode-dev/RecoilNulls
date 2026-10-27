#include "scstring.h"
#include "offsets.h"
#include "scan.h"
#define SCSTRING_INLINE_MAX 7
int read_scstring(uintptr_t value, size_t maxLength, char* out, size_t outSize)
{
    int32_t length = 0;
    uintptr_t data = 0;
    if (!value || !out || !outSize) return 0;
    if (!read_int(value + SCSTRING_LENGTH_OFF, &length)) return 0;
    if (length <= 0 || (size_t)length > maxLength) return 0;
    if ((size_t)length >= outSize) return 0;
    if (length <= SCSTRING_INLINE_MAX)
    {
        data = value + SCSTRING_DATA_OFF;
    }
    else if (!read_ptr(value + SCSTRING_DATA_OFF, (void**)&data) || !data)
    {
        return 0;
    }
    if (!read_bytes(data, out, (size_t)length)) return 0;
    out[length] = '\0';
    return 1;
}
