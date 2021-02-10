cmake_minimum_required(VERSION 3.7)

#----------------------------------------------------------------------------
#
# Build libusb and create a CMake component.
#
PROJECT("libusb")

SET(VERSION_EXTERNAL "1.0.24")
SET(VERSION_EXTERNAL_POST "")

INCLUDE(ExternalProject)

#----------------------------------------------------------------------------
#
# Construct the host ID for the configure command.
#
IF(${CMAKE_CROSSCOMPILING})
	IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# Get the prefix of the compiler.
		GET_FILENAME_COMPONENT(GCC_BASENAME ${CMAKE_C_COMPILER} NAME)
		IF(GCC_BASENAME MATCHES "([^-]+-[^-]+-[^-]+)-gcc")
			SET(CONFIGURE_HOST "${CMAKE_MATCH_1}")
		ELSE(GCC_BASENAME MATCHES "([^-]+-[^-]+-[^-]+)-gcc")
			MESSAGE(FATAL_ERROR "Failed to extract the compiler prefix from the C compiler ${CMAKE_C_COMPILER}")
		ENDIF(GCC_BASENAME MATCHES "([^-]+-[^-]+-[^-]+)-gcc")
	ELSE(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		MESSAGE(FATAL_ERROR "Cross compiling detected, but not using GCC. This is currently not supported.")
	ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
ELSE(${CMAKE_CROSSCOMPILING})
	# Set the host even if not cross compiling.
	# Otherwise GNUTLS detects the CPU and tries to build with special
	# support for it. In the first place this sounds very nice. But it
	# fails on a 32bit OS running on a 64bit CPU - which is the case for
	# Docker 32bit images.
	IF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			SET(CONFIGURE_HOST "${JONCHKI_PLATFORM_CPU_ARCH}-pc-linux-gnu")
		ELSE(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			MESSAGE(FATAL_ERROR "Trying to detect the system with a non-GCC compiler. This is currently not supported.")
		ENDIF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	ELSE(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		MESSAGE(FATAL_ERROR "Trying to detect the system on a non-linux OS. This is currently not supported.")
	ENDIF(CMAKE_SYSTEM_NAME STREQUAL "Linux")
ENDIF(${CMAKE_CROSSCOMPILING})



#----------------------------------------------------------------------------
#
# Build the project.
#
ExternalProject_Add(TARGET_libusb
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}.tar.bz2
                    URL_HASH SHA1=d8d614b538f7c953b6e3b73f1eea5dc70820a7e2
                    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CFLAGS=${CMAKE_C_FLAGS}" "CPPFLAGS=${CMAKE_CXX_FLAGS}" ${CMAKE_CURRENT_BINARY_DIR}/libusb-${VERSION_EXTERNAL}${VERSION_EXTERNAL_POST}/src/TARGET_libusb/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/install --enable-static=yes --enable-shared=no --with-pic --host=${CONFIGURE_HOST}
                    BUILD_COMMAND make
                    INSTALL_COMMAND make install
)
