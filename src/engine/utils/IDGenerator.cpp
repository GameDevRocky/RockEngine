#include <cstdint>
#include "engine/utils/IDGenerator.hpp"

uint64_t IDGenerator::Generate() {
    static uint64_t currentID = 0;
    return ++currentID;
}


