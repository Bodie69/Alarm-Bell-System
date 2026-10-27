#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <Arduino.h>

// Returns the lowercase hex-encoded SHA-256 digest of the input string.
// Used to store/verify the admin password without ever keeping it in
// plaintext on flash.
String sha256Hex(const String& input);

#endif // CRYPTO_UTILS_H
