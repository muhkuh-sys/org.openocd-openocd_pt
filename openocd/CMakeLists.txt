CMAKE_MINIMUM_REQUIRED(VERSION 3.7)

SET(VERSION_EXTERNAL "0.10.0")
SET(VERSION_PACKAGE "8")

PROJECT("openocd")

SET(PROJECT_VERSION "${VERSION_EXTERNAL}.${VERSION_PACKAGE}")

INCLUDE(ExternalProject)


# Python is required for the patch script.
FIND_PACKAGE(PythonInterp 2.7 REQUIRED)


#----------------------------------------------------------------------------
#
# Build the project.
#
SET(OPENOCD_CMAKE_ARGS "")
LIST(APPEND OPENOCD_CMAKE_ARGS "-DPRJ_DIR=${PRJ_DIR}")
LIST(APPEND OPENOCD_CMAKE_ARGS "-DWORKING_DIR=${WORKING_DIR}")
LIST(APPEND OPENOCD_CMAKE_ARGS "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
LIST(APPEND OPENOCD_CMAKE_ARGS "-DCMAKE_PREFIX_PATH=${CMAKE_CURRENT_BINARY_DIR}/../libusb/install/cmake")
LIST(APPEND OPENOCD_CMAKE_ARGS "-DVERSION_EXTERNAL=${VERSION_EXTERNAL}")

ExternalProject_Add(TARGET_openocd
                    DEPENDS TARGET_libusb
                    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/openocd-${VERSION_EXTERNAL}
                    URL ${CMAKE_CURRENT_SOURCE_DIR}/openocd-${VERSION_EXTERNAL}.tar.bz2
                    URL_HASH SHA1=3245d43d60a1a93f8b0f6b1ce20b53e38cab7a4a
                    PATCH_COMMAND "${PYTHON_EXECUTABLE}" ${CMAKE_HOME_DIRECTORY}/cmake/tools/apply_patches.py --working-folder ${CMAKE_CURRENT_BINARY_DIR}/openocd-${VERSION_EXTERNAL}/src/TARGET_openocd --patch-folder ${CMAKE_CURRENT_SOURCE_DIR}/patches --strip 1 --copy-folder ${CMAKE_CURRENT_SOURCE_DIR}/copy
                    CMAKE_ARGS ${OPENOCD_CMAKE_ARGS}
                    INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install DESTDIR=${CMAKE_CURRENT_BINARY_DIR}
)


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

	INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/install/bin/openocd.exe
	        DESTINATION bin)
	INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/install/bin/openocd.dll
	        DESTINATION bin)

	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	        DESTINATION .)
ELSE(${JONCHKI_PLATFORM_DIST_ID} STREQUAL "windows")
	# Package a linux build.

	CONFIGURE_FILE(installer/install_linux.lua
	               ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	               @ONLY)

	INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/install/bin/openocd
	        DESTINATION bin)

	INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/package/install.lua
	        DESTINATION .)
ENDIF(${JONCHKI_PLATFORM_DIST_ID} STREQUAL "windows")

INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/cmake DESTINATION .)
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/include DESTINATION .)
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/install/lib DESTINATION .)


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
