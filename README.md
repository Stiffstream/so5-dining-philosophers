This repository contains several implementations of solutions for ["Dining Philosophers" problem](https://en.wikipedia.org/wiki/Dining_philosophers_problem). These solutions are built by using Actor and CSP models on top of [SObjectizer](https://stiffstream.com/en/products/sobjectizer.html) framework.

# How To Obtain And Try?

## Prerequisites

Since Jan 2020 a compiler with support of C++17 is required. A C++14 version can be found under the [tag 20190129](https://github.com/Stiffstream/so5-dining-philosophers/tree/20190129).

## How To Obtain?

This repository contains only source codes of the examples. SObjectizer's source code is not included into the repository.
There are two ways to get the examples and all necessary dependencies.

Since Nov 2025 the project uses [vcpkg](https://vcpkg.io/) for dependency management.

## How To Try?

You need CMake to compile and build the examples.

### If you have no vcpkg installed

```sh
# Step 1: obtaining vcpkg.
git clone --depth=1 https://github.com/microsoft/vcpkg
cd vcpkg
./bootstrap-vcpkg
cd ..

# Step 2: obtaining the project.
git clone --depth=1 https://github.com/Stiffstream/so5-dining-philosophers
cd so5-dining-philosophers

# Step 3: building the project.
mkdir cmake_build
cd cmake_build
cmake -DCMAKE_INSTALL_PREFIX=target \
  -DCMAKE_BUILD_TYPE=release \
  -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --config Release
cd bin
```

The results will be in `bin` subfolder.

### If you have vckg installed

Obtaining the project:

```sh
# Obtaining the project.
git clone --depth=1 https://github.com/Stiffstream/so5-dining-philosophers
cd so5-dining-philosophers
```
Then, if you have `VCPKG_HOME` environment variable (there is no need to specify `CMAKE_TOOLCHAIN_FILE`):

```sh
mkdir cmake_build
cd cmake_build
cmake -DCMAKE_INSTALL_PREFIX=target \
  -DCMAKE_BUILD_TYPE=release ..
cmake --build . --config Release
cd bin
```

But if `VCPKG_HOME` is not specified, then:

```sh
mkdir cmake_build
cd cmake_build
cmake -DCMAKE_INSTALL_PREFIX=target \
  -DCMAKE_BUILD_TYPE=release \
  -DCMAKE_TOOLCHAIN_FILE=VCPKG_LOCATION/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --config Release
cd bin
```
where `VCPKG_LOCATION` is the path to vcpkg directory.
