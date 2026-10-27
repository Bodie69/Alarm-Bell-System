#include "crypto_utils.h"
#include "mbedtls/md.h"

String sha256Hex(const String& input) {
    unsigned char hash[32];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, (const unsigned char*)input.c_str(), input.length());
    mbedtls_md_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hex + (i * 2), "%02x", hash[i]);
    }
    hex[64] = '\0';
    return String(hex);
}
