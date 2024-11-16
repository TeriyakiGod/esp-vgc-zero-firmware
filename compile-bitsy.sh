#!/bin/bash

# Set directories
UTILS_DIR="utils/bitsy-engine"
OUTPUT_DIR="data/bitsy/engine"
DUKTAPE_BIN="tmp/bin/duk"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Compile each .js file in the utils/bitsy-engine directory
for js_file in "$UTILS_DIR"/*.js; 
do
    # Get the base name of the file
    base_name=$(basename "$js_file" .js)
    
    # Define the output file path
    output_file="$OUTPUT_DIR/$base_name.bin"
    
    # Compile the .js file to .bin using duktape
    "$DUKTAPE_BIN" -c "$output_file" "$js_file"
    
    echo "Compiled $js_file to $output_file"
done

echo "Compilation process completed."