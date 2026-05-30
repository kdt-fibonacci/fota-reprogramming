#ifndef CRYPTO_H
#define CRYPTO_H

void calculate_sha256(const char* path, char output[65]);

int generate_sig_file(
    const char* hex_path,
    const char* sig_path
);


#endif