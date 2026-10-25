#include <cassert>
#include <cstdio>
#include "cpp_c_api.h"

class FakeBus {
public:
    FakeBus() noexcept : selected_(false), writes_(0u) {}
    void select() noexcept { assert(!selected_); selected_ = true; }
    void release() noexcept { selected_ = false; }
    void write() noexcept { assert(selected_); ++writes_; }
    bool selected() const noexcept { return selected_; }
    unsigned writes() const noexcept { return writes_; }
private:
    bool selected_;
    unsigned writes_;
};

class Selection {
public:
    explicit Selection(FakeBus& bus) noexcept : bus_(bus) { bus_.select(); }
    ~Selection() noexcept { bus_.release(); }
    Selection(const Selection&) = delete;
    Selection& operator=(const Selection&) = delete;
private:
    FakeBus& bus_;
};

static int send_packet(FakeBus& bus, unsigned length)
{
    Selection guard(bus);
    // Deliberately validate after acquisition to exercise early-return cleanup.
    if (!packet_size_valid(length)) {
        return -1;
    }
    bus.write();
    return 0;
}

int main()
{
    FakeBus bus;
    int early = send_packet(bus, 0u);
    assert(early == -1 && !bus.selected() && bus.writes() == 0u);
    int oversized = send_packet(bus, 5u);
    assert(oversized == -1 && !bus.selected());
    int normal = send_packet(bus, 3u);
    assert(normal == 0 && !bus.selected() && bus.writes() == 1u);
    assert(packet_size_valid(4u) == 1);
    std::printf("early=%d oversized=%d normal=%d\n", early, oversized, normal);
    std::printf("selected=%d writes=%u\n", bus.selected() ? 1 : 0, bus.writes());
    return 0;
}
