#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/**
 * Combined Compression Algorithms Implementation
 * 
 * This file contains:
 * 1. Simple RLE compression (baseline)
 * 2. Advanced Multi-Strategy compression
 * 3. Comprehensive performance testing suite
 */

// SIMPLE RLE COMPRESSION (Baseline Algorithm)

#define RLE_FLAG 0x80
#define MIN_RUN_LENGTH 2
#define MAX_RUN_LENGTH 127

size_t simple_rle_compress(uint8_t* data_ptr, size_t data_size) {
    if (!data_ptr || data_size == 0) return 0;
    
    uint8_t* temp_buffer = (uint8_t*)malloc(data_size * 2);
    if (!temp_buffer) return data_size;
    
    size_t write_pos = 0;
    size_t read_pos = 0;
    
    while (read_pos < data_size) {
        size_t run_length = 1;
        uint8_t current_byte = data_ptr[read_pos];
        
        while (read_pos + run_length < data_size && 
               data_ptr[read_pos + run_length] == current_byte &&
               run_length < MAX_RUN_LENGTH) {
            run_length++;
        }
        
        if (run_length >= MIN_RUN_LENGTH) {
            temp_buffer[write_pos++] = RLE_FLAG | (uint8_t)run_length;
            temp_buffer[write_pos++] = current_byte;
            read_pos += run_length;
        } else {
            size_t literal_start = read_pos;
            size_t literal_count = 0;
            
            while (read_pos < data_size && literal_count < MAX_RUN_LENGTH) {
                size_t ahead_run = 1;
                if (read_pos + 1 < data_size) {
                    while (read_pos + ahead_run < data_size && 
                           data_ptr[read_pos + ahead_run] == data_ptr[read_pos] &&
                           ahead_run < MAX_RUN_LENGTH) {
                        ahead_run++;
                    }
                }
                
                if (ahead_run >= MIN_RUN_LENGTH) break;
                
                read_pos += ahead_run;
                literal_count += ahead_run;
            }
            
            temp_buffer[write_pos++] = (uint8_t)literal_count;
            memcpy(&temp_buffer[write_pos], &data_ptr[literal_start], literal_count);
            write_pos += literal_count;
        }
    }
    
    memcpy(data_ptr, temp_buffer, write_pos);
    free(temp_buffer);
    return write_pos;
}

size_t simple_rle_decompress(uint8_t* data_ptr, size_t compressed_size) {
    if (!data_ptr || compressed_size == 0) return 0;
    
    uint8_t* temp_buffer = (uint8_t*)malloc(compressed_size * MAX_RUN_LENGTH);
    if (!temp_buffer) return compressed_size;
    
    size_t write_pos = 0;
    size_t read_pos = 0;
    
    while (read_pos < compressed_size) {
        uint8_t control_byte = data_ptr[read_pos++];
        
        if (control_byte & RLE_FLAG) {
            size_t run_length = control_byte & 0x7F;
            if (read_pos < compressed_size) {
                uint8_t value = data_ptr[read_pos++];
                for (size_t i = 0; i < run_length; i++) {
                    temp_buffer[write_pos++] = value;
                }
            }
        } else {
            size_t literal_count = control_byte;
            for (size_t i = 0; i < literal_count && read_pos < compressed_size; i++) {
                temp_buffer[write_pos++] = data_ptr[read_pos++];
            }
        }
    }
    
    memcpy(data_ptr, temp_buffer, write_pos);
    free(temp_buffer);
    return write_pos;
}

// ADVANCED MULTI-STRATEGY COMPRESSION

#define MODE_RLE        0x80
#define MODE_DELTA      0xC0
#define MODE_NIBBLE     0x40
#define MODE_LITERAL    0x00
#define MODE_MASK       0xC0
#define LENGTH_MASK     0x3F

#define EXT_PATTERN     0xE0
#define EXT_ZERO_RUN    0xF0
#define EXT_INCR_SEQ    0xF1
#define EXT_COMMON_VAL  0xF2

