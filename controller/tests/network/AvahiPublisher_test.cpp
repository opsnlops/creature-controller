#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <avahi-common/strlst.h>

#include "creature/network/AvahiPublisher.h"

namespace creatures::network {

namespace {

// Read back TXT entries in the order they go on the wire
std::vector<std::string> wireOrder(AvahiStringList *list) {
    std::vector<uint8_t> buffer(avahi_string_list_serialize(list, nullptr, 0));
    avahi_string_list_serialize(list, buffer.data(), buffer.size());

    std::vector<std::string> entries;
    for (size_t i = 0; i < buffer.size();) {
        const size_t length = buffer[i];
        entries.emplace_back(reinterpret_cast<const char *>(&buffer[i + 1]), length);
        i += 1 + length;
    }
    return entries;
}

TEST(AvahiPublisher, PublishesTxtEntriesInOrder) {
    MdnsService service;
    service.txt = {{"txtvers", "1"}, {"id", "abc"}, {"name", "Caroll"}, {"server", "10.19.63.5:8000"}};

    AvahiStringList *list = makeAvahiTxt(service);
    const auto entries = wireOrder(list);
    avahi_string_list_free(list);

    EXPECT_EQ(entries, (std::vector<std::string>{"txtvers=1", "id=abc", "name=Caroll", "server=10.19.63.5:8000"}));
}

} // namespace

} // namespace creatures::network
