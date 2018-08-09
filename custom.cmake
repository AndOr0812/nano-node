cmake_minimum_required (VERSION 3.9)
option(DISABLE_NATIVE_ARCH "Disable the addition of -march=native" ON)

set (RAIBLOCKS_SECURE_RPC TRUE)

add_compile_definitions(_FORTIFY_SOURCE=2)

include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG(-stdlib=libc++ COMPILER_SUPPORTS_STDLIB_LIBCXX)
if (COMPILER_SUPPORTS_STDLIB_LIBCXX)
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -stdlib=libc++")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

if (APPLE)
	CHECK_CXX_COMPILER_FLAG(-march=core2 COMPILER_SUPPORTS_MARCH_CORE2)
	if (COMPILER_SUPPORTS_MARCH_CORE2)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=core2")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=core2")
	endif ()
else ()
	CHECK_CXX_COMPILER_FLAG(-march=nocona COMPILER_SUPPORTS_MARCH_NOCONA)
	if (COMPILER_SUPPORTS_MARCH_NOCONA)
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=nocona -mno-sse3")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=nocona -mno-sse3")
	endif ()
endif ()

CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden COMPILER_SUPPORTS_VISIBILITY_HIDDEN)
if (COMPILER_SUPPORTS_VISIBILITY_HIDDEN)
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif ()

CHECK_CXX_COMPILER_FLAG(-fstack-protector-all COMPILER_SUPPORTS_STACK_PROTECTOR_ALL)
if (COMPILER_SUPPORTS_STACK_PROTECTOR_ALL)
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all")
endif ()

CHECK_CXX_COMPILER_FLAG(-fPIE COMPILER_SUPPORTS_PIE)
if (COMPILER_SUPPORTS_PIE)
	set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)
endif ()

cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)
set (CMAKE_INTERPROCEDURAL_OPTIMIZATION IPO_SUPPORTED)

if (APPLE)
	set (CMAKE_MACOSX_RPATH TRUE)
	set (CMAKE_OSX_DEPLOYMENT_TARGET 10.10)

	set (MACOSX_BUNDLE_BUNDLE_NAME "rai_node")
	set (MACOSX_BUNDLE_GUI_IDENTIFIER "com.nanowalletcompany.node")
	set (MACOSX_BUNDLE_SHORT_VERSION_STRING "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
	string(TIMESTAMP MACOSX_BUNDLE_BUNDLE_VERSION "+%Y%m%d%H%M%S" UTC)
	set (MACOSX_BUNDLE_COPYRIGHT "Copyright © 2018 Nano Wallet Company LLC. All rights reserved.")
	configure_file(Info.plist.in "CMakeFiles/Info.plist")

	set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-sectcreate,__TEXT,__info_plist,${PROJECT_SOURCE_DIR}/CMakeFiles/Info.plist")
endif ()
