#include "integrity.h"
#include "audit.h"
#include "fs.h"
#include "vga.h"

// ============================================================================
// CRC32 Implementation (ISO 3309 / ITU-T V.42)
// Polynomial: 0xEDB88320 (reversed representation)
// Used in Ethernet, gzip, PNG
// ============================================================================

// CRC32 lookup table — generated at compile time from polynomial
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91B, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBE, 0xE7B82D09, 0x90BF1D9F, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C96C, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBB9D6, 0xACBCE9C0, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F6B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8ABEE5E1, 0xFDBE9BB7, 0x70B7CB14, 0x07B076A2, 0x9EB9A718, 0xE9BE97D6,
    0x7BB12BAE, 0x0CB61B38, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x95BF4A82, 0xE2B87A14, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x95BF4A82, 0xE2B87A14, 0x2D02EF8D, 0x5A05DF1B, 0x2D02EF8D, 0x5A05DF1B,
    0x2D02EF8D, 0x5A05DF1B, 0x2D02EF8D, 0x5A05DF1B, 0x00000000, 0x77073096,
    0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91B, 0x97D2D988, 0x09B64C2B, 0x7EB17CBE,
    0xE7B82D09, 0x90BF1D9F, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0,
    0xFD62F97A, 0x8A65C96C, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447,
    0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C, 0xDBBBB9D6, 0xACBCE9C0,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A,
    0xC8D75180, 0xBFD06116, 0x21B4F6B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11,
    0xC1611DAB, 0xB6662D3D};

uint32_t CRC32::init() { return 0xFFFFFFFF; }

uint32_t CRC32::update(uint32_t crc, const void *data, uint32_t length) {
  const uint8_t *bytes = (const uint8_t *)data;
  for (uint32_t i = 0; i < length; i++) {
    crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
  }
  return crc;
}

uint32_t CRC32::finalize(uint32_t crc) { return crc ^ 0xFFFFFFFF; }

uint32_t CRC32::compute(const void *data, uint32_t length) {
  return finalize(update(init(), data, length));
}

bool CRC32::verify(const void *data, uint32_t length, uint32_t expected_crc) {
  return compute(data, length) == expected_crc;
}

// ============================================================================
// XOR Stream Cipher
// Position-dependent keystream for basic storage obfuscation
// ============================================================================

uint32_t XORCipher::key[4] = {0, 0, 0, 0};
bool XORCipher::initialized = false;

void XORCipher::initialize(uint32_t k0, uint32_t k1, uint32_t k2, uint32_t k3) {
  key[0] = k0;
  key[1] = k1;
  key[2] = k2;
  key[3] = k3;
  initialized = true;
}

uint8_t XORCipher::keystream_byte(uint32_t position) {
  // Simple position-dependent keystream using key mixing
  uint32_t word_idx = (position / 4) % 4;
  uint32_t byte_idx = position % 4;
  uint32_t mixed =
      key[word_idx] ^ (position * 0x9E3779B9); // Golden ratio constant
  mixed ^= (mixed >> 16);
  mixed *= 0x45D9F3B;
  mixed ^= (mixed >> 16);
  return (uint8_t)(mixed >> (byte_idx * 8));
}

void XORCipher::process(void *data, uint32_t length, uint32_t offset) {
  if (!initialized)
    return;
  uint8_t *bytes = (uint8_t *)data;
  for (uint32_t i = 0; i < length; i++) {
    bytes[i] ^= keystream_byte(offset + i);
  }
}

void XORCipher::encrypt(const void *src, void *dst, uint32_t length,
                        uint32_t offset) {
  if (!initialized)
    return;
  const uint8_t *in = (const uint8_t *)src;
  uint8_t *out = (uint8_t *)dst;
  for (uint32_t i = 0; i < length; i++) {
    out[i] = in[i] ^ keystream_byte(offset + i);
  }
}

// ============================================================================
// Filesystem Integrity Checker
// Uses the FileEntry.reserved field (uint16_t at offset 52) for CRC storage
// We store lower 16 bits of CRC32 in the reserved field
// ============================================================================

// Access filesystem internals
extern PersistentFS fs;

uint32_t FSIntegrity::compute_file_crc(uint32_t file_index) {
  // Get file table pointer from persistent region
  uint8_t *base = (uint8_t *)PERSISTENT_REGION_VADDR;
  FileEntry *file_table = (FileEntry *)(base + FS_HEADER_SIZE);
  uint8_t *data_region = base + FS_DATA_OFFSET;

  FileEntry *entry = &file_table[file_index];
  if (entry->type == FILE_TYPE_UNUSED)
    return 0;

  uint32_t crc = CRC32::init();

  // CRC over filename and metadata (exclude reserved and CRC fields)
  crc = CRC32::update(crc, entry->name, 28);
  crc = CRC32::update(crc, &entry->size, 4);
  crc = CRC32::update(crc, &entry->type, 1);

  // CRC over data blocks
  uint32_t block = entry->first_block;
  while (block != INVALID_INDEX && block != FS_BLOCK_END &&
         block < FS_MAX_BLOCKS) {
    uint8_t *block_ptr = data_region + (block * FS_BLOCK_SIZE);
    BlockHeader *hdr = (BlockHeader *)block_ptr;
    uint8_t *data = block_ptr + sizeof(BlockHeader);

    crc = CRC32::update(crc, data, hdr->data_size);
    block = hdr->next_block;
  }

  return CRC32::finalize(crc);
}

