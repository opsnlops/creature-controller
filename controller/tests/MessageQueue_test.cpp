
#include <thread>

#include <gtest/gtest.h>

#include "util/MessageQueue.h"

TEST(MessageQueue, SingleThreadPushPop) {
    creatures::MessageQueue<int> queue;

    queue.push(1);
    queue.push(2);

    EXPECT_EQ(queue.pop(), 1);
    EXPECT_EQ(queue.pop(), 2);
}

void pushMessages(creatures::MessageQueue<int>& queue, int start, int end) {
    for (int i = start; i <= end; ++i) {
        queue.push(i);
    }
}

void popMessages(creatures::MessageQueue<int>& queue, int count, std::vector<int>& out) {
    for (int i = 0; i < count; ++i) {
        out.push_back(queue.pop());
    }
}

TEST(MessageQueue, MultiThreadedPushPop) {
    creatures::MessageQueue<int> queue;
    std::vector<int> poppedMessages;
    std::thread producerThread(pushMessages, std::ref(queue), 1, 10);
    std::thread consumerThread(popMessages, std::ref(queue), 10, std::ref(poppedMessages));

    producerThread.join();
    consumerThread.join();

    // Check that all messages were popped in order
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(poppedMessages[i], i + 1);
    }
}

