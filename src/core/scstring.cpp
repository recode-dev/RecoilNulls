#include "scstring.h"
#include "offsets.h"
#include "scan.h"
inline constexpr int SCSTRING_INLINE_MAX = 7;
int read_scstring(uintptr_t value, size_t maxLength, char* out, size_t outSize)
{
    int32_t   length = 0;
    uintptr_t data   = 0;
    if (!value || !out || !outSize) return 0;
    if (!read_int(value + StringByteLengthOffset, &length)) return 0;
    if (length <= 0 || static_cast<size_t>(length) > maxLength) return 0;
    if (static_cast<size_t>(length) >= outSize) return 0;
    if (length <= SCSTRING_INLINE_MAX)
    {
        data = value + StringCharactersOffset;
    }
    else if (!read_ptr(value + StringCharactersOffset,
                       reinterpret_cast<void**>(&data)) ||
             !data)
    {
        return 0;
    }
    if (!read_bytes(data, out, static_cast<size_t>(length))) return 0;
    out[length] = '\0';
    return 1;
}
