#!/bin/bash

# Check if at least one object file is provided
if [ "$#" -eq 0 ]; then
    echo "Usage: $0 file1.o file2.o ... fileN.o"
    exit 1
fi

# Loop through each provided object file
for obj_file in "$@"; do
    if [ -f "$obj_file" ]; then
        echo "Symbols in $obj_file:"
        arm-none-eabi-nm "$obj_file" | awk '{print $3}' # Extract the symbol name (3rd column)
        echo ""
    else
        echo "Error: $obj_file not found!"
    fi
done
