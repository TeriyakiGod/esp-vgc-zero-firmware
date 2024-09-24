#!/bin/bash

# Define the partition offset and size
PARTITION_OFFSET=0x110000   # Adjust this if needed
IMAGE_SIZE=1048576          # Size in bytes (1M)

# Path to the genfatfs.py tool (adjust the path to match your ESP-IDF path)
ESP_IDF_PATH=~/esp/esp-idf  # Change this to where your ESP-IDF is installed
GENFATFS_PATH=$ESP_IDF_PATH/components/fatfs/fatfsgen.py

# Create FAT image from the data folder
echo "Creating FAT image from 'data' folder..."
python3 $GENFATFS_PATH --size $IMAGE_SIZE --output data_fat.img ../data/

if [ $? -ne 0 ]; then
    echo "Error creating FAT image."
    exit 1
fi

echo "FAT image created successfully."

# Flash the FAT image to ESP32
echo "Flashing FAT image to ESP32..."
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash $PARTITION_OFFSET data_fat.img

if [ $? -ne 0 ]; then
    echo "Error flashing FAT image to ESP32."
    exit 1
fi

echo "FAT image flashed successfully to offset $PARTITION_OFFSET."

# Cleanup
rm data_fat.img
echo "Cleaned up the temporary FAT image file."
