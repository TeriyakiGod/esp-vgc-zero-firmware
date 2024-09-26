# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-vgc-zero-firmware)

set(EXTRA_COMPONENT_DIRS components)

fatfs_create_spiflash_image(storage data FLASH_IN_PROJECT)
