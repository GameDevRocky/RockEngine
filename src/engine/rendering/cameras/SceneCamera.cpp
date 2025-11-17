#include "engine/rendering/cameras/SceneCamera.hpp"
#include <glm/gtc/matrix_transform.hpp>

// Editor-controlled camera


void SceneCamera::Init(){




}


void SceneCamera::Update()
{
    bool changed = false;
    position.x -= 0.2f;

    float scroll = 0.0f;
    if (scroll != 0.0f) {
        zoom -= scroll * zoomSpeed;
        zoom = glm::max(zoom, 0.1f);
        projDirty = true;
    }

    if (changed)
        viewDirty = true;
}