static const uint8_t common_values[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0xFF, 0x7F, 0x20};
#define NUM_COMMON_VALUES 8

typedef struct {
    uint8_t pattern[16];
    size_t length;
    size_t count;
} Pattern;

Pattern find_pattern(uint8_t* data, size_t start, size_t data_size) {
    Pattern best = {0};
    
    for (size_t pattern_len = 2; pattern_len <= 16 && start + pattern_len * 2 <= data_size; pattern_len++) {
        size_t matches = 1;
        
        for (size_t i = start + pattern_len; i + pattern_len <= data_size; i += pattern_len) {
            if (memcmp(&data[start], &data[i], pattern_len) == 0) {
                matches++;
            } else {
                break;
            }
        }
        
        if (matches >= 2) {
            size_t saved = (matches * pattern_len) - (2 + pattern_len);
            size_t best_saved = (best.count * best.length) - (2 + best.length);
            
            if (saved > best_saved) {
                memcpy(best.pattern, &data[start], pattern_len);
                best.length = pattern_len;
                best.count = matches;
            }
        }
    }
    
    return best;
}

bool is_delta_sequence(uint8_t* data, size_t start, size_t data_size, int* delta, size_t* length) {
    if (start + 2 > data_size) return false;
    
    *delta = (int)data[start + 1] - (int)data[start];
    *length = 2;
    
    for (size_t i = start + 2; i < data_size && *length < 63; i++) {
        int expected = (data[i-1] + *delta) & 0x7F;
        if (data[i] != expected) break;
        (*length)++;
    }
    
    return (*length >= 3) && (*delta >= -15 && *delta <= 15);
}

bool can_nibble_pack(uint8_t* data, size_t start, size_t data_size, size_t* length) {
    *length = 0;
    
    for (size_t i = start; i < data_size && i < start + 62; i++) {
        if (data[i] >= 16) break;
        (*length)++;
    }
    
    return *length >= 4;
}

