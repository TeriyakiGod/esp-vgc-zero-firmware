cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-vgc-zero-firmware)

fatfs_create_spiflash_image(storage data FLASH_IN_PROJECT)
