/**
 * @file    wallet.h
 * @author  Cypherock X1 Team
 * @brief   Title of the file.
 *          Short description of the file
 * @copyright Copyright (c) 2022 HODL TECH PTE LTD
 * <br/> You may obtain a copy of license at <a href="https://mitcc.org/"
 * target=_blank>https://mitcc.org/</a>
 *
 */
#ifndef WALLET_H
#define WALLET_H

#include <inttypes.h>
#include <stdbool.h>

#include "memzero.h"
#include "sha2.h"

#define FAMILY_ID_SIZE 4
#define CARD_ID_SIZE (FAMILY_ID_SIZE + 1)

#define CARD_VERSION_MAJOR_MINOR_SIZE 1
#define CARD_VERSION_PATCH_SIZE 1
#define CARD_VERSION_GIT_REV_SIZE 4
#define CARD_VERSION_SIZE                                                      \
  (CARD_VERSION_MAJOR_MINOR_SIZE + CARD_VERSION_PATCH_SIZE +                   \
   CARD_VERSION_GIT_REV_SIZE)

#define BLOCK_SIZE 32
#define NONCE_SIZE 16
#define WALLET_MAC_SIZE 16
#define PIN_SHARE_SIZE 80
#define CHECKSUM_SIZE 4
#define NAME_SIZE 16         ///< Size of name of wallet
#define WALLET_ID_SIZE 32    ///< Size of wallet id (generated by hashing seed)
#define MAX_WALLETS_ALLOWED 4    ///< Maximum number of wallets allowed
#define KEY_SIZE 32              // chacha polly key
#define BENEFICIARY_KEY_SIZE 16
#define IV_FOR_BENEFICIARY_KEY_SIZE 16
#define MINIMUM_NO_OF_SHARES 2

/// Total number of shares. Used when declaring arrays.
#define TOTAL_NUMBER_OF_SHARES 5

/// Max size of array when storing arbirtary data.
#define MAX_ARBITRARY_DATA_SIZE 512

/// Max number of mnemonic words
#define MAX_NUMBER_OF_MNEMONIC_WORDS 24

/// Max length of mnemonic word
#define MAX_MNEMONIC_WORD_LENGTH 16

/// Max length of passphrase entered
#define MAX_PASSPHRASE_INPUT_LENGTH 65

/// Max length of coin name (set as some MAX_MNEMONIC_WORD_LENGTH because they
/// both use ui_list)
#define MAX_COIN_NAME_LENGTH MAX_MNEMONIC_WORD_LENGTH

/// Returns 0 if PIN is not set else 1
#define WALLET_IS_PIN_SET(n) (n & 1)
/// Sets the bit that corresponds to pin
#define WALLET_SET_PIN(n) (n |= 1)
/// Unsets the bit that corresponds to pin
#define WALLET_UNSET_PIN(n) (n &= (~1))
/// Returns 0 if Passphrase is not set else 1
#define WALLET_IS_PASSPHRASE_SET(n) ((n >> 1) & 1)
/// Sets the bit that corresponds to passphrase
#define WALLET_SET_PASSPHRASE(n) (n |= 2)
/// Unsets the bit that corresponds to passphrase
#define WALLET_UNSET_PASSPHRASE(n) (n &= (~2))
/// Returns 0 if Arbitrary Data is not set else 1
#define WALLET_IS_ARBITRARY_DATA(n) ((n >> 2) & 1)
/// Sets the bit that corresponds to Arbitrary Data
#define WALLET_SET_ARBITRARY_DATA(n) (n |= 4)
/// Unsets the bit that corresponds to Arbitrary Data
#define WALLET_UNSET_ARBITRARY_DATA(n) (n &= (~4))

typedef enum wallet_name_opt {
  WALLET_NAME_ONE = 1,
  WALLET_NAME_TWO,
  WALLET_NAME_THREE,
} wallet_name_opt_t;

/**
 * @brief Error codes for wallet data receive from cards
 *
 * @since v1.0.0
 */
typedef enum Card_Data_Errors {
  VALID_DATA = 0,
  INVALID_NAME_LENGTH,
  INVALID_WALLET_CONFIG,
  INVALID_MNEMONIC_LENGTH,
  INVALID_SHAMIR_CONFIG,
  INVALID_SHARE_INDEX,
  INVALID_WALLET_ID,
  INVALID_CHECKSUM,
} Card_Data_errors_t;

/**
 * @brief Wallet struct for X1 Card
 * @details
 *
 * @see
 * @since v1.0.0
 *
 * @note
 */