size_t advanced_compress(uint8_t* data_ptr, size_t data_size) {
    if (!data_ptr || data_size == 0) return 0;
    
    uint8_t* output = (uint8_t*)malloc(data_size * 2);
    if (!output) return data_size;
    
    size_t out_pos = 0;
    size_t in_pos = 0;
    
    while (in_pos < data_size) {
        uint8_t current = data_ptr[in_pos];
        
        // Check for zero runs
        if (current == 0x00) {
            size_t zero_count = 1;
            while (in_pos + zero_count < data_size && 
                   data_ptr[in_pos + zero_count] == 0x00 && 
                   zero_count < 255) {
                zero_count++;
            }
            
            if (zero_count >= 3) {
                output[out_pos++] = EXT_ZERO_RUN;
                output[out_pos++] = (uint8_t)zero_count;
                in_pos += zero_count;
                continue;
            }
        }
        
        // Check for delta sequences
        int delta;
        size_t delta_length;
        if (is_delta_sequence(data_ptr, in_pos, data_size, &delta, &delta_length)) {
            output[out_pos++] = MODE_DELTA | (uint8_t)delta_length;
            output[out_pos++] = data_ptr[in_pos];
            output[out_pos++] = (uint8_t)(delta + 16);
            in_pos += delta_length;
            continue;
        }
        
        // Check for nibble packing
        size_t nibble_length;
        if (can_nibble_pack(data_ptr, in_pos, data_size, &nibble_length)) {
            size_t pairs = nibble_length / 2;
            output[out_pos++] = MODE_NIBBLE | (uint8_t)nibble_length;
            
            for (size_t i = 0; i < pairs; i++) {
                uint8_t packed = (data_ptr[in_pos + i*2] << 4) | 
                                data_ptr[in_pos + i*2 + 1];
                output[out_pos++] = packed;
            }
            
            if (nibble_length % 2) {
                output[out_pos++] = data_ptr[in_pos + nibble_length - 1] << 4;
            }
            
            in_pos += nibble_length;
            continue;
        }
        
        // Pattern matching
        Pattern pattern = find_pattern(data_ptr, in_pos, data_size);
        if (pattern.count >= 2 && pattern.length >= 2) {
            output[out_pos++] = EXT_PATTERN;
            output[out_pos++] = (uint8_t)((pattern.length << 4) | (pattern.count & 0x0F));
            memcpy(&output[out_pos], pattern.pattern, pattern.length);
            out_pos += pattern.length;
            in_pos += pattern.length * pattern.count;
            continue;
        }
        
        // Standard RLE
        size_t run_length = 1;
        while (in_pos + run_length < data_size && 
               data_ptr[in_pos + run_length] == current &&
               run_length < 63) {
            run_length++;
        }
        
        if (run_length >= 3) {
            int common_idx = -1;
            for (int i = 0; i < NUM_COMMON_VALUES; i++) {
                if (common_values[i] == current) {
                    common_idx = i;
                    break;
                }
            }
            
            if (common_idx >= 0 && run_length <= 15) {
                output[out_pos++] = EXT_COMMON_VAL;
                output[out_pos++] = (uint8_t)((run_length << 4) | common_idx);
            } else {
                output[out_pos++] = MODE_RLE | (uint8_t)run_length;
                output[out_pos++] = current;
            }
            in_pos += run_length;
        } else {
            // Literal mode
            size_t literal_count = 0;
            size_t literal_start = in_pos;
            
            while (in_pos < data_size && literal_count < 63) {
                size_t ahead_run = 1;
                if (in_pos + 1 < data_size) {
                    while (in_pos + ahead_run < data_size && 
                           data_ptr[in_pos + ahead_run] == data_ptr[in_pos]) {
                        ahead_run++;
                    }
                }
                
                if (ahead_run >= 3) break;
                
                int test_delta;
                size_t test_length;
                if (is_delta_sequence(data_ptr, in_pos, data_size, &test_delta, &test_length)) {
                    break;
                }
                
                in_pos++;
                literal_count++;
            }
            
            output[out_pos++] = MODE_LITERAL | (uint8_t)literal_count;
            memcpy(&output[out_pos], &data_ptr[literal_start], literal_count);
            out_pos += literal_count;
        }
    }
    
    memcpy(data_ptr, output, out_pos);
    free(output);
    return out_pos;
}

size_t advanced_decompress(uint8_t* data_ptr, size_t compressed_size) {
    if (!data_ptr || compressed_size == 0) return 0;
    
    uint8_t* output = (uint8_t*)malloc(compressed_size * 256);
    if (!output) return compressed_size;
    
    size_t out_pos = 0;
    size_t in_pos = 0;
    
    while (in_pos < compressed_size) {
        uint8_t control = data_ptr[in_pos++];
        
        if (control == EXT_ZERO_RUN) {
            size_t count = data_ptr[in_pos++];
            memset(&output[out_pos], 0, count);
            out_pos += count;
        } 
        else if (control == EXT_PATTERN) {
            uint8_t info = data_ptr[in_pos++];
            size_t pattern_len = info >> 4;
            size_t repeat_count = info & 0x0F;
            
            for (size_t i = 0; i < repeat_count; i++) {
                memcpy(&output[out_pos], &data_ptr[in_pos], pattern_len);
                out_pos += pattern_len;
            }
            in_pos += pattern_len;
        }
        else if (control == EXT_COMMON_VAL) {
            uint8_t info = data_ptr[in_pos++];
            size_t count = info >> 4;
            uint8_t val_idx = info & 0x0F;
            uint8_t value = common_values[val_idx];
            
            memset(&output[out_pos], value, count);
            out_pos += count;
        }
        else {
            uint8_t mode = control & MODE_MASK;
            size_t length = control & LENGTH_MASK;
            
            if (mode == MODE_RLE) {
                uint8_t value = data_ptr[in_pos++];
                memset(&output[out_pos], value, length);
                out_pos += length;
            }
            else if (mode == MODE_DELTA) {
                uint8_t start = data_ptr[in_pos++];
                int delta = (int)data_ptr[in_pos++] - 16;
                
                for (size_t i = 0; i < length; i++) {
                    output[out_pos++] = (start + i * delta) & 0x7F;
                }
            }
            else if (mode == MODE_NIBBLE) {
                size_t pairs = length / 2;
                for (size_t i = 0; i < pairs; i++) {
                    uint8_t packed = data_ptr[in_pos++];
                    output[out_pos++] = packed >> 4;
                    output[out_pos++] = packed & 0x0F;
                }
                if (length % 2) {
                    output[out_pos++] = data_ptr[in_pos++] >> 4;
                }
            }
            else {
                memcpy(&output[out_pos], &data_ptr[in_pos], length);
                out_pos += length;
                in_pos += length;
            }
        }
    }
    
    memcpy(data_ptr, output, out_pos);
    free(output);
    return out_pos;
}

