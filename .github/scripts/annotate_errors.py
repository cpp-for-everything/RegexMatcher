#!/usr/bin/env python3
import re
import sys

def annotate_errors(log_file):
    with open(log_file, 'r') as file:
        for line in file:
            # Match error patterns. For GCC/Clang: "error:"; for MSVC: "error Cxxxx:"
            match = re.search(r'(.+):(\d+):(\d+): (error: .+)|(.+)\((\d+)\) ?: (error C[0-9]+: .+)', line)
            if match:
                if match.group(1):
                    # GCC/Clang style error
                    file_path, line_num, col_num, message = match.group(1), match.group(2), match.group(3), match.group(4)
                    print(f"::error file={file_path},line={line_num},col={col_num}::{message}")
                elif match.group(5):
                    # MSVC style error
                    file_path, line_num, message = match.group(5), match.group(6), match.group(7)
                    print(f"::error file={file_path},line={line_num}::{message}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: annotate_errors.py <log_file>")
        sys.exit(1)

    log_file = sys.argv[1]
    try:
        annotate_errors(log_file)
    except:
        print("Failed to read the file")
