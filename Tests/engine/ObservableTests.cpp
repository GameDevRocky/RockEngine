// Observable is the decoupling backbone: Serializable extends it, so every RuntimeObject,
// System and Component in the engine is an event source, and the editor is wired to the
// engine almost entirely through it. Its semantics have sharp edges that are documented in
// Engine/CLAUDE.md but enforced nowhere -- notably the auto-unsubscribe-on-false idiom and
// the ANY_EVENT / ALL_EVENT asymmetry. Each rule below is one of those documented lines.

#include <doctest.h>

#include <string>
#include <vector>

#include "engine/core/Observable.hpp"

namespace {

// Observable's subscriber map is protected, so tests observe through a plain subclass --
// the same way every real engine type uses it.
class Subject : public Observable {};

} // namespace

TEST_CASE("CreateEvent hands out unique ids") {
    const auto a = Observable::CreateEvent();
    const auto b = Observable::CreateEvent();

    CHECK(a != b);
    // Reserved values must never be minted, or a real event would alias "any" or "all".
    CHECK(a != Observable::ANY_EVENT);
    CHECK(a != Observable::ALL_EVENT);
    CHECK(b != Observable::ANY_EVENT);
    CHECK(b != Observable::ALL_EVENT);
}

TEST_CASE("a callback returning false is auto-unsubscribed after one dispatch") {
    Subject subject;
    int calls = 0;

    // The one-shot idiom the whole codebase relies on.
    subject.Subscribe([&]() { ++calls; return false; });

    subject.Notify();
    subject.Notify();
    subject.Notify();

    CHECK(calls == 1);
}

TEST_CASE("a callback returning true persists across dispatches") {
    Subject subject;
    int calls = 0;

    subject.Subscribe([&]() { ++calls; return true; });

    subject.Notify();
    subject.Notify();

    CHECK(calls == 2);
}

TEST_CASE("an ANY_EVENT subscriber hears specific events") {
    Subject subject;
    const auto event = Observable::CreateEvent();
    int calls = 0;

    subject.Subscribe([&]() { ++calls; return true; });   // defaults to ANY_EVENT

    subject.Notify(event);

    CHECK(calls == 1);
}

TEST_CASE("an ANY_EVENT subscriber DOES hear ALL_EVENT broadcasts") {
    Subject subject;
    int calls = 0;

    subject.Subscribe([&]() { ++calls; return true; });

    // Notify(ALL_EVENT) iterates every bucket in the subscriber map, and ANY_EVENT
    // subscribers live in the bucket keyed 0 -- so they are reached like any other.
    //
    // Engine/CLAUDE.md's "Gotchas" line claimed the opposite ("ANY_EVENT listeners do not
    // hear ALL_EVENT broadcasts") while its own numbered rule 2 four lines above said
    // "ALL_EVENT -> all buckets". The code has always matched rule 2; the gotcha line was
    // simply stale, and has been corrected. This test is what keeps the two in agreement.
    subject.Notify(Observable::ALL_EVENT);

    CHECK(calls == 1);
}

TEST_CASE("a specific-event subscriber ignores other events") {
    Subject subject;
    const auto wanted   = Observable::CreateEvent();
    const auto unwanted = Observable::CreateEvent();
    int calls = 0;

    subject.Subscribe([&]() { ++calls; return true; }, wanted);

    subject.Notify(unwanted);
    CHECK(calls == 0);

    subject.Notify(wanted);
    CHECK(calls == 1);
}

TEST_CASE("Notify(ALL_EVENT) reaches every bucket") {
    Subject subject;
    const auto first  = Observable::CreateEvent();
    const auto second = Observable::CreateEvent();
    int firstCalls = 0, secondCalls = 0;

    subject.Subscribe([&]() { ++firstCalls;  return true; }, first);
    subject.Subscribe([&]() { ++secondCalls; return true; }, second);

    subject.Notify(Observable::ALL_EVENT);

    CHECK(firstCalls  == 1);
    CHECK(secondCalls == 1);
}

TEST_CASE("Unsubscribe(id) stops a callback firing") {
    Subject subject;
    int calls = 0;

    const int id = subject.Subscribe([&]() { ++calls; return true; });
    subject.Notify();
    REQUIRE(calls == 1);

    subject.Unsubscribe(id);
    subject.Notify();

    CHECK(calls == 1);
}

TEST_CASE("a handler may subscribe during dispatch without disturbing it") {
    Subject subject;
    int outer = 0, inner = 0;

    // Notify copies the matching callbacks before executing them precisely so this is
    // safe. Without the copy this mutates the vector being iterated.
    subject.Subscribe([&]() {
        ++outer;
        if (outer == 1)
            subject.Subscribe([&]() { ++inner; return true; });
        return true;
    });

    subject.Notify();
    // The newly added handler must not run in the dispatch that created it.
    CHECK(outer == 1);
    CHECK(inner == 0);

    subject.Notify();
    CHECK(outer == 2);
    CHECK(inner == 1);
}

TEST_CASE("a handler may unsubscribe another during dispatch") {
    Subject subject;
    int firstCalls = 0, secondCalls = 0;
    int secondId = 0;

    subject.Subscribe([&]() { ++firstCalls; subject.Unsubscribe(secondId); return true; });
    secondId = subject.Subscribe([&]() { ++secondCalls; return true; });

    CHECK_NOTHROW(subject.Notify());
    CHECK(firstCalls  == 1);
    // Still runs in THIS dispatch: Notify executed against a copy taken before the first
    // handler removed it. That is the copy-first guarantee, not an accident.
    CHECK(secondCalls == 1);

    subject.Notify();
    CHECK(firstCalls  == 2);
    CHECK(secondCalls == 1);   // gone from the map, so never again
}

TEST_CASE("payload subscribers receive the std::any data") {
    Subject subject;
    const auto event = Observable::CreateEvent();
    std::string received;

    subject.Subscribe([&](std::any data) {
        // Sender and handler must agree on the type; the commonest payload is an object id.
        if (data.has_value() && data.type() == typeid(std::string))
            received = std::any_cast<std::string>(data);
        return true;
    }, event);

    subject.Notify(event, std::string("object-id"));

    CHECK(received == "object-id");
}
