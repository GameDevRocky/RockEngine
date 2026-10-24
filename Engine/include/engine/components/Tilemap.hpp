#include <iostream>
#include "engine/components/Component.hpp"
#include "unordered_map"
#include "array"

static constexpr uint8_t CHUNK_SIZE = 32;

struct Tile {
    std::string sprite_id;
    uint16_t tile_id;

};


struct Chunk{
    std::array<Tile, CHUNK_SIZE * CHUNK_SIZE> tiles; 
    glm::ivec2 chunk_id;

};


class Tilemap : Component{
    
    private:
        std::unordered_map<glm::ivec2, Chunk> data; 

};