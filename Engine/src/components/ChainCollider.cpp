#include "engine/components/ChainCollider.hpp"
#include "engine/components/RigidBody.hpp"
#include "engine/components/Transform.hpp"
#include "engine/components/SpriteRenderer.hpp"
#include "engine/rendering/core/Sprite.hpp"
#include "engine/rendering/core/Texture2D.hpp"
#include "engine/rendering/core/ImageDecoder.hpp"
#include "engine/utils/EngineUtils.hpp"
#include "engine/utils/IVisitor.hpp"
#include "engine/debug/Console.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>

namespace {
constexpr float kMinEdgeMeters = 0.005f;

float Cross(glm::vec2 a, glm::vec2 b) { return a.x * b.y - a.y * b.x; }

float SignedArea(const std::vector<glm::vec2>& points) {
    float twiceArea = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index)
        twiceArea += Cross(points[index], points[(index + 1) % points.size()]);
    return twiceArea * 0.5f;
}

bool EdgesIntersect(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d) {
    constexpr float epsilon = 1e-5f;
    const float abC = Cross(b - a, c - a), abD = Cross(b - a, d - a);
    const float cdA = Cross(d - c, a - c), cdB = Cross(d - c, b - c);
    if ((abC > epsilon && abD < -epsilon || abC < -epsilon && abD > epsilon) &&
        (cdA > epsilon && cdB < -epsilon || cdA < -epsilon && cdB > epsilon)) return true;
    auto onEdge = [epsilon](glm::vec2 p, glm::vec2 q, glm::vec2 r, float cross) {
        return std::abs(cross) <= epsilon &&
               q.x >= std::min(p.x, r.x) - epsilon && q.x <= std::max(p.x, r.x) + epsilon &&
               q.y >= std::min(p.y, r.y) - epsilon && q.y <= std::max(p.y, r.y) + epsilon;
    };
    return onEdge(a, c, b, abC) || onEdge(a, d, b, abD) ||
           onEdge(c, a, d, cdA) || onEdge(c, b, d, cdB);
}

bool ValidPoints(const std::vector<glm::vec2>& points, bool isLoop, float minEdge) {
    if (points.size() < (isLoop ? 4u : 2u)) return false;
    const std::size_t edgeCount = isLoop ? points.size() : points.size() - 1;
    for (glm::vec2 point : points)
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
    for (std::size_t index = 0; index < edgeCount; ++index)
        if (glm::distance(points[index], points[(index + 1) % points.size()]) <= minEdge) return false;
    const std::size_t turnCount = isLoop ? points.size() : points.size() - 2;
    for (std::size_t index = 0; index < turnCount; ++index) {
        const glm::vec2 first = points[(index + 1) % points.size()] - points[index];
        const glm::vec2 second = points[(index + 2) % points.size()] - points[(index + 1) % points.size()];
        if (std::abs(Cross(first, second)) <= minEdge * minEdge && glm::dot(first, second) < 0.0f)
            return false;
    }
    if (isLoop && std::abs(SignedArea(points)) <= minEdge * minEdge) return false;
    for (std::size_t first = 0; first < edgeCount; ++first)
        for (std::size_t second = first + 1; second < edgeCount; ++second) {
            if (second == first + 1 || (isLoop && first == 0 && second == edgeCount - 1)) continue;
            if (EdgesIntersect(points[first], points[(first + 1) % points.size()],
                               points[second], points[(second + 1) % points.size()])) return false;
        }
    return true;
}

struct PixelEdge { glm::ivec2 from, to; bool used = false; };

