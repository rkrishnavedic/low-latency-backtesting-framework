#include <iostream>
#include "common.hpp"

int main() {
    std::cout << "Backtesting Framework Initialized. Cache Line Size: " 
              << CACHE_LINE_SIZE << " bytes." << std::endl;
    return 0;
}