
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "controller/Input.h"

TEST(Input, Create) {

    EXPECT_NO_THROW({
        auto input = creatures::Input("test", 1, 2);
    });
}

TEST(Input, Getter) {

    auto input = creatures::Input("test", 1, 2);

    EXPECT_EQ(input.getName(), "test");
    EXPECT_EQ(input.getSlot(), 1);
    EXPECT_EQ(input.getWidth(), 2);

}