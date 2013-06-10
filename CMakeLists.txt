project(darling-hfsplus)

cmake_minimum_required(VERSION 2.8)

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ggdb")

add_executable(darling-hfsplus
	src/main.cpp
	src/unichar.cpp
	src/FileReader.cpp
	src/HFSVolume.cpp
	src/AppleDisk.cpp
	src/SubReader.cpp
	src/HFSBTree.cpp
	src/HFSFork.cpp
	src/HFSCatalogBTree.cpp
	src/HFSExtentsOverflowBTree.cpp
)

target_link_libraries(darling-hfsplus -licuuc)
install(TARGETS darling-hfsplus RUNTIME DESTINATION bin)

