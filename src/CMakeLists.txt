cmake_minimum_required(VERSION 3.4.3)
project(Bon)

set(BON_VERSION "0.01.0")
add_definitions( -DBON_VERSION="${BON_VERSION}" )

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_FLAGS "-std=c++11 -fPIC -fexceptions -O3 -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -ffunction-sections -fdata-sections -DNDEBUG -g1")
#set(CMAKE_CXX_FLAGS "-std=c++11 -fPIC -fexceptions -DNDEBUG -D_GLIBCXX_DEBUG -O0 -g -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS "-rdynamic -g")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Now build our tools
add_executable(bon bonTokenizer.cc bonParser.cc bonAST.cc bonTypeAnalysisPass.cc bonModuleState.cc bonCodeGenPass.cc bonDebugASTPass.cc bonStdLib.cc bon.cc bonLogger.cc bonTypesystem.cc utils.cc)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support core irreader mcjit native scalaropts vectorize)

# Link against LLVM libraries
target_link_libraries(bon ${llvm_libs})
install(TARGETS bon DESTINATION $ENV{HOME}/.bon/${BON_VERSION}/bin)
install(DIRECTORY ../stdlib DESTINATION $ENV{HOME}/.bon/${BON_VERSION}/)
