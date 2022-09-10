#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "mbedtls/aes.h"
#include "core/cryption_mngr.h"

#define TEST_INPUT_LENGTH 256

#define CHECK_AES_LEN(len)                                             \
    if (len < 16) len = 16;                                            \
    if ((len % 16)) {                                                  \
        ESP_LOGE(TAG, "For CBC it has to be a multiple of 16 bytes."); \
        return CRYPTION_FAIL;                                          \
    }                                                                  \

static const char *TAG = "cryption_mngr";

typedef struct {
    uint8_t *enc_key;
    unsigned int keybits;
    unsigned char *iv_in;
    unsigned char *iv_out;
    mbedtls_aes_context *aes;
} s_cryption_if_t;

static s_cryption_if_t s_cryption_if = {.enc_key = NULL,
                                        .keybits = 256,
                                        .iv_in = NULL,
                                        .iv_out = NULL,
                                       };
static mbedtls_aes_context s_aes;

esp_err_t cryption_mngr_encrypt(char *input, size_t len, char *output)
{
    CHECK_AES_LEN(len)
    return mbedtls_aes_crypt_cbc(s_cryption_if.aes, MBEDTLS_AES_ENCRYPT, len, s_cryption_if.iv_in, (const unsigned char *)input, (unsigned char *)output) ? ESP_FAIL : ESP_OK;
}

esp_err_t cryption_mngr_decrypt(char *input, size_t len, char *output)
{
    CHECK_AES_LEN(len)
    return mbedtls_aes_crypt_cbc(s_cryption_if.aes, MBEDTLS_AES_DECRYPT, len, s_cryption_if.iv_out, (const unsigned char *)input, (unsigned char *)output) ? ESP_FAIL : ESP_OK;
}

static esp_err_t cryption_mngr_set_key(s_cryption_if_t *s_cryption_ifp)
{
    return mbedtls_aes_setkey_enc(s_cryption_ifp->aes, s_cryption_ifp->enc_key, s_cryption_ifp->keybits) ? ESP_FAIL : ESP_OK;
}

esp_err_t cryption_mngr_init(char *key)
{
    mbedtls_aes_init(&s_aes);
    s_cryption_if.aes = &s_aes;
    s_cryption_if.enc_key = (uint8_t *)strdup(key);
    s_cryption_if.iv_in = (unsigned char *)strdup(key);
    s_cryption_if.iv_out = (unsigned char *)strdup(key);

    esp_err_t sta = ESP_OK;
    sta |= cryption_mngr_set_key(&s_cryption_if);
    ESP_LOGI(TAG, "enc_key:\t%s", s_cryption_if.enc_key);
    ESP_LOGI(TAG, "enc_key iv_in:\t%s", s_cryption_if.iv_in);
    ESP_LOGI(TAG, "enc_key iv_out:\t%s", s_cryption_if.iv_out);
    ESP_LOGI(TAG, "enc_key bits:\t%d", s_cryption_if.keybits);

    /* Testing*/
    char enc_input[TEST_INPUT_LENGTH] = {"EncryptionString"};

    char *encrypt_output = (char *)malloc(TEST_INPUT_LENGTH);
    if (cryption_mngr_encrypt(enc_input, TEST_INPUT_LENGTH, encrypt_output)) {
        ESP_LOGI(TAG, "cryption_mngr_encrypt success.");
    }

    char *decrypt_output = (char *)malloc(TEST_INPUT_LENGTH);
    if (cryption_mngr_decrypt(encrypt_output, TEST_INPUT_LENGTH, decrypt_output)) {
        ESP_LOGI(TAG, "cryption_mngr_decrypt success.");
    }
    ESP_LOGI(TAG, "decrypt output string:%s", decrypt_output);
    /* End test */
    return sta;
}
