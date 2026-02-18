#ifndef FS_H
#define FS_H

#include "types.h"
#include "paging.h"

// Filesystem constants
#define FS_MAGIC            0x4D4F5652  // "MOVR" in little endian
#define FS_VERSION          2           // Version 2 with directories
#define FS_MAX_FILES        256         // Increased file limit
#define FS_MAX_FILENAME     27          // 27 chars + null (for parent_dir field)
#define FS_MAX_PATH         128         // Max path length

// Block management
#define FS_BLOCK_SIZE       4096        // 4KB blocks
#define FS_HEADER_SIZE      4096        // 4KB header (includes bitmap)
#define FS_FILE_TABLE_SIZE  16384       // 16KB file table (256 * 64 bytes)
#define FS_DATA_OFFSET      20480       // Data starts at 20KB
#define FS_MAX_BLOCKS       256         // For 1MB data = 256 blocks
#define FS_BITMAP_SIZE      32          // 256 bits = 32 bytes
#define FS_BLOCK_END        0xFFFFFFFF  // End of block chain marker

// File types/flags
#define FILE_TYPE_UNUSED    0x00
#define FILE_TYPE_FILE      0x01
#define FILE_TYPE_DIR       0x02
#define FILE_FLAG_READONLY  0x10

// Special directory indices
#define ROOT_DIR_INDEX      0           // Root directory is always entry 0
#define INVALID_INDEX       0xFFFFFFFF

// Filesystem header (fits in first 4KB)
struct FSHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t file_count;
    uint32_t block_count;       // Total blocks in data region
    uint32_t free_blocks;       // Free block count
    uint8_t  block_bitmap[FS_BITMAP_SIZE];  // 256 bits for block allocation
    uint32_t reserved[48];      // Padding to align
} __attribute__((packed));

// File entry (64 bytes each, 256 entries = 16KB)
struct FileEntry {
    char name[28];              // Filename (null-terminated)
    uint32_t parent_dir;        // Parent directory index (INVALID_INDEX for root)
    uint32_t first_block;       // First data block (INVALID_INDEX if empty)
    uint32_t size;              // File size in bytes
    uint8_t  type;              // FILE_TYPE_FILE, FILE_TYPE_DIR, etc.
    uint8_t  flags;             // Additional flags
    uint16_t reserved;          // Alignment padding
    uint32_t created;           // Creation time
    uint32_t modified;          // Modified time
    uint32_t block_count;       // Number of blocks used
} __attribute__((packed));      // Total: 64 bytes

// Block header (each data block starts with this)
struct BlockHeader {
    uint32_t next_block;        // Next block in chain (FS_BLOCK_END if last)
    uint32_t data_size;         // Bytes of data in this block
} __attribute__((packed));

#define FS_BLOCK_DATA_SIZE  (FS_BLOCK_SIZE - sizeof(BlockHeader))  // ~4088 bytes

// Persistent filesystem class
class PersistentFS {
private:
    void* base_addr;
    FSHeader* header;
    FileEntry* file_table;
    uint8_t* data_region;
    uint32_t current_dir;       // Current working directory index
    
    // Block management
    int32_t alloc_block();
    void free_block(uint32_t block);
    bool is_block_free(uint32_t block);
    void set_block_used(uint32_t block);
    void set_block_free(uint32_t block);
    uint8_t* get_block_ptr(uint32_t block);
    
    // File lookup
    FileEntry* find_entry(const char* name, uint32_t parent);
    FileEntry* find_free_entry();
    uint32_t find_entry_index(const char* name, uint32_t parent);
    
    // Path parsing
    bool parse_path(const char* path, char* name, uint32_t* parent_dir);
    
public:
    PersistentFS();
    
    // Initialize/format
    void initialize();
    bool is_formatted();
    void format();
    
    // File operations
    bool create(const char* path, const char* content, uint32_t size);
    bool remove(const char* path);
    int32_t read(const char* path, char* buffer, uint32_t max_size);
    bool write(const char* path, const char* content, uint32_t size);
    
    // Directory operations
    bool mkdir(const char* path);
    bool chdir(const char* path);
    const char* getcwd();
    uint32_t get_cwd_index() { return current_dir; }
    
    // File info
    bool exists(const char* path);
    uint32_t get_size(const char* path);
    bool is_directory(const char* path);
    
    // Listing
    uint32_t get_file_count();
    uint32_t get_dir_file_count(uint32_t dir_index);
    bool get_file_info(uint32_t dir_index, uint32_t index, char* name, uint32_t* size, uint8_t* type);
    
    // Stats
    uint32_t get_used_space();
    uint32_t get_free_space();
    uint32_t get_free_blocks() { return header->free_blocks; }
};

// Global filesystem instance
extern PersistentFS fs;

// String helper functions
void str_copy(char* dest, const char* src, uint32_t max_len);
int str_compare(const char* s1, const char* s2);
uint32_t str_length(const char* s);

#endif // FS_H
