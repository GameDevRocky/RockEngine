#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/SharedResources.hpp"
// Ensure  happens in this compiled TU

YAML::Node SpriteRenderer::Serialize()
{
    YAML::Node node;
    return node;
}

void SpriteRenderer::Deserialize(const YAML::Node& node)
{
    Component::Deserialize(node);
    material = SharedResources::Get().GetMaterial("default");
}

void SpriteRenderer::PostDeserialize()
{
   
}



