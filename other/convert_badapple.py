#!/usr/bin/env python3
"""
Convert BadApple text RLE format to packed binary format.
Input: Text file with RLE encoding
Format: "row:col count+char count+char ..."
  Example: "0:39 1%" means row 0, starting at col 39, place 1 '%' character
Output: Binary file with packed bits (8 pixels per byte)
"""

import sys

def parse_rle_to_frames(filename):
    """Parse the RLE text file and convert to frames."""
    frames = []
    current_frame = None
    
    with open(filename, 'r') as f:
        for line in f:
            line = line.rstrip('\n')
            
            if line == 'F':  # Frame start
                if current_frame is not None:
                    frames.append(current_frame)
                # Initialize new frame with spaces
                current_frame = [[' ' for _ in range(64)] for _ in range(24)]
                
            elif line == 'E':  # Frame end
                if current_frame is not None:
                    frames.append(current_frame)
                    current_frame = None
                    
            elif ':' in line:  # Row data
                if current_frame is None:
                    # Start new frame if not started
                    current_frame = [[' ' for _ in range(64)] for _ in range(24)]
                
                # Split only on first colon
                colon_pos = line.index(':')
                row_num = int(line[:colon_pos])
                
                if row_num >= 24:
                    continue
                
                rest = line[colon_pos + 1:]
                
                # Extract column start (digits at the beginning)
                col_start = 0
                i = 0
                while i < len(rest) and rest[i].isdigit():
                    col_start = col_start * 10 + int(rest[i])
                    i += 1
                
                # Skip any spaces
                while i < len(rest) and rest[i] == ' ':
                    i += 1
                
                # Rest is the character data
                char_data = rest[i:]
                
                # Parse character data: "count+char count+char ..."
                col = col_start
                j = 0
                while j < len(char_data) and col < 64:
                    if char_data[j].isdigit():
                        # Extract count
                        count_str = ''
                        while j < len(char_data) and char_data[j].isdigit():
                            count_str += char_data[j]
                            j += 1
                        count = int(count_str)
                        
                        # Next character is what to place
                        if j < len(char_data):
                            char = char_data[j]
                            for _ in range(count):
                                if col < 64:
                                    current_frame[row_num][col] = char
                                    col += 1
                            j += 1
                    else:
                        # No count, single character
                        current_frame[row_num][col] = char_data[j]
                        col += 1
                        j += 1
    
    # Add last frame if exists
    if current_frame is not None:
        frames.append(current_frame)
    
    return frames

def frame_to_binary(frame):
    """Convert a frame (24 rows x 64 cols) to packed binary."""
    bits = []
    for row in frame:
        for char in row:
            # '#' or '@' or any non-space = 1 (pixel on)
            # ' ' = 0 (pixel off)
            bits.append(1 if char != ' ' else 0)
    
    # Pack bits into bytes (MSB first)
    bytes_data = bytearray()
    for i in range(0, len(bits), 8):
        byte_bits = bits[i:i+8]
        byte_val = 0
        for j, bit in enumerate(byte_bits):
            byte_val |= (bit << (7 - j))
        bytes_data.append(byte_val)
    
    return bytes_data

def main():
    input_file = 'bin/badapplebit.txt'
    output_file = 'bin/badapplebit.bin'
    
    print(f"Reading from {input_file}...")
    frames = parse_rle_to_frames(input_file)
    print(f"Found {len(frames)} frames")
    
    print(f"Converting to binary...")
    binary_data = bytearray()
    
    for i, frame in enumerate(frames):
        frame_bytes = frame_to_binary(frame)
        binary_data.extend(frame_bytes)
        
        if (i + 1) % 100 == 0:
            print(f"  Processed {i + 1} frames...")
    
    print(f"Writing to {output_file}...")
    with open(output_file, 'wb') as f:
        f.write(binary_data)
    
    print(f"Done! Output size: {len(binary_data)} bytes")
    print(f"Frame count: {len(frames)}")
    print(f"Bytes per frame: {len(binary_data) // len(frames) if frames else 0}")

if __name__ == '__main__':
    main()
