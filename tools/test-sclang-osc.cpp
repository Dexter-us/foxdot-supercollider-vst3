#include "../spikes/vst3-callback/sclang_osc_protocol.h"

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
