#include "framebuffer_hash.h"

#include <algorithm>
#include <cstdio>
#include <vector>
#include <openssl/evp.h>

std::string ppm_md5(const uint8_t* rgba, int width, int height) {
    char header[64];
    int header_size = std::snprintf(
        header, sizeof(header), "P6\n%d %d\n255\n", width, height);
    size_t pixel_size = static_cast<size_t>(width) * height * 3;
    std::vector<uint8_t> ppm(static_cast<size_t>(header_size) + pixel_size);
    std::copy(header, header + header_size, ppm.begin());
    for (int i = 0; i < width * height; ++i) {
        ppm[header_size + i * 3] = rgba[i * 4];
        ppm[header_size + i * 3 + 1] = rgba[i * 4 + 1];
        ppm[header_size + i * 3 + 2] = rgba[i * 4 + 2];
    }

    unsigned char digest[EVP_MAX_MD_SIZE] = {};
    unsigned int digest_size = 0;
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) return {};
    bool ok = EVP_DigestInit_ex(context, EVP_md5(), nullptr) == 1
        && EVP_DigestUpdate(context, ppm.data(), ppm.size()) == 1
        && EVP_DigestFinal_ex(context, digest, &digest_size) == 1
        && digest_size == 16;
    EVP_MD_CTX_free(context);
    if (!ok) return {};

    char hex[33];
    for (size_t i = 0; i < 16; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    return std::string(hex, 32);
}
