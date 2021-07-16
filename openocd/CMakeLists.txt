CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

SET(VERSION_EXTERNAL "0.11.0")
SET(VERSION_PACKAGE "1")

PROJECT("openocd")

SET(PROJECT_VERSION "${VERSION_EXTERNAL}.${VERSION_PACKAGE}")

INCLUDE(ExternalProject)


# Python is required for the patch script.
FIND_PACKAGE(PythonInterp 2.7 REQUIRED)


#----------------------------------------------------------------------------

SET(LIBUSB_INSTALL_PATH "${CMAKE_BINARY_DIR}/external/libusb/install")
SET(OPENOCD_LIBRARY "${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd-build/src/.libs/libopenocd.a")
SET(JIMTCL_LIBRARY "${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd-build/jimtcl/libjim.a")

IF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        SET(LIBOPENOCD_LDFLAGS -lws2_32)
        SET(LIBUSB_LIBRARIES ${LIBUSB_INSTALL_PATH}/lib/libusb-1.0.a)
ELSE(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        SET(LIBOPENOCD_LDFLAGS "")
        SET(LIBUSB_LIBRARIES ${LIBUSB_INSTALL_PATH}/lib/libusb-1.0.a -ludev -pthread)
ENDIF(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")

ADD_LIBRARY(LIBRARY_libopenocd SHARED
            muhkuh_openocd.c)
TARGET_INCLUDE_DIRECTORIES(LIBRARY_libopenocd
                           PRIVATE ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd/src
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd/src/helper
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd/src/jtag/drivers
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd/src/jtag/drivers/libjaylink
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd/jimtcl
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd-build
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd-build/jimtcl
                                   ${CMAKE_BINARY_DIR}/external/openocd/openocd/src/TARGET_openocd-build/src)

TARGET_LINK_LIBRARIES(LIBRARY_libopenocd
                      PRIVATE ${OPENOCD_LIBRARY} ${JIMTCL_LIBRARY} ${LIBUSB_LIBRARIES} "${LIBOPENOCD_LDFLAGS}")
TARGET_COMPILE_DEFINITIONS(LIBRARY_libopenocd
                           PRIVATE HAVE_CONFIG_H
                                   RELSTR=""
                                   GITVERSION="")
# Set the name of the output file to "openocd".
SET_TARGET_PROPERTIES(LIBRARY_libopenocd PROPERTIES
                      PREFIX ""
                      OUTPUT_NAME "openocd")


#----------------------------------------------------------------------------
#
# Build a package for the selected platform.
#
SET(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_BINARY_DIR}/../repository/org/openocd/openocd_pt/${PROJECT_VERSION})
SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY FALSE)
SET(CPACK_PACKAGE_FILE_NAME "openocd-${PROJECT_VERSION}-${JONCHKI_PLATFORM}")

SET(CPACK_GENERATOR "TXZ")
INCLUDE(CPack)

IF(${JONCHKI_PLATFORM_DIST_ID} STREQUAL "windows")
	# Package a windows build.

	CONFIGURE_FILE(installer/install_windows.lua
	               ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	               @ONLY)

	INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/external/openocd/install/bin/openocd.exe
	        DESTINATION bin)
	INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/openocd.dll
	        DESTINATION bin)

	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	        DESTINATION .)
ELSE(${JONCHKI_PLATFORM_DIST_ID} STREQUAL "windows")
	# Package a linux build.

	CONFIGURE_FILE(installer/install_linux.lua
	               ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	               @ONLY)

	INSTALL(PROGRAMS ${CMAKE_BINARY_DIR}/external/openocd/install/bin/openocd
	        DESTINATION bin)
	INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/openocd.so
	        DESTINATION bin)

	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	        DESTINATION .)
ENDIF(${JONCHKI_PLATFORM_DIST_ID} STREQUAL "windows")

INSTALL(FILES muhkuh_openocd.h DESTINATION include)


CONFIGURE_FILE(installer/openocd.xml
               ${CPACK_OUTPUT_FILE_PREFIX}/openocd-${PROJECT_VERSION}-${JONCHKI_PLATFORM}.xml
               @ONLY)


#-----------------------------------------------------------------------------
#
# Create a custom target named "pack".
# It builds the package and creates the hashes.
#
ADD_CUSTOM_TARGET(pack
                  COMMAND ${CMAKE_MAKE_PROGRAM}
                  COMMAND ${CMAKE_MAKE_PROGRAM} package
                  COMMAND "${PYTHON_EXECUTABLE}" ${CMAKE_HOME_DIRECTORY}/cmake/tools/generate_hash.py ${CPACK_OUTPUT_FILE_PREFIX}/openocd-${PROJECT_VERSION}-${JONCHKI_PLATFORM}.xml
                  COMMAND "${PYTHON_EXECUTABLE}" ${CMAKE_HOME_DIRECTORY}/cmake/tools/generate_hash.py ${CPACK_OUTPUT_FILE_PREFIX}/openocd-${PROJECT_VERSION}-${JONCHKI_PLATFORM}.tar.xz
                  COMMENT "Pack the artifact and generate the hashes...")


#-----------------------------------------------------------------------------
#
# Filter the pom.xml file.
#
SET(POM_ARTIFACT_ID  "openocd")
SET(POM_GROUP_ID     "org.openocd")
SET(POM_VERSION      "${PROJECT_VERSION}")
SET(POM_PACKAGING    "tar.xz")
CONFIGURE_FILE(installer/pom.xml
               ${CPACK_OUTPUT_FILE_PREFIX}/openocd-${PROJECT_VERSION}.pom
               @ONLY)
