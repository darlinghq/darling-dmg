project(darling-dmg)

cmake_minimum_required(VERSION 2.8)

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

include(FindLibxml2)

find_package(Libxml2 REQUIRED)
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

if (COMPILE_WITH_DECRYPTION_SUPPORT)
	set(Decrypt_SRC
		src/decrypt/Aes.cpp
		src/decrypt/Crypto.cpp
		src/decrypt/Des.cpp
		src/decrypt/EncryptedReader.cpp
		src/decrypt/Sha1.cpp
		src/decrypt/Sha256.cpp
		src/decrypt/TripleDes.cpp
	)
endif (COMPILE_WITH_DECRYPTION_SUPPORT)

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

	add_definitions(-D_FILE_OFFSET_BITS=64)

	pkg_check_modules(FUSE REQUIRED fuse)

	include_directories(BEFORE ${CMAKE_SOURCE_DIR}/src/external/libcxx/include)
	include_directories(
		${FUSE_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/src/libiconv/include
		${CMAKE_SOURCE_DIR}/src/libc/include/FreeBSD
		${CMAKE_SOURCE_DIR}/src/external/icu/icuSources/common
		${CMAKE_SOURCE_DIR}/src/external/libxml2/include
		${CMAKE_BINARY_DIR}/src/external/libxml2/include
		${CMAKE_SOURCE_DIR}/src/external/openssl/src/include
		${CMAKE_SOURCE_DIR}/src/external/zlib
		${CMAKE_SOURCE_DIR}/src/external/bzip2
	)

	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -nostdinc")

	wrap_elf(fuse libfuse.so)

	add_darling_executable(hdiutil
		${dmg_sources}
		src/main-hdiutil.cpp
	)

	target_link_libraries(hdiutil fuse icucore z bz2 crypto098 xml2 iconv)
	install(TARGETS hdiutil DESTINATION libexec/darling/usr/bin)

endif (NOT DARLING)
