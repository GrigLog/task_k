#include "common/message.h"

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

TEST(Message, RoundTripUint64Length) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    const std::string payload = "hello\x00world";
    EXPECT_TRUE(TMessage::WriteToSocket(fds[0], payload));

    std::string received;
    EXPECT_TRUE(TMessage::ReadFromSocket(fds[1], received));
    EXPECT_EQ(received, payload);

    close(fds[0]);
    close(fds[1]);
}

TEST(Message, EmptyPayload) {
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    EXPECT_TRUE(TMessage::WriteToSocket(fds[0], ""));
    std::string received;
    EXPECT_TRUE(TMessage::ReadFromSocket(fds[1], received));
    EXPECT_TRUE(received.empty());

    close(fds[0]);
    close(fds[1]);
}
