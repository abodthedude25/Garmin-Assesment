
# Data Compression Algorithm(s)

A byte-level compression implementation featuring two algorithms: Simple RLE and Advanced Multi-Strategy compression.

### Algorithms

**Simple RLE**: Baseline run-length encoding
- Compresses runs of 3+ identical bytes
- Minimal overhead
- Fast execution

**Advanced Multi-Strategy**: Multiple compression techniques
- Delta encoding for sequences
- Nibble packing for small values
- Pattern matching
- Zero-run optimization
- Standard RLE fallback

## How the Algorithms Work

### Simple RLE Algorithm

The Simple RLE implementation uses the unused high bit (0x80) as a control flag to distinguish between two encoding modes:

**Run-Length Mode**: When 3 or more identical bytes are detected
- Encodes as: `[0x80 | count] [value]`
- Example: `0x00 0x00 0x00 0x00 0x00` → `0x85 0x00` (5 zeros become 2 bytes)
- Maximum run length: 127 bytes

**Literal Mode**: For non-repeating sequences
- Encodes as: `[count] [literal bytes...]`
- Example: `0x03 0x74 0x45` → `0x03 0x03 0x74 0x45` (3 bytes become 4)
- Used when repetition is less than 3 bytes

The algorithm scans the input buffer once, making decisions at each position:
1. Count consecutive identical bytes
2. If count ≥ 3, use RLE mode
3. Otherwise, batch non-repeating bytes in literal mode
4. Look ahead to avoid splitting upcoming runs

### Advanced Multi-Strategy Algorithm

The Advanced algorithm analyzes data patterns and selects the optimal compression strategy for each segment:

**Strategy Selection Process**:
1. **Zero Run Detection** (0xF0 prefix)
   - Special case for runs of 0x00 (very common in data)
   - Encodes as: `[0xF0] [count]`
   - Example: Eight zeros → `0xF0 0x08` (2 bytes)

2. **Delta Sequence Detection** (0xC0-0xFF range)
   - Identifies arithmetic progressions
   - Encodes as: `[0xC0 | length] [start_value] [delta+16]`
   - Example: `0x10 0x11 0x12 0x13 0x14` → `0xC5 0x10 0x11` (5 bytes become 3)
   - Handles both increasing and decreasing sequences

3. **Nibble Packing** (0x40-0x7F range)
   - For sequences where all values < 16
   - Packs two 4-bit values into one byte
   - Encodes as: `[0x40 | count] [packed_nibbles...]`
   - Example: `0x01 0x02 0x03 0x04` → `0x44 0x12 0x34` (4 bytes become 3)

4. **Pattern Matching** (0xE0 prefix)
   - Detects repeating multi-byte patterns
   - Encodes as: `[0xE0] [pattern_length << 4 | repeat_count] [pattern...]`
   - Example: `0x12 0x34 0x12 0x34 0x12 0x34` → `0xE0 0x23 0x12 0x34` (6 bytes become 4)
   - Searches for patterns from 2 to 16 bytes long

5. **Common Value Optimization** (0xF2 prefix)
   - Dictionary of frequent values (0x00, 0x01, 0x02, etc.)
   - Encodes as: `[0xF2] [count << 4 | dictionary_index]`
   - Saves one byte for short runs of common values

6. **Standard RLE** (0x80-0xBF range)
   - Falls back to simple run-length encoding
   - Similar to Simple RLE but with 6-bit length field
   - Maximum run: 63 bytes

7. **Literal Mode** (0x00-0x3F range)
   - For truly random, uncompressible data
   - Stores data as-is with a length prefix
   - Maximum literal sequence: 63 bytes

**Decision Flow**:
The algorithm evaluates each position in order of compression potential:
1. Check for zero runs (highest compression)
2. Check for delta sequences (good for sequential data)
3. Check for nibble packing (good for small values)
4. Check for repeating patterns (good for structured data)
5. Check for simple runs (standard RLE)
6. Default to literal mode (no compression)

# Compression Example Walkthrough

## Original Data (24 bytes)
```
0x03, 0x74, 0x04, 0x04, 0x04, 0x35, 0x35, 0x64,
0x64, 0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00,
0x56, 0x45, 0x56, 0x56, 0x56, 0x09, 0x09, 0x09
```

## Simple RLE Compression

### Step-by-Step Process

| Position | Data | Analysis | Encoding | Output |
|----------|------|----------|----------|--------|
| 0-2 | `0x03, 0x74` | Different values | Literal: `[2] [values...]` | `0x02 0x03 0x74` |
| 2-5 | `0x04, 0x04, 0x04` | 3 identical (RLE threshold) | RLE: `[0x80\|3] [0x04]` | `0x83 0x04` |
| 5-7 | `0x35, 0x35` | Only 2 identical | Literal: `[2] [values...]` | `0x02 0x35 0x35` |
| 7-11 | `0x64, 0x64, 0x64, 0x64` | 4 identical | RLE: `[0x80\|4] [0x64]` | `0x84 0x64` |
| 11-16 | `0x00, 0x00, 0x00, 0x00, 0x00` | 5 identical | RLE: `[0x80\|5] [0x00]` | `0x85 0x00` |
| 16-18 | `0x56, 0x45` | Different values | Literal: `[2] [values...]` | `0x02 0x56 0x45` |
| 18-21 | `0x56, 0x56, 0x56` | 3 identical | RLE: `[0x80\|3] [0x56]` | `0x83 0x56` |
| 21-24 | `0x09, 0x09, 0x09` | 3 identical | RLE: `[0x80\|3] [0x09]` | `0x83 0x09` |

