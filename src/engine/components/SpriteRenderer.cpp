#include "engine/components/SpriteRenderer.hpp"

// Ensure registration happens in this compiled TU

YAML::Node SpriteRenderer::Serialize()
{
    YAML::Node node;
    return node;
}

void SpriteRenderer::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
   
}

void SpriteRenderer::PostDeserialize()
{
   
}