std::vector<glm::vec2> TraceOuterLoop(const DecodedImage& image, int x0, int y0,
                                      int width, int height, float threshold) {
    std::vector<unsigned char> opaque(static_cast<std::size_t>(width) * height, 0);
    const int cutoff = static_cast<int>(std::clamp(threshold, 0.0f, 1.0f) * 255.0f);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y0 + y) * image.width + x0 + x) * image.channels;
            const int alpha = image.channels == 4 ? image.pixels[offset + 3] :
                              image.channels == 2 ? image.pixels[offset + 1] : 255;
            opaque[static_cast<std::size_t>(y) * width + x] =
                (cutoff == 0 ? alpha > 0 : alpha >= cutoff) ? 1 : 0;
        }
    auto filled = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < width && y < height &&
               opaque[static_cast<std::size_t>(y) * width + x] != 0;
    };
    auto key = [width](glm::ivec2 point) { return point.y * (width + 1) + point.x; };
    std::vector<PixelEdge> edges;
    std::unordered_map<int, std::vector<std::size_t>> outgoing;
    auto addEdge = [&](glm::ivec2 from, glm::ivec2 to) {
        outgoing[key(from)].push_back(edges.size());
        edges.push_back({from, to});
    };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            if (!filled(x, y)) continue;
            if (!filled(x, y - 1)) addEdge({x, y}, {x + 1, y});
            if (!filled(x + 1, y)) addEdge({x + 1, y}, {x + 1, y + 1});
            if (!filled(x, y + 1)) addEdge({x + 1, y + 1}, {x, y + 1});
            if (!filled(x - 1, y)) addEdge({x, y + 1}, {x, y});
        }

    std::vector<glm::vec2> largestLoop;
    float largestArea = 0.0f;
    for (std::size_t startIndex = 0; startIndex < edges.size(); ++startIndex) {
        if (edges[startIndex].used) continue;
        std::vector<glm::vec2> loop;
        const glm::ivec2 start = edges[startIndex].from;
        std::size_t edgeIndex = startIndex;
        bool closed = false;
        for (std::size_t step = 0; step < edges.size(); ++step) {
            PixelEdge& edge = edges[edgeIndex];
            if (edge.used) break;
            edge.used = true;
            loop.emplace_back(edge.from.x, edge.from.y);
            if (edge.to == start) { closed = true; break; }
            const auto nextIt = outgoing.find(key(edge.to));
            if (nextIt == outgoing.end()) break;
            std::size_t nextIndex = edges.size();
            float bestTurn = -4.0f;
            const glm::ivec2 direction = edge.to - edge.from;
            for (std::size_t candidate : nextIt->second) {
                if (edges[candidate].used) continue;
                const glm::ivec2 nextDirection = edges[candidate].to - edges[candidate].from;
                const float turn = static_cast<float>(direction.x * nextDirection.y - direction.y * nextDirection.x);
                if (turn > bestTurn) { bestTurn = turn; nextIndex = candidate; }
            }
            if (nextIndex == edges.size()) break;
            edgeIndex = nextIndex;
        }
        if (!closed || loop.size() < 4) continue;
        const float area = SignedArea(loop);
        if (area > largestArea) { largestArea = area; largestLoop = std::move(loop); }
    }
    return largestLoop;
}

std::vector<glm::vec2> SimplifyArc(const std::vector<glm::vec2>& arc, float tolerance) {
    if (arc.size() <= 2) return arc;
    std::vector<unsigned char> keep(arc.size(), 0);
    keep.front() = keep.back() = 1;
    std::vector<std::pair<std::size_t, std::size_t>> spans{{0, arc.size() - 1}};
    const float toleranceSquared = tolerance * tolerance;
    while (!spans.empty()) {
        const auto [first, last] = spans.back();
        spans.pop_back();
        if (last <= first + 1) continue;
        const glm::vec2 start = arc[first], end = arc[last], direction = end - start;
        const float lengthSquared = glm::dot(direction, direction);
        float farthestDistance = toleranceSquared;
        std::size_t farthestIndex = last;
        for (std::size_t index = first + 1; index < last; ++index) {
            const float fraction = lengthSquared > 0.0f ?
                std::clamp(glm::dot(arc[index] - start, direction) / lengthSquared, 0.0f, 1.0f) : 0.0f;
            const glm::vec2 offset = arc[index] - (start + fraction * direction);
            const float distanceSquared = glm::dot(offset, offset);
            if (distanceSquared > farthestDistance) {
                farthestDistance = distanceSquared;
                farthestIndex = index;
            }
        }
        if (farthestIndex != last) {
            keep[farthestIndex] = 1;
            spans.emplace_back(first, farthestIndex);
            spans.emplace_back(farthestIndex, last);
        }
    }
    std::vector<glm::vec2> result;
    for (std::size_t index = 0; index < arc.size(); ++index)
        if (keep[index]) result.push_back(arc[index]);
    return result;
}

