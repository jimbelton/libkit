#include <sxe-util.h>
#include <string.h>
#include <tap.h>

#include "kit.h"

int
main(int argc, char **argv)
{
    size_t blen, tlen;
    uint8_t bin[50];
    char txt[100];
    unsigned i;

    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    plan_tests(61);

    diag("The bin2hex/hex2bin interface");
    {
        is(kit_bin2hex(txt, (const uint8_t *)"\1\2\3\4", 4, KIT_BIN2HEX_LOWER), 8, "Translated a small binary string to hex");
        is_eq(txt, "01020304", "The hex data is correct");
        is(kit_hex2bin(bin, txt, 8), 4, "Translated it back to binary");
        ok(memcmp(bin, "\1\2\3\4", 4) == 0, "The binary is the same as the original");
        is(kit_bin2hex(txt, (const uint8_t *)"\12\24\36\50", 4, KIT_BIN2HEX_LOWER), 8, "Translated a small binary string to lowercase hex");
        is_eq(txt, "0a141e28", "The hex data is correct");
        is(kit_hex2bin(bin, txt, 8), 4, "Translated it back to binary");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");
        is(kit_bin2hex(txt, (const uint8_t *)"\12\24\36\50", 4, KIT_BIN2HEX_UPPER), 8, "Translated a small binary string to uppercase hex");
        is_eq(txt, "0A141E28", "The hex data is correct");

        memset(bin, '\0', 4);
        is(kit_hex2bin(bin, txt, 8), 4, "Translated it back to binary");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");

        memset(bin, '\0', 4);
        is(kit_hex2bin(bin, txt, 6), 3, "Translated part of it back to binary");
        ok(memcmp(bin, "\12\24\36", 3) == 0, "The binary is the same as the original");

        memset(bin, '\0', 4);
        is(kit_hex2bin(bin, txt, 7), 3, "Translated an odd number of characters back to binary - only read 3 binary bytes");
        ok(memcmp(bin, "\12\24\36", 3) == 0, "The binary is the same as the original");
    }

    diag("The base16encode/base16decode interface");
    {
        memset(txt, '\0', 8);
        blen = 4;
        tlen = sizeof(txt);
        is(kit_base16encode(txt, &tlen, (const uint8_t *)"\12\24\36\50", &blen), NULL, "Got no error from kit_base16encode()");
        is(tlen, 8, "tlen was set to 8");
        is(blen, 4, "blen remains at 4");
        is_eq(txt, "0A141E28", "The txt data is correct");

        memset(bin, '\0', 4);
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is(kit_base16decode(bin, &blen, txt, &tlen, 0), NULL, "Got no error from kit_base16decode()");
        is(blen, 4, "blen was set to 4");
        is(tlen, 8, "tlen was set to 8");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");

        diag("Setting up with input containing spaces");
        strcpy(txt, "0A141E28 323c4650");
        memset(bin, '\0', 4);
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is(kit_base16decode(bin, &blen, txt, &tlen, 0), NULL, "Got no error from kit_base16decode()");
        is(blen, 4, "blen was set to 4");
        is(tlen, 8, "tlen was set to 8");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");

        memset(bin, '\0', 4);
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is(kit_base16decode(bin, &blen, txt, &tlen, KIT_BASE_DECODE_SKIP_WHITESPACE), NULL,
           "Got no error from kit_base16decode(..., KIT_BASE_DECODE_SKIP_WHITESPACE)");
        is(blen, 8, "blen was set to 8");
        is(tlen, 17, "tlen was set to 17");
        ok(memcmp(bin, "\12\24\36\50\62\74\106\120", 8) == 0, "The binary is correct");
    }

    diag("The base32encode/base32decode interface");
    {
        memset(txt, '\0', 8);
        blen = 4;
        tlen = sizeof(txt);
        is(kit_base32hexencode(txt, &tlen, (const uint8_t *)"\12\24\36\50", &blen), NULL, "Got no error from kit_base32hexencode()");
        is(tlen, 7, "tlen was set to 7");
        is(blen, 4, "blen remains at 4");
        is_eq(txt, "18A1SA0", "The base32 data is correct");

        memset(bin, '\0', 4);
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is(kit_base32hexdecode(bin, &blen, txt, &tlen, 0), NULL, "Got no error from kit_base32hexdecode()");
        is(blen, 4, "blen was set to 4");
        is(tlen, 7, "tlen was set to 7");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");

        diag("Same again, but with a small output buffer");
        blen = 3;
        tlen = sizeof(txt);
        is_eq(kit_base32hexdecode(bin, &blen, txt, &tlen, 0), "Output overflow", "Got an error from kit_base32hexdecode()");

        diag("Adding some padding to the end of the input");
        strcat(txt, "=");
        memset(bin, '\0', 4);
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is(kit_base32hexdecode(bin, &blen, txt, &tlen, 0), NULL, "Got no error from kit_base32hexdecode()");
        is(blen, 4, "blen was set to 4");
        is(tlen, 7, "tlen was set to 7 - despite the padding character (which was ignored)");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");
    }

    diag("The base64encode/base64decode interface");
    {
        memset(txt, '\0', 8);
        blen = 4;
        tlen = sizeof(txt);
        is(kit_base64encode(txt, &tlen, (const uint8_t *)"\12\24\36\50", &blen), NULL, "Got no error from kit_base64encode()");
        is(tlen, 8, "tlen was set to 8");
        is(blen, 4, "blen remains at 4");
        is_eq(txt, "ChQeKA==", "The base64 data is correct");

        memset(bin, '\0', 4);
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is(kit_base64decode(bin, &blen, txt, &tlen, 0), NULL, "Got no error from kit_base64decode()");
        is(blen, 4, "blen was set to 4");
        is(tlen, 8, "tlen was set to 8");
        ok(memcmp(bin, "\12\24\36\50", 4) == 0, "The binary is the same as the original");

        diag("Some more padding tests");
        for (i = 7; i > 5; i--) {
            txt[i] = '\0';
            blen = sizeof(bin);
            tlen = sizeof(txt);
            is_eq(kit_base64decode(bin, &blen, txt, &tlen, 0), "Padding characters missing", "Got an error from kit_base64decode()");
        }

        diag("Setting up some bogus base64 padding");
        strcpy(txt, "AwEAAYvgW===");
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is_eq(kit_base64decode(bin, &blen, txt, &tlen, 0), "Padding characters missing", "Got no error from kit_base64decode()");

        strcpy(txt, "AwEAAYvgWb=b");
        blen = sizeof(bin);
        tlen = sizeof(txt);
        is_eq(kit_base64decode(bin, &blen, txt, &tlen, 0), "Padding characters missing", "Got an error from kit_base64decode()");

        diag("Some more overflow tests");
        for (i = 8; i > 4; i--) {
            blen = 4;
            tlen = i;
            is_eq(kit_base64encode(txt, &tlen, (const uint8_t *)"\12\24\36\50", &blen), "Output overflow", "Got an error from kit_base64encode()");
        }
    }

    return exit_status();
}