**Compression: 24 → 19 bytes (20.8% saved)**

## Advanced Multi-Strategy Compression

### Step-by-Step Process

| Position | Data | Strategy Checks | Selected Strategy | Output |
|----------|------|-----------------|-------------------|--------|
| 0-2 | `0x03, 0x74` | ❌ Zero run<br>❌ Delta (gap too large)<br>❌ Nibble (values > 16)<br>❌ Pattern<br>❌ RLE | **Literal mode** | `0x02 0x03 0x74` |
| 2-5 | `0x04, 0x04, 0x04` | ❌ Zero run<br>❌ Delta (no progression)<br>✓ Could nibble pack<br>❌ Pattern<br>✓ RLE works | **RLE mode** | `0xC3 0x04 0x10` |
| 5-7 | `0x35, 0x35` | ❌ All strategies<br>(only 2 bytes) | **Literal mode** | `0x02 0x35 0x35` |
| 7-11 | `0x64, 0x64, 0x64, 0x64` | ❌ Zero run<br>❌ Delta<br>❌ Nibble (0x64 > 16)<br>❌ Pattern<br>✓ RLE | **RLE mode** | `0xC4 0x64 0x10` |
| 11-16 | `0x00, 0x00, 0x00, 0x00, 0x00` | ✓ Zero run detected! | **Zero run special** | `0xF0 0x05` |
| 16-18 | `0x56, 0x45` | ❌ All strategies | **Literal mode** | `0x02 0x56 0x45` |
| 18-21 | `0x56, 0x56, 0x56` | ❌ Special modes<br>✓ RLE | **RLE mode** | `0xC3 0x56 0x10` |
| 21-24 | `0x09, 0x09, 0x09` | ❌ Special modes<br>✓ RLE | **RLE mode** | `0xC3 0x09 0x10` |

**Compression: 24 → 23 bytes (4.2% saved)**

## Comparison Summary

| Aspect | Simple RLE | Advanced |
|--------|------------|----------|
| **Compressed size** | 19 bytes | 23 bytes |
| **Compression ratio** | 20.8% | 4.2% |
| **Winner for this data** | ✓ Better | Less efficient |
### Key Differences

Zero handling: Advanced uses special 2-byte encoding for the 5 zeros, Simple RLE also uses 2 bytes but different format
Overhead: Advanced algorithm has more complex encoding which sometimes adds bytes
This specific data: Doesn't have sequences or patterns that Advanced excels at, so Simple RLE actually performs better here

The Advanced algorithm shines when you have:

Long sequences (0x10, 0x11, 0x12, 0x13...)
Nibble data (0x01, 0x02, 0x03...)
Repeating patterns (0x12, 0x34, 0x12, 0x34...)

For this particular example with mostly short runs, Simple RLE is more efficient!


## Usage

### Compilation
```bash
gcc -O2 -o compress compress.c -lm
```

### API
```c
// Main compression function
size_t byte_compress(uint8_t* data_ptr, size_t data_size);

// Main decompression function  
size_t byte_decompress(uint8_t* data_ptr, size_t compressed_size);

// Alternative algorithms
size_t simple_rle_compress(uint8_t* data_ptr, size_t data_size);
size_t simple_rle_decompress(uint8_t* data_ptr, size_t compressed_size);
size_t advanced_compress(uint8_t* data_ptr, size_t data_size);
size_t advanced_decompress(uint8_t* data_ptr, size_t compressed_size);
```

### Example
```c
uint8_t data[] = {0x03, 0x74, 0x04, 0x04, 0x04, 0x35, 0x35, 0x64};
size_t original_size = sizeof(data);

// Compress
size_t compressed_size = byte_compress(data, original_size);

// Data is now compressed in-place
// Decompress
size_t decompressed_size = byte_decompress(data, compressed_size);
```

## Performance

### Compression Ratios

| Data Type | Simple RLE | Advanced | 
|-----------|------------|----------|
| All zeros (256B) | 97.3% | 98.4% |
| Random runs (256B) | 65.6% | 52.0% |
| Incrementing sequence (256B) | -1.2% | 94.1% |
| Mixed patterns (256B) | 23.4% | 33.2% |
| Example data (24B) | 20.8% | 4.2% |

### Speed

| Metric | Simple RLE | Advanced |
|--------|------------|----------|
| Throughput | 211 MB/s | 101 MB/s |
| Time per operation | 1.2 μs | 2.5 μs |

## Algorithm Selection

**Use Simple RLE when:**
- Speed is critical
- Data contains primarily long runs of identical bytes
- Minimal code complexity required
- Real-time constraints

**Use Advanced when:**
- Maximum compression needed
- Data contains sequences, patterns, or small values
- Batch processing scenarios
- Mixed data patterns

## Testing

Run the built-in test suite:
```bash
./compress
```

The test suite includes:
- Original example validation
- 7 different data patterns
- Size scaling tests (16B to 4KB)
- 10,000 iteration speed benchmark
- Automatic verification of round-trip accuracy

## Files

- `compress.c` - Complete implementation with both algorithms and test suite


## Note: 
I did use AI to help write this readme for me in a very well formatted form, saving me time, as I had a lot of research(work) deliverables due these 4 days! Also, the testing prints in the compress.c file after writting the compress algorithim on my own.