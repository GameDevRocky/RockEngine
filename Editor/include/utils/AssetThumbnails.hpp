#pragma once
#include <QPixmap>
#include <string>

// Generates fixed-size QPixmaps for assets, suitable for grid pickers and
// delegates. Material and Sprite thumbnails are GL-rendered into an FBO;
// Texture thumbnails load the source image directly from disk.
namespace AssetThumbnails {
    constexpr int kSize = 192;

    QPixmap forMaterial(const std::string& id);
    QPixmap forSprite   (const std::string& id);
    QPixmap forTexture  (const std::string& id);
}
