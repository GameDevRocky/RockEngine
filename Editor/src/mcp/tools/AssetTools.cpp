#include "mcp/Tools.hpp"

#include "mcp/McpDispatcher.hpp"
#include "mcp/PyApiCall.hpp"

#include "engine/audio/AudioClip.hpp"
#include "engine/rendering/core/AssetManager.hpp"
#include "engine/rendering/core/Font.hpp"
#include "engine/rendering/core/Material.hpp"
#include "engine/rendering/core/Shader.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"

#include <QJsonArray>
#include <QJsonObject>

namespace mcp {

namespace {

// A sliced tileset alone produces hundreds of sprites, so an unfiltered dump is both
// unreadable and expensive to ship over the wire. Every listing is therefore filterable
// and capped, and reports the true total so a caller knows it is seeing a subset.
constexpr int kDefaultLimit = 50;

struct Filter {
    QString substring;   // matched case-insensitively against name and path
    int     limit = kDefaultLimit;
};

// Assets live on AssetManager, a process-global singleton outside any Container -- so
// unlike GameObjects there is no editor/runtime distinction to worry about here.
// Everything in these maps derives from Resource, which is where id/name/path come from.
template <typename T>
QJsonObject DescribeAssets(const std::unordered_map<std::string, T*>& assets, const Filter& filter) {
    QJsonArray items;
    int matched = 0;

    for (const auto& [id, asset] : assets) {
        if (!asset) continue;

        const QString name = QString::fromStdString(asset->GetName());
        const QString path = QString::fromStdString(asset->GetFilePath());
        if (!filter.substring.isEmpty() &&
            !name.contains(filter.substring, Qt::CaseInsensitive) &&
            !path.contains(filter.substring, Qt::CaseInsensitive))
            continue;

        ++matched;
        if (items.size() >= filter.limit) continue;

        QJsonObject entry;
        entry["id"] = QString::fromStdString(id);
        entry["name"] = name;
        entry["path"] = path;
        items.append(entry);
    }

    QJsonObject result;
    result["total"] = matched;
    result["returned"] = items.size();
    result["items"] = items;
    if (matched > items.size())
        result["truncated"] = true;
    return result;
}

} // namespace

void RegisterAssetTools(McpDispatcher& dispatcher) {
    // Ids are what every assignment tool takes, and they are not guessable from a
    // filename, so listing is the necessary first step for anything asset-valued.
    dispatcher.RegisterTool("assets.list", [](const QJsonObject& params) {
        const QString type = params.value("type").toString().toLower();
        AssetManager& assets = AssetManager::Get();

        Filter filter;
        filter.substring = params.value("nameContains").toString();
        if (params.contains("limit"))
            filter.limit = std::max(1, params.value("limit").toInt(kDefaultLimit));

        QJsonObject data;
        if (type.isEmpty() || type == "sprite")   data["sprites"]   = DescribeAssets(assets.GetAllSprites(), filter);
        if (type.isEmpty() || type == "material") data["materials"] = DescribeAssets(assets.GetAllMaterials(), filter);
        if (type.isEmpty() || type == "texture")  data["textures"]  = DescribeAssets(assets.GetAllTextures(), filter);
        if (type.isEmpty() || type == "font")     data["fonts"]     = DescribeAssets(assets.GetAllFonts(), filter);
        if (type.isEmpty() || type == "audio")    data["audioClips"] = DescribeAssets(assets.GetAllAudioClips(), filter);
        if (type == "shader")                     data["shaders"]   = DescribeAssets(assets.GetAllShaders(), filter);

        if (data.isEmpty())
            return McpResult::Error(ObjectNotFound,
                "unknown asset type \"" + type + "\" -- expected sprite, material, texture, "
                "font, audio or shader (omit for everything but shaders)");
        return McpResult::Ok(data);
    });

    // Asset-valued properties: the Python setters want a wrapper object, so these go
    // through SetAssetProperty rather than the plain property helpers.
    dispatcher.RegisterTool("sprite_renderer.get_sprite", [](const QJsonObject& params) {
        return pyapi::GetProperty(pyapi::kSpriteRenderer,
                                  params.value("id").toString().toStdString(), "sprite");
    });
    dispatcher.RegisterTool("sprite_renderer.set_sprite", [](const QJsonObject& params) {
        return pyapi::SetAssetProperty(pyapi::kSpriteRenderer,
                                       params.value("id").toString().toStdString(), "sprite",
                                       pyapi::kSpriteAsset,
                                       params.value("spriteId").toString().toStdString());
    });

    dispatcher.RegisterTool("audio_source.get_clip", [](const QJsonObject& params) {
        return pyapi::GetProperty(pyapi::kAudioSource,
                                  params.value("id").toString().toStdString(), "clip");
    });
    dispatcher.RegisterTool("audio_source.set_clip", [](const QJsonObject& params) {
        return pyapi::SetAssetProperty(pyapi::kAudioSource,
                                       params.value("id").toString().toStdString(), "clip",
                                       pyapi::kAudioClipAsset,
                                       params.value("clipId").toString().toStdString());
    });
}

} // namespace mcp
