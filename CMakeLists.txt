project(darling-dmg)

cmake_minimum_required(VERSION 3.13)

set(dmg_sources
	src/unichar.cpp
	src/Reader.cpp
	src/FileReader.cpp
	src/HFSVolume.cpp
	src/AppleDisk.cpp
	src/SubReader.cpp
	src/HFSBTree.cpp
	src/HFSFork.cpp
	src/HFSCatalogBTree.cpp
	src/HFSExtentsOverflowBTree.cpp
	src/HFSAttributeBTree.cpp

	src/DMGDisk.cpp
	src/DMGPartition.cpp
	src/DMGDecompressor.cpp
	src/adc.cpp
	src/HFSZlibReader.cpp
	src/MemoryReader.cpp

	src/GPTDisk.cpp
	
	src/MacBinary.cpp
	src/ResourceFork.cpp
	src/CacheZone.cpp
	src/CachedReader.cpp

	src/HFSHighLevelVolume.cpp
)


# This part of CMakeLists.txt is for when building darling-dmg
# as a standalone FUSE module outside of Darling.
if (NOT DARLING)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

add_definitions(-D_FILE_OFFSET_BITS=64)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -ggdb -O0")

include(FindLibXml2)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})

if (WITH_TESTS)
	enable_testing()
	find_package(Boost COMPONENTS unit_test_framework REQUIRED)

	add_definitions(-DBOOST_TEST_DYN_LINK ) # -DDEBUG

	set(CacheTest_SRC
		test/CacheTest.cpp
		src/CacheZone.cpp
		src/CachedReader.cpp
		src/Reader.cpp
		src/MemoryReader.cpp
	)

	add_executable(CacheTest ${CacheTest_SRC})
	target_link_libraries(CacheTest ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
	add_test(NAME CacheTest COMMAND CacheTest)
endif (WITH_TESTS)

add_library(dmg SHARED
	src/unichar.cpp
	src/Reader.cpp
	src/FileReader.cpp
	src/HFSVolume.cpp
	src/AppleDisk.cpp
	src/SubReader.cpp
	src/HFSBTree.cpp
	src/HFSFork.cpp
	src/HFSCatalogBTree.cpp
	src/HFSExtentsOverflowBTree.cpp
	src/HFSAttributeBTree.cpp

	src/DMGDisk.cpp
	src/DMGPartition.cpp
	src/DMGDecompressor.cpp
	src/adc.cpp
	src/HFSZlibReader.cpp
	src/MemoryReader.cpp

	src/GPTDisk.cpp
	
	src/MacBinary.cpp
	src/ResourceFork.cpp
	src/CacheZone.cpp
	src/CachedReader.cpp

	src/HFSHighLevelVolume.cpp
)
target_link_libraries(dmg -licuuc -lcrypto -lz -lbz2 ${LIBXML2_LIBRARY})
install(TARGETS dmg DESTINATION lib)

add_executable(darling-dmg
	src/main-fuse.cpp
)

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
SET(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

target_link_libraries(darling-dmg -lfuse dmg)
install(TARGETS darling-dmg RUNTIME DESTINATION bin)

# This is used in Darling build.
else (NOT DARLING)
	include(wrap_elf)
	include(darling_exe)
	include(FindPkgConfig)

	add_definitions(-D_FILE_OFFSET_BITS=64 -DCOMPILE_WITH_LZFSE=1)

	pkg_check_modules(FUSE REQUIRED fuse)

	include_directories(
		${FUSE_INCLUDE_DIRS}
	)

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -nostdinc")

	wrap_elf(fuse libfuse.so)

	add_darling_executable(hdiutil
		${dmg_sources}
		src/main-hdiutil.cpp
	)

	target_link_libraries(hdiutil fuse icucore z bz2 crypto44 xml2 iconv lzfse)
	install(TARGETS hdiutil DESTINATION libexec/darling/usr/bin)

endif (NOT DARLING)
