# Building

Compile with [C++ Crow library](https://crowcpp.org/master/getting_started/setup/).

In short, you need to:

1. Include the `asio` development header file.
2. Include the C++ Crow header file.
3. Link the `pthread` library.

### Example

If you have the `crow_all.h` file in the `./include` directory, you can compile your code with:

```bash
g++ -std=c++20 ./src/*.cpp -I ./include/ -I <path to asio header file> -I /usr/local/include -lpthread
