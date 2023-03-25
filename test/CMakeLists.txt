cmake_minimum_required(VERSION 3.13)

add_executable(fatfs_test)
nds_create_rom(fatfs_test)

dkp_target_generate_symbol_list(fatfs_test)

target_compile_options(fatfs_test PRIVATE
	-Wall
)

target_sources(fatfs_test PRIVATE
	arm9_main.c
	console.c
)

dkp_add_embedded_binary_library(fatfs_test_data
	font.bin
)

target_link_libraries(fatfs_test PRIVATE FatFs fatfs_test_data)
