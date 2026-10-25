#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QObject>

#include "engine/core/Observable.hpp"
#include "engine/utils/Properties.hpp"

// Owns the short-lived engine subscriptions needed by an expanded object/asset
// reference picker. The owner starts it on expand and stops it on collapse, so
// an inspector full of collapsed reference fields carries no background work.
//
// No Q_OBJECT is needed: engine Observables invoke ordinary std::functions and
// the Qt event loop is used only to coalesce a burst (for example, deleting an
// object with several components) into one picker rebuild.
class ObjectRefPickerObserver final : public QObject {
public:
    ObjectRefPickerObserver(const Properties::PropDesc& desc,
                            std::function<void()> refresh,
                            QObject* parent = nullptr);
    ~ObjectRefPickerObserver() override;

    void Start();
    void Stop();

private:
    struct Subscription {
        Observable* source = nullptr;
        int id = 0;
        bool dynamic = false;
        std::string key;
    };

    void BindRoots();
    void RebindDynamic();
    void ObserveAssets();
    void ObserveObjects();
    void QueueRefresh(bool rebind);
    void MarkSourceGone(Observable* source);
    void MarkAssetGone(const std::string& id);
    void ClearDynamic();
    void ClearAll();
    void Subscribe(Observable* source, Observable::Event event,
                   Observable::payload_function callback,
                   bool dynamic = false, std::string key = {});

    Properties::PropDesc m_desc;
    std::function<void()> m_refresh;
    std::vector<Subscription> m_subscriptions;
    bool m_active = false;
    bool m_refreshQueued = false;
    bool m_rebindQueued = false;
};