// INTERFACE FUNCTIONS
size_t byte_compress(uint8_t* data_ptr, size_t data_size) {
    return advanced_compress(data_ptr, data_size);
}
size_t byte_decompress(uint8_t* data_ptr, size_t compressed_size) {
    return advanced_decompress(data_ptr, compressed_size);
}

// COMPREHENSIVE TESTING SUITE

typedef struct {
    const char* name;
    size_t (*compress)(uint8_t*, size_t);
    size_t (*decompress)(uint8_t*, size_t);
} Algorithm;

typedef struct {
    const char* name;
    uint8_t* data;
    size_t size;
    const char* description;
} TestCase;

typedef struct {
    double compression_ratio;
    double compression_time_ms;
    double decompression_time_ms;
    bool verified;
    size_t compressed_size;
    size_t original_size;
} TestResult;

// Performance timer
double get_time_ms() {
    return (double)clock() / CLOCKS_PER_SEC * 1000.0;
}

// Generate test data patterns
uint8_t* generate_pattern(const char* type, size_t size) {
    uint8_t* data = (uint8_t*)malloc(size);
    if (!data) return NULL;
    
    if (strcmp(type, "zeros") == 0) {
        memset(data, 0x00, size);
    }
    else if (strcmp(type, "random") == 0) {
        for (size_t i = 0; i < size; i++) {
            data[i] = rand() & 0x7F;
        }
    }
    else if (strcmp(type, "runs") == 0) {
        size_t pos = 0;
        while (pos < size) {
            uint8_t value = rand() & 0x7F;
            size_t run_len = (rand() % 10) + 1;
            for (size_t i = 0; i < run_len && pos < size; i++, pos++) {
                data[pos] = value;
            }
        }
    }
    else if (strcmp(type, "sequence") == 0) {
        for (size_t i = 0; i < size; i++) {
            data[i] = (i % 128);
        }
    }
    else if (strcmp(type, "pattern") == 0) {
        uint8_t pattern[] = {0x12, 0x34, 0x56, 0x78};
        for (size_t i = 0; i < size; i++) {
            data[i] = pattern[i % 4];
        }
    }
    else if (strcmp(type, "mixed") == 0) {
        size_t pos = 0;
        while (pos < size) {
            int choice = rand() % 4;
            size_t chunk_size = (rand() % 20) + 5;
            
            for (size_t i = 0; i < chunk_size && pos < size; i++, pos++) {
                if (choice == 0) data[pos] = 0x00;
                else if (choice == 1) data[pos] = pos & 0x7F;
                else if (choice == 2) data[pos] = (pos / 3) & 0x7F;
                else data[pos] = rand() & 0x7F;
            }
        }
    }
    else if (strcmp(type, "nibbles") == 0) {
        for (size_t i = 0; i < size; i++) {
            data[i] = rand() & 0x0F;
        }
    }
    
    return data;
}

