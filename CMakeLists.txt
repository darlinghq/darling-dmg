project(darling-hfsplus)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ggdb")

include(FindLibxml2)

find_package(Libxml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

add_executable(darling-hfsplus
	#src/main-test-hfs+.cpp
	src/main-test-dmg.cpp
	
	src/unichar.cpp
	src/FileReader.cpp
	src/HFSVolume.cpp
	src/AppleDisk.cpp
	src/SubReader.cpp
	src/HFSBTree.cpp
	src/HFSFork.cpp
	src/HFSCatalogBTree.cpp
	src/HFSExtentsOverflowBTree.cpp

	src/DMGDisk.cpp
	src/DMGPartition.cpp
)

target_link_libraries(darling-hfsplus -licuuc -lssl ${LIBXML2_LIBRARY})
install(TARGETS darling-hfsplus RUNTIME DESTINATION bin)

