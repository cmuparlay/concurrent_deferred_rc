cd build
cmake -DBUILD_ASAN_TESTS=On -DCMAKE_BUILD_TYPE=Debug .
cmake --build .
ctest
cd -
