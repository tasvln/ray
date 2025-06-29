cmake_minimum_required(VERSION 3.14)
project(RAY)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)

# Add source files
file(GLOB_RECURSE SRC_FILES
    src/*.cpp
    src/*.hpp
    include/*.hpp
)

# Include directories
include_directories(include)
include_directories(src)
include_directories(external)

# Create executable
add_executable(RAY ${SRC_FILES})

# Find packages from vcpkg
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
# find_package(tinyobjloader CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)

target_include_directories(RAY PRIVATE
	${Vulkan_INCLUDE_DIRS}
)

# Link the libraries
target_link_libraries(RAY
	PRIVATE
		Vulkan::Vulkan
		glfw
		glm::glm
		# assimp::assimp
        tinyobjloader::tinyobjloader
        volk::volk
)

target_compile_definitions(RAY PRIVATE VOLK_IMPLEMENTATION VK_NO_PROTOTYPES)

add_custom_command(
    TARGET RAY POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:RAY>/shaders
)

# For CUDA integration (optional)
# enable_language(CUDA)
# add_subdirectory(shaders)
