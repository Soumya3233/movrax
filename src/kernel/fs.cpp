#include "fs.h"
#include "vga.h"

// Global filesystem instance
PersistentFS fs;

// String helper implementations
void str_copy(char* dest, const char* src, uint32_t max_len) {
    uint32_t i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int str_compare(const char* s1, const char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

uint32_t str_length(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

// Constructor
PersistentFS::PersistentFS() 
    : base_addr(nullptr), header(nullptr), file_table(nullptr), 
      data_region(nullptr), current_dir(ROOT_DIR_INDEX) {
}

// Block management
bool PersistentFS::is_block_free(uint32_t block) {
    if (block >= FS_MAX_BLOCKS) return false;
    uint32_t byte_idx = block / 8;
    uint32_t bit_idx = block % 8;
    return !(header->block_bitmap[byte_idx] & (1 << bit_idx));
}

void PersistentFS::set_block_used(uint32_t block) {
    if (block >= FS_MAX_BLOCKS) return;
    uint32_t byte_idx = block / 8;
    uint32_t bit_idx = block % 8;
    header->block_bitmap[byte_idx] |= (1 << bit_idx);
}

void PersistentFS::set_block_free(uint32_t block) {
    if (block >= FS_MAX_BLOCKS) return;
    uint32_t byte_idx = block / 8;
    uint32_t bit_idx = block % 8;
    header->block_bitmap[byte_idx] &= ~(1 << bit_idx);
}

int32_t PersistentFS::alloc_block() {
    for (uint32_t i = 0; i < FS_MAX_BLOCKS; i++) {
        if (is_block_free(i)) {
            set_block_used(i);
            header->free_blocks--;
            
            // Clear block
            uint8_t* block = get_block_ptr(i);
            for (uint32_t j = 0; j < FS_BLOCK_SIZE; j++) {
                block[j] = 0;
            }
            
            return i;
        }
    }
    return -1;  // No free blocks
}

void PersistentFS::free_block(uint32_t block) {
    if (block < FS_MAX_BLOCKS && !is_block_free(block)) {
        set_block_free(block);
        header->free_blocks++;
    }
}

uint8_t* PersistentFS::get_block_ptr(uint32_t block) {
    return data_region + (block * FS_BLOCK_SIZE);
}

// File lookup
FileEntry* PersistentFS::find_entry(const char* name, uint32_t parent) {
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].type != FILE_TYPE_UNUSED &&
            file_table[i].parent_dir == parent &&
            str_compare(file_table[i].name, name) == 0) {
            return &file_table[i];
        }
    }
    return nullptr;
}

uint32_t PersistentFS::find_entry_index(const char* name, uint32_t parent) {
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].type != FILE_TYPE_UNUSED &&
            file_table[i].parent_dir == parent &&
            str_compare(file_table[i].name, name) == 0) {
            return i;
        }
    }
    return INVALID_INDEX;
}

FileEntry* PersistentFS::find_free_entry() {
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].type == FILE_TYPE_UNUSED) {
            return &file_table[i];
        }
    }
    return nullptr;
}

// Path parsing - returns name and parent directory
bool PersistentFS::parse_path(const char* path, char* name, uint32_t* parent_dir) {
    if (!path || !path[0]) return false;
    
    uint32_t dir = current_dir;
    
    // Handle absolute path
    if (path[0] == '/') {
        dir = ROOT_DIR_INDEX;
        path++;
        if (!path[0]) {
            // Root directory itself
            name[0] = '/';
            name[1] = '\0';
            *parent_dir = INVALID_INDEX;
            return true;
        }
    }
    
    // Parse path components
    char component[FS_MAX_FILENAME + 1];
    uint32_t comp_len = 0;
    const char* last_component = path;
    
    while (*path) {
        if (*path == '/') {
            if (comp_len > 0) {
                component[comp_len] = '\0';
                
                // Look up this directory
                uint32_t next = find_entry_index(component, dir);
                if (next == INVALID_INDEX) return false;
                if (file_table[next].type != FILE_TYPE_DIR) return false;
                
                dir = next;
                comp_len = 0;
            }
            last_component = path + 1;
        } else {
            if (comp_len < FS_MAX_FILENAME) {
                component[comp_len++] = *path;
            }
        }
        path++;
    }
    
    // Copy final component as the name
    str_copy(name, last_component, FS_MAX_FILENAME + 1);
    *parent_dir = dir;
    return true;
}

// Initialize
void PersistentFS::initialize() {
    base_addr = (void*)PERSISTENT_REGION_VADDR;
    header = (FSHeader*)base_addr;
    file_table = (FileEntry*)((uint8_t*)base_addr + FS_HEADER_SIZE);
    data_region = (uint8_t*)base_addr + FS_DATA_OFFSET;
    current_dir = ROOT_DIR_INDEX;
    
    if (!is_formatted()) {
        format();
    }
}

bool PersistentFS::is_formatted() {
    return header->magic == FS_MAGIC && header->version == FS_VERSION;
}