std::vector<glm::vec2> SimplifyLoop(const std::vector<glm::vec2>& points, float tolerance) {
    if (points.size() < 4) return points;
    // Split the ring at a far-away vertex, then simplify both original arcs.
    // A local, repeatedly-erasing test deletes every 90-degree pixel corner at
    // tolerance 1 and can reduce a detailed sprite to a triangle.
    std::size_t first = 0;
    for (std::size_t index = 1; index < points.size(); ++index)
        if (points[index].x < points[first].x ||
            (points[index].x == points[first].x && points[index].y < points[first].y)) first = index;
    std::size_t opposite = first;
    float farthestDistance = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index) {
        const glm::vec2 difference = points[index] - points[first];
        const float distanceSquared = glm::dot(difference, difference);
        if (distanceSquared > farthestDistance) {
            farthestDistance = distanceSquared;
            opposite = index;
        }
    }
    if (opposite == first) return points;
    auto makeArc = [&](std::size_t begin, std::size_t end) {
        std::vector<glm::vec2> arc;
        for (std::size_t index = begin;; index = (index + 1) % points.size()) {
            arc.push_back(points[index]);
            if (index == end) break;
        }
        return SimplifyArc(arc, tolerance);
    };
    auto firstArc = makeArc(first, opposite);
    auto secondArc = makeArc(opposite, first);
    firstArc.pop_back();
    secondArc.pop_back();
    firstArc.insert(firstArc.end(), secondArc.begin(), secondArc.end());
    if (firstArc.size() == 3) {
        std::size_t longestEdge = 0;
        float longestDistance = 0.0f;
        for (std::size_t index = 0; index < 3; ++index) {
            const glm::vec2 edge = firstArc[(index + 1) % 3] - firstArc[index];
            const float distanceSquared = glm::dot(edge, edge);
            if (distanceSquared > longestDistance) {
                longestDistance = distanceSquared;
                longestEdge = index;
            }
        }
        const glm::vec2 midpoint = (firstArc[longestEdge] + firstArc[(longestEdge + 1) % 3]) * 0.5f;
        firstArc.insert(firstArc.begin() + static_cast<std::ptrdiff_t>(longestEdge + 1), midpoint);
    }
    return firstArc;
}
}

YAML::Node ChainCollider::Serialize() {
    YAML::Node node = Collider::Serialize();
    node["isLoop"] = isLoop;
    node["reverseSide"] = reverseSide;
    node["alphaThreshold"] = alphaThreshold;
    node["points"] = YAML::Node(YAML::NodeType::Sequence);
    for (const glm::vec2& point : points) {
        YAML::Node item;
        item.push_back(point.x);
        item.push_back(point.y);
        item.SetStyle(YAML::EmitterStyle::Flow);
        node["points"].push_back(item);
    }
    return node;
}

void ChainCollider::Deserialize(const YAML::Node& node) {
    if (state >= State::Loaded) return;
    Collider::Deserialize(node);
    isSensor = false; // b2ChainDef cannot create sensor chains.
    isLoop = node["isLoop"].as<bool>(true);
    reverseSide = node["reverseSide"].as<bool>(false);
    alphaThreshold = std::clamp(node["alphaThreshold"].as<float>(0.1f), 0.0f, 1.0f);
    if (const YAML::Node savedPoints = node["points"]; savedPoints && savedPoints.IsSequence()) {
        points.clear();
        for (const YAML::Node& point : savedPoints) {
            if (point.IsSequence() && point.size() == 2)
                points.emplace_back(point[0].as<float>(), point[1].as<float>());
        }
    }
    state = State::Loaded;
}

void ChainCollider::Init() {
    if (state >= State::Initialized) return;
    const bool hadRigidBody = GetComponent<RigidBody>() != nullptr;
    Collider::Init();
    if (!hadRigidBody && rigidBody) rigidBody->SetBodyType(b2_staticBody);
}

void ChainCollider::Awake() {
    if (state >= State::Awakened) return;
    if (GetEnabled()) CreateShape();
    state = State::Awakened;
}

bool ChainCollider::HasValidGeometry() const {
    return ValidPoints(points, isLoop, kMinEdgeMeters * EngineUtils::RenderUtils::PixelsPerMeter);
}

