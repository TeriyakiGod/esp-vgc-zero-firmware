#!/bin/bash

# Define source and destination directories
SOURCE1_DIR="scripts/bitsy/engine"
DEST1_DIR="data/bitsy/engine"

SOURCE2_DIR="scripts/bitsy/system"
DEST2_DIR="data/bitsy/system"

# Create destination directory if it doesn't exist
mkdir -p "$DEST1_DIR"
mkdir -p "$DEST2_DIR"

# Compile each JavaScript file in the source directory
for file in "$SOURCE1_DIR"/*.js; 
do
    # Get the base name of the file and replace .js with .bin
    base_name=$(basename "$file" .js).bin
    
    # Compile the file using duk and save it to the destination directory
    tmp/bin/duk -c $DEST1_DIR/$base_name $file
done

# Compile each JavaScript file in the source directory
for file in "$SOURCE2_DIR"/*.js; 
do
    # Get the base name of the file and replace .js with .bin
    base_name=$(basename "$file" .js).bin
    
    # Compile the file using duk and save it to the destination directory
    tmp/bin/duk -c $DEST2_DIR/$base_name $file
done

echo "All files compiled successfully."