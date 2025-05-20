# SPDX-License-Identifier: BSD-2-Clause

# Copyright (c) 2021 NKI/AVL, Netherlands Cancer Institute

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:

# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.15)

# set the project name
project(dssp-server VERSION 1.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CPM)
include(GNUInstallDirs)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers")
endif()

set(CONFIG_PATH "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
set(DSSPD_EXE "${CMAKE_INSTALL_FULL_SBINDIR}/dsspd")
set(DSSPD_PID "/var/run/dsspd")

# Create a revision file, containing the current git version info
include(VersionString)
write_version_header("${PROJECT_SOURCE_DIR}/src")

# Optionally use mrc to create resources
find_package(mrc QUIET)

if(NOT MRC_FOUND)
	message(FATAL_ERROR "Cannot use resources since mrc was not found")
endif()

set(CMAKE_THREAD_PREFER_PTHREAD)
set(THREADS_PREFER_PTHREAD_FLAG)
find_package(Threads)

find_package(nlohmann_json REQUIRED)
find_package(mrc REQUIRED)

CPMAddPackage("gh:HowardHinnant/date@3.0.3")
CPMAddPackage("https://forge.hekkelman.net/maarten/mcfp.git@1.4.2")
CPMAddPackage(NAME zeep
	GIT_REPOSITORY "https://forge.hekkelman.net/maarten/zeep.git"
	GIT_TAG 875eac2
	EXCLUDE_FROM_ALL YES)
CPMAddPackage("https://forge.hekkelman.net/maarten/gxrio.git@1.0.2")
CPMAddPackage(NAME cifpp
	GIT_REPOSITORY https://github.com/pdb-redo/libcifpp.git
	GIT_TAG v8.0.1
	EXCLUDE_FROM_ALL YES)
CPMAddPackage(NAME dssp
	GIT_REPOSITORY https://github.com/pdb-redo/dssp.git
	GIT_TAG v4.5.1
	EXCLUDE_FROM_ALL YES)
CPMAddPackage(NAME libpqxx
	GIT_REPOSITORY https://github.com/jtv/libpqxx.git
	GIT_TAG 7.10.1
	EXCLUDE_FROM_ALL YES)

find_program(YARN yarn REQUIRED)

add_executable(dsspd
	${PROJECT_SOURCE_DIR}/src/databank-service.cpp
	${PROJECT_SOURCE_DIR}/src/db-connection.cpp
	${PROJECT_SOURCE_DIR}/src/dssp-server.cpp)

target_link_libraries(dsspd PRIVATE dssp::dssp zeep::zeep gxrio::gxrio
	libpqxx::pqxx date::date mcfp::mcfp)

# yarn rules for javascripts
set(webpack_input
	${CMAKE_CURRENT_SOURCE_DIR}/webapp/pdb-redo-style.scss
	${CMAKE_CURRENT_SOURCE_DIR}/webapp/index.js
	${CMAKE_CURRENT_SOURCE_DIR}/webpack.config.js
)

set(webpack_output ${CMAKE_CURRENT_SOURCE_DIR}/docroot/scripts/index.js)

add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/node_modules
	COMMAND ${YARN}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM)

add_custom_command(OUTPUT ${webpack_output}
	DEPENDS ${webpack_input} ${CMAKE_CURRENT_SOURCE_DIR}/node_modules
	COMMAND ${YARN} run $<IF:$<CONFIG:Debug>,build,build-production>
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM)

add_custom_target(generate_webpack DEPENDS ${webpack_output})

add_dependencies(dsspd generate_webpack)

file(COPY_FILE
	${dssp_SOURCE_DIR}/libdssp/mmcif_pdbx/dssp-extension.dic
	${CMAKE_CURRENT_BINARY_DIR}/dssp-extension-built-in.dic)

mrc_target_resources(dsspd
	${cifpp_SOURCE_DIR}/rsrc/mmcif_pdbx.dic
	${CMAKE_CURRENT_BINARY_DIR}/dssp-extension-built-in.dic
	${PROJECT_SOURCE_DIR}/docroot/)

# Install rules
install(TARGETS dsspd
	EXPORT dsspTargets
	RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})

if(LINUX)
	configure_file(dsspd.service.in ${CMAKE_CURRENT_BINARY_DIR}/dsspd.service @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dsspd.service" DESTINATION "/etc/systemd/system")
endif()