void FSIntegrity::update_file_crc(uint32_t file_index) {
  uint8_t *base = (uint8_t *)PERSISTENT_REGION_VADDR;
  FileEntry *file_table = (FileEntry *)(base + FS_HEADER_SIZE);

  uint32_t crc = compute_file_crc(file_index);
  file_table[file_index].reserved = (uint16_t)(crc & 0xFFFF);
}

uint32_t FSIntegrity::verify_all() {
  uint8_t *base = (uint8_t *)PERSISTENT_REGION_VADDR;
  FileEntry *file_table = (FileEntry *)(base + FS_HEADER_SIZE);

  uint32_t corrupted = 0;

  for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
    if (file_table[i].type == FILE_TYPE_UNUSED)
      continue;
    if (file_table[i].reserved == 0)
      continue; // No CRC stored yet

    uint32_t computed = compute_file_crc(i);
    uint16_t stored = file_table[i].reserved;

    if ((uint16_t)(computed & 0xFFFF) != stored) {
      corrupted++;
      AuditLog::log(AUDIT_SECURITY_ALERT, 0, stored, computed, i);
    }
  }

  return corrupted;
}

bool FSIntegrity::verify_header() {
  uint8_t *base = (uint8_t *)PERSISTENT_REGION_VADDR;
  FSHeader *header = (FSHeader *)base;

  return (header->magic == FS_MAGIC && header->version == FS_VERSION);
}

// Helper to print hex
static void print_hex_integ(uint32_t val) {
  const char hex[] = "0123456789ABCDEF";
  char buf[11] = "0x00000000";
  for (int i = 9; i >= 2; i--) {
    buf[i] = hex[val & 0xF];
    val >>= 4;
  }
  terminal.write_string(buf);
}

static void print_num_integ(uint32_t num) {
  if (num == 0) {
    terminal.write_string("0");
    return;
  }
  char buf[12];
  int i = 0;
  while (num > 0) {
    buf[i++] = '0' + (num % 10);
    num /= 10;
  }
  while (i > 0) {
    char c[2] = {buf[--i], '\0'};
    terminal.write_string(c);
  }
}

void FSIntegrity::report() {
  terminal.set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
  terminal.write_string("=== Filesystem Integrity Report ===\n");

  // Header check
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("  Header:     ");
  if (verify_header()) {
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string("OK\n");
  } else {
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    terminal.write_string("CORRUPTED\n");
  }

  // File integrity
  uint8_t *base = (uint8_t *)PERSISTENT_REGION_VADDR;
  FileEntry *file_table = (FileEntry *)(base + FS_HEADER_SIZE);

  uint32_t total = 0, checked = 0, passed = 0, failed = 0, no_crc = 0;

  for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
    if (file_table[i].type == FILE_TYPE_UNUSED)
      continue;
    total++;

    if (file_table[i].reserved == 0) {
      no_crc++;
      continue;
    }

    checked++;
    uint32_t computed = compute_file_crc(i);
    if ((uint16_t)(computed & 0xFFFF) == file_table[i].reserved) {
      passed++;
    } else {
      failed++;
      terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
      terminal.write_string("  FAIL: ");
      terminal.write_string(file_table[i].name);
      terminal.write_string(" expected=");
      print_hex_integ(file_table[i].reserved);
      terminal.write_string(" got=");
      print_hex_integ(computed & 0xFFFF);
      terminal.write_string("\n");
    }
  }

  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("  Files:      ");
  print_num_integ(total);
  terminal.write_string("\n  Checked:    ");
  print_num_integ(checked);
  terminal.write_string("\n  Passed:     ");
  terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
  print_num_integ(passed);
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("\n  Failed:     ");
  if (failed > 0)
    terminal.set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
  print_num_integ(failed);
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal.write_string("\n  No CRC:     ");
  print_num_integ(no_crc);
  terminal.write_string("\n");

  // Encryption status
  terminal.write_string("  Encryption: ");
  if (XORCipher::is_ready()) {
    terminal.set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal.write_string("ACTIVE\n");
  } else {
    terminal.set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal.write_string("INACTIVE\n");
  }
  terminal.set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