void ChainCollider::CreateShape() {
    if (!rigidBody || !b2Body_IsValid(rigidBody->GetBodyId())) return;
    if (b2Chain_IsValid(chainId)) b2DestroyChain(chainId);
    chainId = b2_nullChainId;
    if (!HasValidGeometry()) return;
    Transform* transform = GetTransform();
    Transform* bodyTransform = rigidBody->GetTransform();
    if (!transform || !bodyTransform) return;

    const glm::mat4 worldMatrix = transform->GetWorldMatrix();
    const glm::vec2 bodyPosition = bodyTransform->GetWorldPosition();
    const float bodyAngle = -glm::radians(bodyTransform->GetWorldRotation());
    const float cosine = std::cos(bodyAngle), sine = std::sin(bodyAngle);
    std::vector<glm::vec2> physicsPoints;
    physicsPoints.reserve(points.size());
    for (glm::vec2 point : points) {
        const glm::vec2 worldPoint = glm::vec2(worldMatrix * glm::vec4(point + center, 0.0f, 1.0f));
        const glm::vec2 offset = worldPoint - bodyPosition;
        physicsPoints.emplace_back((offset.x * cosine - offset.y * sine) / EngineUtils::RenderUtils::PixelsPerMeter,
                                   (offset.x * sine + offset.y * cosine) / EngineUtils::RenderUtils::PixelsPerMeter);
    }
    if (!ValidPoints(physicsPoints, isLoop, kMinEdgeMeters)) return;
    if (reverseSide) std::reverse(physicsPoints.begin(), physicsPoints.end());
    if (!isLoop) {
        // Box2D reserves the first and final edges as noncolliding ghost edges.
        // Extrapolate them so every visible, authored edge collides.
        physicsPoints.insert(physicsPoints.begin(), physicsPoints.front() * 2.0f - physicsPoints[1]);
        physicsPoints.push_back(physicsPoints.back() * 2.0f - physicsPoints[physicsPoints.size() - 2]);
    }
    std::vector<b2Vec2> chainPoints;
    chainPoints.reserve(physicsPoints.size());
    for (glm::vec2 point : physicsPoints) chainPoints.push_back({point.x, point.y});
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = friction;
    material.restitution = bounciness;
    material.rollingResistance = rollingResistance;
    b2ChainDef definition = b2DefaultChainDef();
    definition.points = chainPoints.data();
    definition.count = static_cast<int>(chainPoints.size());
    definition.materials = &material;
    definition.materialCount = 1;
    definition.isLoop = isLoop;
    definition.filter = GetEnabled() ? filter : b2Filter{};
    chainId = b2CreateChain(rigidBody->GetBodyId(), &definition);
    if (b2Chain_IsValid(chainId)) {
        std::vector<b2ShapeId> segments(static_cast<std::size_t>(b2Chain_GetSegmentCount(chainId)));
        b2Chain_GetSegments(chainId, segments.data(), static_cast<int>(segments.size()));
        for (b2ShapeId segment : segments) b2Shape_EnableContactEvents(segment, true);
    }
}