// Run single test
TestResult run_single_test(Algorithm* algo, uint8_t* original_data, size_t size) {
    TestResult result = {0};
    result.original_size = size;
    
    // Make working copies
    uint8_t* compress_buffer = (uint8_t*)malloc(size * 2);
    uint8_t* verify_buffer = (uint8_t*)malloc(size * 2);
    
    if (!compress_buffer || !verify_buffer) {
        free(compress_buffer);
        free(verify_buffer);
        return result;
    }
    
    memcpy(compress_buffer, original_data, size);
    
    // Compress
    double start_time = get_time_ms();
    size_t compressed_size = algo->compress(compress_buffer, size);
    result.compression_time_ms = get_time_ms() - start_time;
    result.compressed_size = compressed_size;
    
    // Calculate compression ratio
    result.compression_ratio = (1.0 - (double)compressed_size / size) * 100.0;
    
    // Decompress
    memcpy(verify_buffer, compress_buffer, compressed_size);
    start_time = get_time_ms();
    size_t decompressed_size = algo->decompress(verify_buffer, compressed_size);
    result.decompression_time_ms = get_time_ms() - start_time;
    
    // Verify
    result.verified = (decompressed_size == size) && 
                     (memcmp(original_data, verify_buffer, size) == 0);
    
    free(compress_buffer);
    free(verify_buffer);
    
    return result;
}

// Print comparison table
void print_comparison_header() {
    printf("\n╔════════════════════════════╦═══════════════════╦═══════════════════╦═══════════╦═══════════╗\n");
    printf("║ Test Case                  ║ Simple RLE        ║ Advanced Multi    ║ Winner    ║ Advantage ║\n");
    printf("╠════════════════════════════╬═══════════════════╬═══════════════════╬═══════════╬═══════════╣\n");
}

void print_comparison_row(const char* test_name, TestResult* simple, TestResult* advanced) {
    const char* winner = "Draw";
    double advantage = 0;
    
    if (simple->compression_ratio > advanced->compression_ratio + 0.1) {
        winner = "Simple";
        advantage = simple->compression_ratio - advanced->compression_ratio;
    } else if (advanced->compression_ratio > simple->compression_ratio + 0.1) {
        winner = "Advanced";
        advantage = advanced->compression_ratio - simple->compression_ratio;
    }
    
    printf("║ %-26s ║ %6.1f%% (%4zu B) ║ %6.1f%% (%4zu B) ║ %-9s ║ %+6.1f%%  ║\n",
           test_name,
           simple->compression_ratio, simple->compressed_size,
           advanced->compression_ratio, advanced->compressed_size,
           winner, advantage);
}

void print_comparison_footer() {
    printf("╚════════════════════════════╩═══════════════════╩═══════════════════╩═══════════╩═══════════╝\n");
}

