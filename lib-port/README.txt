This directory is based on the portability package in libsxe. The portability philosophy is different:
 1. Linux/glibc/gcc are considered the "right" platform/tool chain.
 2. Functions from other operating systems/tool chains (e.g. spawn) are implemented as kit_ functions here.
 3. Functions missing in other operating systems/tool chains are emulated here (e.g. kit-port.h)
 4. The Windows/Visual C port has not been tested, and almost certainly needs work to make it usable.
