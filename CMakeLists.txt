cmake_minimum_required(VERSION 3.13)

option(FATFS_BUILD_TEST "Build test application for NDS" OFF)

if(FATFS_BUILD_TEST)
	set(DKP_NDS_PLATFORM_LIBRARY calico)
endif()

project(FatFs LANGUAGES C)

if(FATFS_BUILD_TEST AND NOT NINTENDO_DS)
	message(FATAL_ERROR "Test application must be built with dkP CMake for Nintendo DS")
endif()

add_library(${PROJECT_NAME} OBJECT
	source/ff.c
	source/ffunicode.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
	source
)

if(FATFS_BUILD_TEST)
	add_subdirectory(test)
endif()