typedef struct Wallet {
  uint8_t wallet_name[NAME_SIZE];

  uint8_t wallet_info;
  uint8_t password_double_hash[BLOCK_SIZE];

  uint8_t wallet_share_with_mac_and_nonce[BLOCK_SIZE + NONCE_SIZE +
                                          WALLET_MAC_SIZE];
  uint8_t arbitrary_data_share[512];

  uint8_t number_of_mnemonics;
  uint8_t minimum_number_of_shares;
  uint8_t total_number_of_shares;
  uint8_t arbitrary_data_size;

  uint8_t xcor;
  ///< 30-bit checksum of data. The last 2-bits of checksum (`01`) define if the
  ///< checksum holds a meaningful value.
  uint8_t checksum[CHECKSUM_SIZE];

  uint8_t key[KEY_SIZE];    // This key is currently used for
                            // encrypting/decrypting extended public key

  uint8_t beneficiary_key[BENEFICIARY_KEY_SIZE];
  uint8_t iv_for_beneficiary_key[IV_FOR_BENEFICIARY_KEY_SIZE];

  uint8_t wallet_id[WALLET_ID_SIZE];    // Hash of master public key
} Wallet;

/**
 * @brief Global struct to store shamir data during flows
 * @details
 *
 * @see
 * @since v1.0.0
 *
 * @note
 */
#pragma pack(push, 1)
typedef struct Wallet_shamir_data {
  union {
    uint8_t mnemonic_shares[TOTAL_NUMBER_OF_SHARES][BLOCK_SIZE];
    uint8_t arbitrary_data_shares[TOTAL_NUMBER_OF_SHARES]
                                 [MAX_ARBITRARY_DATA_SIZE];
  };
  uint8_t share_x_coords[TOTAL_NUMBER_OF_SHARES];
  uint8_t share_encryption_data[TOTAL_NUMBER_OF_SHARES]
                               [NONCE_SIZE + WALLET_MAC_SIZE];
} Wallet_shamir_data;
#pragma pack(pop)

/**
 * @brief Global struct to store confidential data such as mnemonics, passphrase
 * and password hashes during flows
 * @details
 *
 * @see
 * @since v1.0.0
 *
 * @note
 */
#pragma pack(push, 1)
typedef struct Wallet_credential_data {
  char mnemonics[MAX_NUMBER_OF_MNEMONIC_WORDS][MAX_MNEMONIC_WORD_LENGTH];
  char passphrase[MAX_PASSPHRASE_INPUT_LENGTH];
  uint8_t password_single_hash[SHA256_DIGEST_LENGTH];
} Wallet_credential_data;
#pragma pack(pop)

extern Wallet_credential_data wallet_credential_data;
extern Wallet_shamir_data wallet_shamir_data;
extern Wallet wallet;

/**
 * @brief Encrypts hash of share using chachapoly
 * @details
 *
 * @param []
 *
 * @return Success status
 * @retval true Success
 * @retval false Failure
 *
 * @see
 * @since v1.0.0
 *
 * @note
 */
bool encrypt_shares();

/**
 * @brief Decrypt hash of share using chachapoly
 * @details
 *
 * @param []
 *
 * @return Success status
 * @retval true Success
 * @retval false Failure
 *
 * @see
 * @since v1.0.0
 *
 * @note
 */
bool decrypt_shares();

/**
 * @brief Calculate the checksum for wallet's data stored and retrieved from
 * card The checksum is first 30-bits of SHA256 on the packed serialized of the
 * data received from the card. The last 2-bits of the arrays are set to `01`
 * for ensuring the distinction if the checksum value exists or not.
 *
 * The members are serialized in the following order:
 * wallet_name | xcor | number_of_mnemonics | total_number_of_shares |
 * wallet_share_with_mac_and_nonce | minimum_number_of_shares |
 * wallet_info | key | wallet_id | arbitrary_data_share
 *
 * @param [in]      Pointer to a Wallet instance
 * @param [out]     Calculated checksum of the wallet instance
 *
 * @see Wallet
 * @since v1.0.0
 *
 * @note The last 2-bits of checksum (`01`) define if the checksum holds a
 * meaningful value.
 */
void calculate_checksum(const Wallet *wallet, uint8_t *checksum);

/**
 * @brief Verifies if the checksum of the given Wallet instance matches with the
 * actual checksum calculation based on the data in the instance.
 * If the provided wallet instance is NULL, this function returns false.
 * if the wallet version bit in Wallet.wallet_info is not set, returns true.
 *
 * @param [in]      Pointer to a Wallet instance
 *
 * @return Checksum verification status
 * @retval true If the calculated checksum matches the checksum in the instance
 * @retval false Otherwise
 *
 * @see calculcate_checksum(), Wallet
 * @since v1.0.0
 */
bool verify_checksum(const Wallet *wallet);

#endif