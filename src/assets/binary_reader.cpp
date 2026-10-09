#include "assets/binary_reader.h"

#include <cstring>

// Intentionally left as a header-mostly utility; this TU exists so that
// <cstring> usage has one home, and to give the library a stable translation
// unit while the class is header-inline.
