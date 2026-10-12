#pragma once
#include <memory>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

// Linux 6.9 introduced process-group scope for a pidfd. The kernel uses the
// descriptor's held struct pid, even after its leader is reaped; no PID lookup.
constexpr unsigned MediaProcessGroupScope = 1U << 2;
struct MediaGroup {
    int fd = -1;
    explicit MediaGroup(int descriptor) : fd(descriptor) {}
    ~MediaGroup() { if (fd >= 0) ::close(fd); }
    MediaGroup(const MediaGroup &) = delete;
    MediaGroup &operator=(const MediaGroup &) = delete;
    int signal(int number) const {
        return int(::syscall(SYS_pidfd_send_signal, fd, number, nullptr, MediaProcessGroupScope));
    }
};

// Runs in QProcess's child modifier before exec. Only stack storage and Linux
// syscalls are used. Capturing self avoids any parent PID/reap acquisition race.
inline bool sendOwnMediaGroup(int socket) {
    const int fd = int(::syscall(SYS_pidfd_open, ::getpid(), 0));
    const bool supported = fd >= 0 && ::syscall(SYS_pidfd_send_signal, fd, 0, nullptr, MediaProcessGroupScope) == 0;
    char tag = supported ? 'G' : 'U';
    struct iovec vector {&tag, sizeof(tag)};
    alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(int))] {};
    struct msghdr message {};
    message.msg_iov = &vector; message.msg_iovlen = 1;
    if (supported) {
        message.msg_control = control; message.msg_controllen = sizeof(control);
        auto ancillary = CMSG_FIRSTHDR(&message);
        ancillary->cmsg_level = SOL_SOCKET; ancillary->cmsg_type = SCM_RIGHTS; ancillary->cmsg_len = CMSG_LEN(sizeof(int));
        __builtin_memcpy(CMSG_DATA(ancillary), &fd, sizeof(fd));
    }
    const auto sent = ::sendmsg(socket, &message, MSG_NOSIGNAL);
    if (fd >= 0) ::close(fd);
    if (!supported || sent != sizeof(tag)) return false;
    // The child must not exec until the GUI has adopted the descriptor. A lost
    // or rejected transfer cannot leave a renderer without a cleanup owner.
    struct pollfd ready {socket, POLLIN, 0};
    char acknowledgement = 0;
    return ::poll(&ready, 1, 2000) > 0 && ::recv(socket, &acknowledgement, sizeof(acknowledgement), 0) == sizeof(acknowledgement) && acknowledgement == 'A';
}
inline std::shared_ptr<MediaGroup> receiveMediaGroup(int socket, bool *unsupported = nullptr) {
    char tag = 0;
    struct iovec vector {&tag, sizeof(tag)};
    alignas(struct cmsghdr) char control[CMSG_SPACE(sizeof(int))] {};
    struct msghdr message {};
    message.msg_iov = &vector; message.msg_iovlen = 1;
    message.msg_control = control; message.msg_controllen = sizeof(control);
    const auto bytes = ::recvmsg(socket, &message, MSG_CMSG_CLOEXEC);
    if (unsupported) *unsupported = bytes == sizeof(tag) && tag == 'U';
    int fd = -1;
    const auto ancillary = CMSG_FIRSTHDR(&message);
    if (bytes == sizeof(tag) && ancillary && ancillary->cmsg_level == SOL_SOCKET && ancillary->cmsg_type == SCM_RIGHTS && ancillary->cmsg_len == CMSG_LEN(sizeof(int)))
        __builtin_memcpy(&fd, CMSG_DATA(ancillary), sizeof(fd));
    if (tag != 'G' || (message.msg_flags & MSG_CTRUNC)) { if (fd >= 0) ::close(fd); return {}; }
    return fd >= 0 ? std::make_shared<MediaGroup>(fd) : std::shared_ptr<MediaGroup>();
}

inline bool acknowledgeMediaGroup(int socket, bool accepted) {
    const char tag = accepted ? 'A' : 'N';
    return ::send(socket, &tag, sizeof(tag), MSG_NOSIGNAL) == sizeof(tag);
}
