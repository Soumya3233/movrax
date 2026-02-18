#ifndef INTEGRITY_H
#define INTEGRITY_H

#include "types.h"

// ============================================================================
// MOVRAX Integrity Subsystem
// CRC32 checksums for data integrity verification
// XOR-based lightweight encryption for persistent storage
// ============================================================================

// CRC32 (ISO 3309 / ITU-T V.42 polynomial)
class CRC32 {
public:
  // Compute CRC32 for a block of data
  static uint32_t compute(const void *data, uint32_t length);

  // Incremental CRC32 (start with init(), then update(), then finalize())
  static uint32_t init();
  static uint32_t update(uint32_t crc, const void *data, uint32_t length);
  static uint32_t finalize(uint32_t crc);

  // Verify data against expected CRC
  static bool verify(const void *data, uint32_t length, uint32_t expected_crc);
};

// Lightweight XOR stream cipher for persistent storage
// NOTE: This is NOT cryptographically secure — it's obfuscation.
// For real military use, AES-256 would be needed (hardware AES-NI or software).
// This provides basic protection against casual memory dumps.
class XORCipher {
private:
  static uint32_t key[4]; // 128-bit key
  static bool initialized;

  // Generate keystream byte at position
  static uint8_t keystream_byte(uint32_t position);

public:
  // Initialize with a 128-bit key
  static void initialize(uint32_t k0, uint32_t k1, uint32_t k2, uint32_t k3);

  // Encrypt/decrypt in-place (XOR is symmetric)
  static void process(void *data, uint32_t length, uint32_t offset = 0);

  // Encrypt a copy
  static void encrypt(const void *src, void *dst, uint32_t length,
                      uint32_t offset = 0);

  // Check if initialized
  static bool is_ready() { return initialized; }
};

// Filesystem integrity checker
class FSIntegrity {
public:
  // Compute CRC32 for a file entry + its data blocks
  static uint32_t compute_file_crc(uint32_t file_index);

  // Verify all files in the filesystem
  static uint32_t verify_all(); // Returns number of corrupted entries

  // Update stored CRC for a file (call after write)
  static void update_file_crc(uint32_t file_index);

  // Verify filesystem header integrity
  static bool verify_header();

  // Full filesystem integrity report (to terminal)
  static void report();
};

#endif // INTEGRITY_H