void PersistentFS::format() {
    // Clear header
    header->magic = FS_MAGIC;
    header->version = FS_VERSION;
    header->file_count = 1;  // Root directory
    header->block_count = FS_MAX_BLOCKS;
    header->free_blocks = FS_MAX_BLOCKS;
    
    // Clear block bitmap
    for (uint32_t i = 0; i < FS_BITMAP_SIZE; i++) {
        header->block_bitmap[i] = 0;
    }
    
    // Clear file table
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        file_table[i].type = FILE_TYPE_UNUSED;
        file_table[i].name[0] = '\0';
    }
    
    // Create root directory
    str_copy(file_table[0].name, "/", 2);
    file_table[0].parent_dir = INVALID_INDEX;
    file_table[0].first_block = INVALID_INDEX;
    file_table[0].size = 0;
    file_table[0].type = FILE_TYPE_DIR;
    file_table[0].flags = 0;
    file_table[0].created = 0;
    file_table[0].modified = 0;
    file_table[0].block_count = 0;
    
    current_dir = ROOT_DIR_INDEX;
}

// Create file
bool PersistentFS::create(const char* path, const char* content, uint32_t size) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    
    if (!parse_path(path, name, &parent)) return false;
    if (find_entry(name, parent)) return false;  // Already exists
    
    FileEntry* entry = find_free_entry();
    if (!entry) return false;
    
    // Allocate blocks for content
    uint32_t first_block = INVALID_INDEX;
    uint32_t prev_block = INVALID_INDEX;
    uint32_t blocks_needed = (size + FS_BLOCK_DATA_SIZE - 1) / FS_BLOCK_DATA_SIZE;
    if (size == 0) blocks_needed = 0;
    
    uint32_t remaining = size;
    const char* data_ptr = content;
    
    for (uint32_t i = 0; i < blocks_needed; i++) {
        int32_t block = alloc_block();
        if (block < 0) {
            // Allocation failed - free allocated blocks
            uint32_t b = first_block;
            while (b != INVALID_INDEX) {
                BlockHeader* bh = (BlockHeader*)get_block_ptr(b);
                uint32_t next = bh->next_block;
                free_block(b);
                b = next;
            }
            return false;
        }
        
        if (first_block == INVALID_INDEX) {
            first_block = block;
        }
        if (prev_block != INVALID_INDEX) {
            BlockHeader* prev_hdr = (BlockHeader*)get_block_ptr(prev_block);
            prev_hdr->next_block = block;
        }
        
        // Write data to block
        BlockHeader* hdr = (BlockHeader*)get_block_ptr(block);
        uint32_t write_size = remaining > FS_BLOCK_DATA_SIZE ? FS_BLOCK_DATA_SIZE : remaining;
        hdr->next_block = FS_BLOCK_END;
        hdr->data_size = write_size;
        
        uint8_t* block_data = get_block_ptr(block) + sizeof(BlockHeader);
        for (uint32_t j = 0; j < write_size; j++) {
            block_data[j] = data_ptr[j];
        }
        
        data_ptr += write_size;
        remaining -= write_size;
        prev_block = block;
    }
    
    // Fill in entry
    str_copy(entry->name, name, FS_MAX_FILENAME + 1);
    entry->parent_dir = parent;
    entry->first_block = first_block;
    entry->size = size;
    entry->type = FILE_TYPE_FILE;
    entry->flags = 0;
    entry->created = 0;
    entry->modified = 0;
    entry->block_count = blocks_needed;
    
    header->file_count++;
    return true;
}

// Remove file or empty directory
bool PersistentFS::remove(const char* path) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    
    if (!parse_path(path, name, &parent)) return false;
    
    uint32_t idx = find_entry_index(name, parent);
    if (idx == INVALID_INDEX) return false;
    if (idx == ROOT_DIR_INDEX) return false;  // Can't delete root
    
    FileEntry* entry = &file_table[idx];
    
    // If directory, check if empty
    if (entry->type == FILE_TYPE_DIR) {
        for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
            if (file_table[i].type != FILE_TYPE_UNUSED &&
                file_table[i].parent_dir == idx) {
                return false;  // Directory not empty
            }
        }
    }
    
    // Free all blocks
    uint32_t block = entry->first_block;
    while (block != INVALID_INDEX && block != FS_BLOCK_END) {
        BlockHeader* hdr = (BlockHeader*)get_block_ptr(block);
        uint32_t next = hdr->next_block;
        free_block(block);
        block = next;
    }
    
    // Clear entry
    entry->type = FILE_TYPE_UNUSED;
    entry->name[0] = '\0';
    header->file_count--;
    
    return true;
}

