#include <chrono>
#include <fcntl.h>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

#include <gtest/gtest.h>

#include "io/Message.h"
#include "io/SerialWriter.h"
#include "mocks/logging/MockLogger.h"
#include "util/MessageQueue.h"

namespace creatures::io {

namespace {

using namespace std::chrono_literals;

// A non-blocking pipe behaves like the serial port when its buffer is full
class NonBlockingPipe {
  public:
    NonBlockingPipe() {
        EXPECT_EQ(pipe(fds), 0);
        fcntl(fds[0], F_SETFL, O_NONBLOCK);
        fcntl(fds[1], F_SETFL, O_NONBLOCK);
    }

    ~NonBlockingPipe() {
        close(fds[0]);
        close(fds[1]);
    }

    [[nodiscard]] int readEnd() const { return fds[0]; }
    [[nodiscard]] int writeEnd() const { return fds[1]; }

    // Returns how many bytes it took to fill the pipe
    size_t fill() const {
        const std::string chunk(1024, 'x');
        size_t total = 0;
        ssize_t written;
        while ((written = write(fds[1], chunk.data(), chunk.size())) > 0) {
            total += static_cast<size_t>(written);
        }
        return total;
    }

  private:
    int fds[2] = {-1, -1};
};

template <typename Predicate> bool waitFor(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

class SerialWriterTest : public ::testing::Test {
  protected:
    std::shared_ptr<SerialWriter> makeWriter(int fileDescriptor, std::chrono::milliseconds timeout) {
        writer = std::make_shared<SerialWriter>(logger, "test-device", UARTDevice::A, fileDescriptor, queue, timeout);
        return writer;
    }

    void TearDown() override {
        if (writer) {
            // shutdown() detaches, so give the thread time to finish with the object
            writer->shutdown();
            std::this_thread::sleep_for(250ms);
        }
    }

    std::shared_ptr<NiceMockLogger> logger = std::make_shared<NiceMockLogger>();
    std::shared_ptr<MessageQueue<Message>> queue = std::make_shared<MessageQueue<Message>>();
    std::shared_ptr<SerialWriter> writer;
};

TEST_F(SerialWriterTest, SurvivesFullBufferAndDeliversWholeMessage) {
    NonBlockingPipe pipe;
    const size_t prefill = pipe.fill();

    // Larger than the pipe, so this can only arrive through partial writes
    const std::string payload(prefill * 3, 'p');
    queue->push(Message(UARTDevice::A, payload));

    makeWriter(pipe.writeEnd(), 5000ms)->start();
    std::this_thread::sleep_for(50ms); // Let it hit EAGAIN

    std::string received;
    const size_t expected = prefill + payload.size() + 1;
    char buffer[4096];
    ASSERT_TRUE(waitFor([&] {
        const ssize_t count = read(pipe.readEnd(), buffer, sizeof(buffer));
        if (count > 0) {
            received.append(buffer, static_cast<size_t>(count));
        }
        return received.size() >= expected;
    }));

    EXPECT_EQ(received.size(), expected);
    EXPECT_EQ(received.substr(prefill), payload + "\n");
    EXPECT_FALSE(writer->hasFailed());
}

TEST_F(SerialWriterTest, ReportsFailureWhenPortStaysStalled) {
    NonBlockingPipe pipe;
    pipe.fill();
    queue->push(Message(UARTDevice::A, "POS\t0 1500"));

    makeWriter(pipe.writeEnd(), 50ms)->start();

    EXPECT_TRUE(waitFor([&] { return writer->hasFailed(); }));
}

TEST_F(SerialWriterTest, ReportsFailureOnBadFileDescriptor) {
    queue->push(Message(UARTDevice::A, "POS\t0 1500"));

    makeWriter(-1, 50ms)->start();

    EXPECT_TRUE(waitFor([&] { return writer->hasFailed(); }));
}

TEST_F(SerialWriterTest, StoppingWhileStalledIsNotAFailure) {
    NonBlockingPipe pipe;
    pipe.fill();
    queue->push(Message(UARTDevice::A, "POS\t0 1500"));

    makeWriter(pipe.writeEnd(), 60000ms)->start();
    std::this_thread::sleep_for(50ms);

    writer->shutdown();
    std::this_thread::sleep_for(250ms);

    EXPECT_FALSE(writer->hasFailed());
}

} // namespace

} // namespace creatures::io
