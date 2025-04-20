# Minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C++)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(MPIJacobi)

find_package(MPI REQUIRED)

# --- Define Executables ---

# Define the 'generator' executable from generator.cpp
add_executable(jacobi jacobi.cpp)


# --- Link MPI Libraries ---
# Link both executables against the MPI C++ libraries found by find_package(MPI).
# We use target_link_libraries with the imported target MPI::MPI_CXX,
# which is the modern CMake approach. It handles include directories and
# library linking automatically.
target_link_libraries(jacobi PUBLIC MPI::MPI_CXX)

# --- Optional: Installation ---
# If you wanted to install the executables (e.g., with 'make install')
# you could add rules like this:
install(TARGETS jacobi DESTINATION .)
# The 'bin' directory is relative to the CMAKE_INSTALL_PREFIX.