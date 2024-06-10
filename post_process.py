#!/usr/bin/python3

import re
import sys


def insert_empty_line_after_open_brace_sequence(file_path):
    with open(file_path, "r") as file:
        lines = file.readlines()

    pattern = re.compile(r"\s*\{\n")

    with open(file_path, "w") as file:
        i = 0
        while i < len(lines):
            file.write(lines[i])
            if pattern.search(lines[i]):
                # Ensure next line is not empty before adding an extra newline
                if i + 1 < len(lines) and lines[i + 1].strip() != "":
                    file.write("\n")
            i += 1


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python post_process.py <file_path>")
        sys.exit(1)

    file_path = sys.argv[1]
    insert_empty_line_after_open_brace_sequence(file_path)
