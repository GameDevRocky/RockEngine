#pragma once
#include <QPixmap>
#include <string>

// Generates fixed-size QPixmaps for assets, suitable for grid pickers and
// delegates. Thumbnails are GL-rendered into an FBO from the engine's
// uploaded GPU assets. Picker thumbnails letterbox to kSize; forSpritePreview
// preserves the sprite's aspect ratio within kSize without square padding. Both reflect the asset as the
// renderer sees it. Requires a valid SceneViewGui context; returns a null
// QPixmap if there isn't one yet.
//
// Results are CACHED by asset id. Rendering one is not cheap -- a context
// switch, an FBO round trip, and a glReadPixels that stalls the pipeline -- and
// the callers ask repeatedly: a picker renders every asset it lists on each
// open, and the folder view delegate used to re-render on every single paint. After the first hit these
// are a hash lookup.
//
// The cache invalidates itself from AssetManager's add/remove events and from
// each asset's own change notifications, so callers never have to think about
// staleness. Invalidate() is exposed only for changes that reach an asset
// without going through either (there are none today).
namespace AssetThumbnails {
    constexpr int kSize = 192;

    QPixmap forMaterial(const std::string& id);
    QPixmap forSprite   (const std::string& id);
    QPixmap forTexture  (const std::string& id);

    // Unpadded sprite image bounded by kSize, centered without its scene pivot
    // offset. Used by the Folder view's inline sprite cells and drag previews.
    QPixmap forSpritePreview(const std::string& id);

    // Drop the cached thumbnail for `id`, plus every cached thumbnail drawn
    // through it (a texture's sprites, a material using that texture or shader).
    void Invalidate(const std::string& id);
    void InvalidateAll();
}
