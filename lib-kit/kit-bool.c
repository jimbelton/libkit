#include "kit-bool.h"

bool
kit_bool_from_strn(bool *val, const char *txt, unsigned len)
{
    if ((len == 1 && strncasecmp(txt, "1", len) == 0)
     || (len == 3 && strncasecmp(txt, "yes", len) == 0)
     || (len == 4 && strncasecmp(txt, "true", len) == 0))
        return *val = true;

    if ((len == 1 && strncasecmp(txt, "0", len) == 0)
     || (len == 2 && strncasecmp(txt, "no", len) == 0)
     || (len == 5 && strncasecmp(txt, "false", len) == 0)) {
        *val = false;
        return true;
    }

    return false;
}
