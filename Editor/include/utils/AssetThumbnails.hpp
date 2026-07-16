#pragma once
#include <QPixmap>
#include <string>

// Generates fixed-size QPixmaps for assets, suitable for grid pickers and
// delegates. All thumbnails are GL-rendered into an FBO from the engine's
// uploaded GPU assets, so they letterbox to kSize and reflect the asset as the
// renderer sees it. Requires a valid SceneViewGui context; returns a null
// QPixmap if there isn't one yet.
namespace AssetThumbnails {
    constexpr int kSize = 192;

    QPixmap forMaterial(const std::string& id);
    QPixmap forSprite   (const std::string& id);
    QPixmap forTexture  (const std::string& id);
}