// Main testing function
void run_comprehensive_tests() {
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf("           COMPRESSION ALGORITHM PERFORMANCE COMPARISON                \n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    
    Algorithm algorithms[] = {
        {"Simple RLE", simple_rle_compress, simple_rle_decompress},
        {"Advanced Multi-Strategy", advanced_compress, advanced_decompress}
    };
    
    // Test with original example
    uint8_t original_example[] = { 
        0x03, 0x74, 0x04, 0x04, 0x04, 0x35, 0x35, 0x64,
        0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x56, 0x45, 0x56, 0x56, 0x56, 0x09, 0x09, 0x09 
    };
    
    printf("\n1. ORIGINAL EXAMPLE TEST\n");
    printf("   ─────────────────────\n");
    
    for (int a = 0; a < 2; a++) {
        uint8_t* test_data = (uint8_t*)malloc(sizeof(original_example));
        memcpy(test_data, original_example, sizeof(original_example));
        
        TestResult result = run_single_test(&algorithms[a], original_example, sizeof(original_example));
        
        printf("   %s:\n", algorithms[a].name);
        printf("   • Compression: %zu → %zu bytes (%.1f%% saved)\n",
               result.original_size, result.compressed_size, result.compression_ratio);
        printf("   • Time: %.3f ms compress, %.3f ms decompress\n",
               result.compression_time_ms, result.decompression_time_ms);
        printf("   • Verification: %s\n", result.verified ? "✓ PASSED" : "✗ FAILED");
        
        free(test_data);
    }
    
    // Pattern-based tests
    const char* patterns[] = {
        "zeros", "runs", "sequence", "pattern", "nibbles", "mixed", "random"
    };
    const char* descriptions[] = {
        "All zeros",
        "Random runs",
        "Incrementing",
        "Repeating pattern",
        "Small values <16",
        "Mixed patterns",
        "Random data"
    };
    
    printf("\n2. PATTERN-BASED PERFORMANCE TESTS (256 bytes each)\n");
    printf("   ────────────────────────────────────────────────\n");
    
    print_comparison_header();
    
    double total_simple_ratio = 0;
    double total_advanced_ratio = 0;
    int test_count = 0;
    
    for (int p = 0; p < 7; p++) {
        uint8_t* test_data = generate_pattern(patterns[p], 256);
        if (!test_data) continue;
        
        TestResult simple_result = run_single_test(&algorithms[0], test_data, 256);
        TestResult advanced_result = run_single_test(&algorithms[1], test_data, 256);
        
        print_comparison_row(descriptions[p], &simple_result, &advanced_result);
        
        total_simple_ratio += simple_result.compression_ratio;
        total_advanced_ratio += advanced_result.compression_ratio;
        test_count++;
        
        free(test_data);
    }
    
    print_comparison_footer();
    
    // Size scaling tests
    printf("\n3. SIZE SCALING TESTS (Mixed Pattern)\n");
    printf("   ──────────────────────────────────\n");
    
    size_t sizes[] = {16, 64, 256, 1024, 4096};
    
    print_comparison_header();
    
    for (int s = 0; s < 5; s++) {
        uint8_t* test_data = generate_pattern("mixed", sizes[s]);
        if (!test_data) continue;
        
        TestResult simple_result = run_single_test(&algorithms[0], test_data, sizes[s]);
        TestResult advanced_result = run_single_test(&algorithms[1], test_data, sizes[s]);
        
        char test_name[32];
        snprintf(test_name, sizeof(test_name), "%zu bytes", sizes[s]);
        print_comparison_row(test_name, &simple_result, &advanced_result);
        
        free(test_data);
    }
    
    print_comparison_footer();
    
    // Speed benchmark
    printf("\n4. SPEED BENCHMARK (10000 iterations on 256-byte buffer)\n");
    printf("   ─────────────────────────────────────────────────────\n");
    
    uint8_t* bench_data = generate_pattern("mixed", 256);
    uint8_t* work_buffer = (uint8_t*)malloc(512);
    
    for (int a = 0; a < 2; a++) {
        memcpy(work_buffer, bench_data, 256);
        
        double start = get_time_ms();
        for (int i = 0; i < 10000; i++) {
            memcpy(work_buffer, bench_data, 256);
            algorithms[a].compress(work_buffer, 256);
        }
        double compress_time = get_time_ms() - start;
        
        printf("   %s:\n", algorithms[a].name);
        printf("   • Compression: %.2f ms total, %.4f μs per operation\n",
               compress_time, compress_time * 1000 / 10000);
        printf("   • Throughput: %.2f MB/s\n", 
               (256.0 * 10000) / (compress_time * 1000));
    }
    
    free(bench_data);
    free(work_buffer);
    
    // Summary
    printf("\n5. SUMMARY & RECOMMENDATIONS\n");
    printf("   ─────────────────────────\n");
    
    double avg_simple = total_simple_ratio / test_count;
    double avg_advanced = total_advanced_ratio / test_count;
    
    printf("   • Average compression: Simple RLE = %.1f%%, Advanced = %.1f%%\n",
           avg_simple, avg_advanced);
    printf("   • Advanced algorithm achieves %.1f%% better compression on average\n",
           avg_advanced - avg_simple);
    printf("\n   Recommendations:\n");
    printf("   • Use Simple RLE when: Speed is critical, data has long runs only\n");
    printf("   • Use Advanced when: Maximum compression needed, varied patterns\n");
    printf("   • Advanced excels at: Sequences, patterns, small values, zero runs\n");
    printf("   • Simple RLE excels at: Speed, simplicity, pure run-length data\n");
    
    printf("\n════════════════════════════════════════════════════════════════════════\n");
}

// Main function
int main() {
    // Seed random number generator
    srand(time(NULL));
    
    // Run comprehensive tests
    run_comprehensive_tests();
    
    // Quick demo with original data
    printf("\n\nQUICK DEMONSTRATION\n");
    printf("═══════════════════\n\n");
    
    uint8_t demo_data[] = { 
        0x03, 0x74, 0x04, 0x04, 0x04, 0x35, 0x35, 0x64,
        0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x56, 0x45, 0x56, 0x56, 0x56, 0x09, 0x09, 0x09 
    };
    
    printf("Original data (%zu bytes):\n", sizeof(demo_data));
    for (size_t i = 0; i < sizeof(demo_data); i++) {
        printf("0x%02X ", demo_data[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    
    // Test Simple RLE
    uint8_t simple_test[100];
    memcpy(simple_test, demo_data, sizeof(demo_data));
    size_t simple_size = simple_rle_compress(simple_test, sizeof(demo_data));
    
    printf("\nSimple RLE compressed (%zu bytes, %.1f%% saved):\n", 
           simple_size, (1.0 - (double)simple_size/sizeof(demo_data)) * 100);
    for (size_t i = 0; i < simple_size; i++) {
        printf("0x%02X ", simple_test[i]);
        if ((i + 1) % 8 == 0 && i + 1 < simple_size) printf("\n");
    }
    
    // Test Advanced
    uint8_t advanced_test[100];
    memcpy(advanced_test, demo_data, sizeof(demo_data));
    size_t advanced_size = advanced_compress(advanced_test, sizeof(demo_data));
    
    printf("\n\nAdvanced compressed (%zu bytes, %.1f%% saved):\n", 
           advanced_size, (1.0 - (double)advanced_size/sizeof(demo_data)) * 100);
    for (size_t i = 0; i < advanced_size; i++) {
        printf("0x%02X ", advanced_test[i]);
        if ((i + 1) % 8 == 0 && i + 1 < advanced_size) printf("\n");
    }
    
    // Verify decompression
    uint8_t verify_simple[100];
    memcpy(verify_simple, simple_test, simple_size);
    size_t simple_decompressed = simple_rle_decompress(verify_simple, simple_size);
    
    uint8_t verify_advanced[100];
    memcpy(verify_advanced, advanced_test, advanced_size);
    size_t advanced_decompressed = advanced_decompress(verify_advanced, advanced_size);
    
    bool simple_correct = (simple_decompressed == sizeof(demo_data)) &&
                          (memcmp(demo_data, verify_simple, sizeof(demo_data)) == 0);
    bool advanced_correct = (advanced_decompressed == sizeof(demo_data)) &&
                            (memcmp(demo_data, verify_advanced, sizeof(demo_data)) == 0);
    
    printf("\n\nVerification:\n");
    printf("• Simple RLE decompression: %s\n", simple_correct ? "✓ PASSED" : "✗ FAILED");
    printf("• Advanced decompression: %s\n", advanced_correct ? "✓ PASSED" : "✗ FAILED");
    
    return 0;
}