#include "../spikes/vst3-callback/sclang_osc_protocol.h"
#include "../spikes/vst3-callback/sclang_runtime_limits.h"
#include "../spikes/vst3-callback/event_effect_tail.h"

#include <cassert>
#include <string>
#include <vector>

static std::string oscStringAt(const std::vector<char>& packet,
                               std::size_t offset,
                               std::size_t& next) {
    assert(offset < packet.size());
    std::size_t end = offset;
    while (end < packet.size() && packet[end] != '\0')
        ++end;
    assert(end < packet.size());
    next = (end + 4u) & ~std::size_t(3u);
    assert(next <= packet.size());
    return {packet.data() + offset, end - offset};
}

int main() {
    using namespace dexter::sclang;
    using namespace dexter;

    assert(validFirstUserAudioBus(2));
    assert(validFirstUserAudioBus(1023));
    assert(validFirstUserAudioBus(1338));
    assert(validFirstUserAudioBus(1626));
    assert(validFirstUserAudioBus(2046));
    assert(!validFirstUserAudioBus(0));
    assert(!validFirstUserAudioBus(2047));
    assert(!validFirstUserAudioBus(2048));

    assert(kMaxEventEffectTailVoicesPerPlayer == 4);
    assert(eventEffectTailQuietGraceSeconds("JPverb") == 0.75);
    assert(eventEffectTailQuietGraceSeconds("Greyhole") == 1.5);
    assert(eventEffectTailQuietGraceSeconds("LimutPingPong") == 2.1);
    assert(eventEffectTailQuietGraceSeconds("Chorus") == 0.0);
    auto tail = beginEventEffectTail("JPverb", 100, 48000.0);
    assert(tail.active && !tail.finished);
    assert(!updateEventEffectTail(tail, 1.0e-4f, 100, 64));
    assert(tail.quietStartSample == 0);
    assert(!updateEventEffectTail(tail, 0.0f, 164, 64));
    assert(!updateEventEffectTail(
        tail, 0.0f, 164 + 48000 * 3 / 4 - 65, 64));
    assert(updateEventEffectTail(
        tail, 0.0f, 164 + 48000 * 3 / 4, 64));
    auto boundedTail = beginEventEffectTail("Greyhole", 100, 1000.0);
    assert(boundedTail.active);
    assert(updateEventEffectTail(
        boundedTail, 0.0f, boundedTail.hardStopSample, 64));
    auto unsupportedTail = beginEventEffectTail("Tremolo", 100, 48000.0);
    assert(!unsupportedTail.active);

    const auto eval = makeOscString("/dexter/evalFile",
                                    "C:/Users/test/AppData/Local/Dexter/run.scd");
    assert(!eval.empty());
    assert((eval.size() & 3u) == 0);
    std::size_t next = 0;
    assert(oscStringAt(eval, 0, next) == "/dexter/evalFile");
    assert(oscStringAt(eval, next, next) == ",s");
    assert(oscStringAt(eval, next, next) ==
           "C:/Users/test/AppData/Local/Dexter/run.scd");

    const auto stop = makeOscNoArgs("/dexter/stop");
    assert(!stop.empty());
    assert((stop.size() & 3u) == 0);
    assert(oscStringAt(stop, 0, next) == "/dexter/stop");
    assert(oscStringAt(stop, next, next) == ",");

    assert(makeOscNoArgs("dexter/stop").empty());
    assert(makeOscString("/dexter/evalFile",
                         std::string(kMaxOscStringBytes, 'x')).empty());
}
