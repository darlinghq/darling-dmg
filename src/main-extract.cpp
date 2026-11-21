#include "DMGExtractor.h"
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: darling-dmg-extract <image> <empty-directory>\n";
        return 2;
    }
    try {
        extractDMG(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::cerr << "Extraction failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