void ChainCollider::SetPoints(std::vector<glm::vec2> value) {
    points = std::move(value);
    CreateShape();
    Notify(POINTS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

bool ChainCollider::SetPoint(std::size_t index, glm::vec2 value) {
    if (index >= points.size()) return false;
    auto edited = points;
    edited[index] = value;
    SetPoints(std::move(edited));
    return true;
}

bool ChainCollider::InsertPoint(std::size_t index, glm::vec2 value) {
    if (index > points.size()) return false;
    auto edited = points;
    edited.insert(edited.begin() + static_cast<std::ptrdiff_t>(index), value);
    SetPoints(std::move(edited));
    return true;
}

bool ChainCollider::RemovePoint(std::size_t index) {
    if (index >= points.size()) return false;
    auto edited = points;
    edited.erase(edited.begin() + static_cast<std::ptrdiff_t>(index));
    SetPoints(std::move(edited));
    return true;
}

void ChainCollider::AppendPoint(glm::vec2 value) { InsertPoint(points.size(), value); }
void ChainCollider::PrependPoint(glm::vec2 value) { InsertPoint(0, value); }

void ChainCollider::SetIsLoop(bool value) {
    if (isLoop == value) return;
    isLoop = value;
    CreateShape();
    Notify(LOOP_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ChainCollider::SetReverseSide(bool value) {
    if (reverseSide == value) return;
    reverseSide = value;
    CreateShape();
    Notify(SIDE_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ChainCollider::SetAlphaThreshold(float value) {
    value = std::clamp(value, 0.0f, 1.0f);
    if (alphaThreshold == value) return;
    alphaThreshold = value;
    Notify(ALPHA_THRESHOLD_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

bool ChainCollider::GenerateFromSprite() { return GenerateFromSprite(alphaThreshold); }

bool ChainCollider::GenerateFromSprite(float threshold) {
    auto* renderer = GetComponent<SpriteRenderer>();
    Sprite* sprite = renderer ? renderer->GetSprite() : nullptr;
    Texture2D* texture = sprite ? sprite->GetTexture() : nullptr;
    if (!texture) return false;
    const DecodedImage image = ImageDecoder::Decode(texture->GetPath());
    if (!image.ok || image.width <= 0 || image.height <= 0 || image.channels <= 0) return false;
    const glm::vec2 uvMin = sprite->GetUVMin(), uvMax = sprite->GetUVMax();
    const int x0 = std::clamp(static_cast<int>(std::lround(uvMin.x * image.width)), 0, image.width);
    const int x1 = std::clamp(static_cast<int>(std::lround(uvMax.x * image.width)), 0, image.width);
    const int y0 = std::clamp(static_cast<int>(std::lround(uvMin.y * image.height)), 0, image.height);
    const int y1 = std::clamp(static_cast<int>(std::lround(uvMax.y * image.height)), 0, image.height);
    const int width = x1 - x0, height = y1 - y0;
    if (width <= 0 || height <= 0) return false;
    std::vector<glm::vec2> contour = TraceOuterLoop(image, x0, y0, width, height, threshold);
    if (contour.size() < 4) return false;
    for (glm::vec2& point : contour) {
        if (renderer->GetFlipX()) point.x = static_cast<float>(width) - point.x;
        if (renderer->GetFlipY()) point.y = static_cast<float>(height) - point.y;
    }
    if (renderer->GetFlipX() != renderer->GetFlipY())
        std::reverse(contour.begin(), contour.end());
    const glm::vec2 pixelSize(width, height);
    const glm::vec2 quadOffset = pixelSize * (glm::vec2(-0.5f) - sprite->GetPivot()) - center;
    for (glm::vec2& point : contour) point += quadOffset;
    for (float tolerance : {1.0f, 0.5f, 0.0f}) {
        auto simplified = SimplifyLoop(contour, tolerance);
        if (ValidPoints(simplified, true, kMinEdgeMeters * EngineUtils::RenderUtils::PixelsPerMeter)) {
            isLoop = true;
            reverseSide = false;
            SetPoints(std::move(simplified));
            Notify(LOOP_CHANGED_EVENT);
            Notify(SIDE_CHANGED_EVENT);
            return true;
        }
    }
    return false;
}

void ChainCollider::OnEnabled() {
    if (!b2Chain_IsValid(chainId)) CreateShape();
    if (b2Chain_IsValid(chainId)) {
        std::vector<b2ShapeId> segments(static_cast<std::size_t>(b2Chain_GetSegmentCount(chainId)));
        b2Chain_GetSegments(chainId, segments.data(), static_cast<int>(segments.size()));
        for (b2ShapeId segment : segments) b2Shape_SetFilter(segment, filter);
    }
    Component::OnEnabled();
}

void ChainCollider::OnDisabled() {
    if (b2Chain_IsValid(chainId)) {
        std::vector<b2ShapeId> segments(static_cast<std::size_t>(b2Chain_GetSegmentCount(chainId)));
        b2Chain_GetSegments(chainId, segments.data(), static_cast<int>(segments.size()));
        for (b2ShapeId segment : segments) b2Shape_SetFilter(segment, b2Filter{});
    }
    Component::OnDisabled();
}

void ChainCollider::SetFriction(float value) {
    if (friction == value) return;
    friction = value;
    CreateShape();
    Notify(FRICTION_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ChainCollider::SetBounciness(float value) {
    if (bounciness == value) return;
    bounciness = value;
    CreateShape();
    Notify(BOUNCINESS_CHANGED_EVENT);
    Notify(CHANGED_EVENT);
}

void ChainCollider::SetIsSensor(bool value) {
    if (value) Console::Alert("Box2D chain colliders do not support sensor shapes.");
}

void ChainCollider::Shutdown() {
    if (b2Chain_IsValid(chainId)) b2DestroyChain(chainId);
    chainId = b2_nullChainId;
    Collider::Shutdown();
}

ChainCollider* ChainCollider::Copy() {
    auto* copy = new ChainCollider();
    copy->id = id;
    copy->enabled = enabled;
    copy->gameobject_id = gameobject_id;
    copy->center = center;
    copy->filter = filter;
    copy->isSensor = isSensor;
    copy->density = density;
    copy->friction = friction;
    copy->bounciness = bounciness;
    copy->rollingResistance = rollingResistance;
    copy->points = points;
    copy->isLoop = isLoop;
    copy->reverseSide = reverseSide;
    copy->alphaThreshold = alphaThreshold;
    copy->state = State::Loaded;
    return copy;
}

void ChainCollider::Accept(IVisitor* visitor) {
    visitor->Visit(this);
}