// Read file
int32_t PersistentFS::read(const char* path, char* buffer, uint32_t max_size) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    
    if (!parse_path(path, name, &parent)) return -1;
    
    FileEntry* entry = find_entry(name, parent);
    if (!entry || entry->type != FILE_TYPE_FILE) return -1;
    
    uint32_t read_size = entry->size < max_size ? entry->size : max_size;
    uint32_t remaining = read_size;
    char* buf_ptr = buffer;
    
    uint32_t block = entry->first_block;
    while (block != INVALID_INDEX && block != FS_BLOCK_END && remaining > 0) {
        BlockHeader* hdr = (BlockHeader*)get_block_ptr(block);
        uint8_t* data = get_block_ptr(block) + sizeof(BlockHeader);
        
        uint32_t copy_size = hdr->data_size < remaining ? hdr->data_size : remaining;
        for (uint32_t i = 0; i < copy_size; i++) {
            buf_ptr[i] = data[i];
        }
        
        buf_ptr += copy_size;
        remaining -= copy_size;
        block = hdr->next_block;
    }
    
    return read_size;
}

// Write file (overwrite or create)
bool PersistentFS::write(const char* path, const char* content, uint32_t size) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    
    if (!parse_path(path, name, &parent)) return false;
    
    // Remove existing file first
    uint32_t idx = find_entry_index(name, parent);
    if (idx != INVALID_INDEX) {
        remove(path);
    }
    
    return create(path, content, size);
}

// Create directory
bool PersistentFS::mkdir(const char* path) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    
    if (!parse_path(path, name, &parent)) return false;
    if (find_entry(name, parent)) return false;  // Already exists
    
    FileEntry* entry = find_free_entry();
    if (!entry) return false;
    
    str_copy(entry->name, name, FS_MAX_FILENAME + 1);
    entry->parent_dir = parent;
    entry->first_block = INVALID_INDEX;
    entry->size = 0;
    entry->type = FILE_TYPE_DIR;
    entry->flags = 0;
    entry->created = 0;
    entry->modified = 0;
    entry->block_count = 0;
    
    header->file_count++;
    return true;
}

// Change directory
bool PersistentFS::chdir(const char* path) {
    if (str_compare(path, "/") == 0) {
        current_dir = ROOT_DIR_INDEX;
        return true;
    }
    if (str_compare(path, "..") == 0) {
        if (current_dir != ROOT_DIR_INDEX) {
            current_dir = file_table[current_dir].parent_dir;
            if (current_dir == INVALID_INDEX) current_dir = ROOT_DIR_INDEX;
        }
        return true;
    }
    
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    
    if (!parse_path(path, name, &parent)) return false;
    
    uint32_t idx = find_entry_index(name, parent);
    if (idx == INVALID_INDEX) return false;
    if (file_table[idx].type != FILE_TYPE_DIR) return false;
    
    current_dir = idx;
    return true;
}

// Get current working directory path
const char* PersistentFS::getcwd() {
    static char path[FS_MAX_PATH];
    
    if (current_dir == ROOT_DIR_INDEX) {
        path[0] = '/';
        path[1] = '\0';
        return path;
    }
    
    // Build path backwards
    char temp[FS_MAX_PATH];
    uint32_t pos = FS_MAX_PATH - 1;
    temp[pos] = '\0';
    
    uint32_t dir = current_dir;
    while (dir != ROOT_DIR_INDEX && dir != INVALID_INDEX) {
        uint32_t len = str_length(file_table[dir].name);
        pos -= len;
        for (uint32_t i = 0; i < len; i++) {
            temp[pos + i] = file_table[dir].name[i];
        }
        pos--;
        temp[pos] = '/';
        dir = file_table[dir].parent_dir;
    }
    
    str_copy(path, &temp[pos], FS_MAX_PATH);
    return path;
}

// File info
bool PersistentFS::exists(const char* path) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    if (!parse_path(path, name, &parent)) return false;
    return find_entry(name, parent) != nullptr;
}

uint32_t PersistentFS::get_size(const char* path) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    if (!parse_path(path, name, &parent)) return 0;
    FileEntry* entry = find_entry(name, parent);
    return entry ? entry->size : 0;
}

bool PersistentFS::is_directory(const char* path) {
    char name[FS_MAX_FILENAME + 1];
    uint32_t parent;
    if (!parse_path(path, name, &parent)) return false;
    FileEntry* entry = find_entry(name, parent);
    return entry && entry->type == FILE_TYPE_DIR;
}

// Listing
uint32_t PersistentFS::get_file_count() {
    return header->file_count;
}

uint32_t PersistentFS::get_dir_file_count(uint32_t dir_index) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].type != FILE_TYPE_UNUSED &&
            file_table[i].parent_dir == dir_index) {
            count++;
        }
    }
    return count;
}

bool PersistentFS::get_file_info(uint32_t dir_index, uint32_t index, char* name, uint32_t* size, uint8_t* type) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].type != FILE_TYPE_UNUSED &&
            file_table[i].parent_dir == dir_index) {
            if (count == index) {
                str_copy(name, file_table[i].name, FS_MAX_FILENAME + 1);
                *size = file_table[i].size;
                *type = file_table[i].type;
                return true;
            }
            count++;
        }
    }
    return false;
}

// Stats
uint32_t PersistentFS::get_used_space() {
    return (FS_MAX_BLOCKS - header->free_blocks) * FS_BLOCK_SIZE;
}

uint32_t PersistentFS::get_free_space() {
    return header->free_blocks * FS_BLOCK_SIZE;
}
