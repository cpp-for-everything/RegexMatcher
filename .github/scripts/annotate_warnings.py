#!/usr/bin/env python3
import re
import sys

def annotate_warnings(log_file):
    with open(log_file, 'r') as file:
        for line in file:
            match = re.search(r'(.+):(\d+):(\d+): warning: (.+)', line)
            if match:
                file_path, line_num, col_num, message = match.groups()
                print(f"::warning file={file_path},line={line_num},col={col_num}::{message}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: annotate_warnings.py <log_file>")
        sys.exit(1)

    log_file = sys.argv[1]
    try:
        annotate_warnings(log_file)
    except:
        print("Failed to read the file")
