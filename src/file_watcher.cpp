#include "file_watcher.hpp"
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <iostream>

namespace fs = std::filesystem;

FileWatcher::FileWatcher(const std::string& path) : rootPath(path) {
    fd = inotify_init();
    if (fd < 0) perror("inotify_init");
    add_dir_to_watch(fs::path(path) / "src");
    add_dir_to_watch(fs::path(path) / "include");
}

FileWatcher::~FileWatcher() {
    for (auto const& [wd, path] : watch_descriptors) {
        inotify_rm_watch(fd, wd);
    }
    close(fd);
}


void FileWatcher::add_dir_to_watch(const fs::path& path) {
    if (fs::exists(path) && fs::is_directory(path)) {
        int wd = inotify_add_watch(fd, path.c_str(), IN_CLOSE_WRITE | IN_MODIFY);
        watch_descriptors[wd] = path.string();
        std::cout << "Watching: " << path << "\n";
    }
}

void FileWatcher::watch(std::function<void()> callback) {
    char buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    std::cout << "\033[1;35m" << ">> Live Watch Mode Active. Waiting for changes..." << "\033[0m\n";

    while (true) {
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if (len <= 0) break;

        auto now = std::chrono::steady_clock::now();

        // If multiple events happen within 100ms, ignore them
        if (now - last_event_time < debounce_duration) {
            continue;
        }

        for (char *ptr = buffer; ptr < buffer + len;
             ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            if (event->len > 0) {
                std::string filename = event->name;
                // Ignore swap and hidden files
                if (filename[0] == '.' || filename.find('~') != std::string::npos) continue;

                last_event_time = std::chrono::steady_clock::now();

                std::cout << "\n\033[1;32m" << "Change detected: " << filename << "\033[0m\n";
                callback();

                int flags = fcntl(fd, F_GETFL, 0);
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
                while (read(fd, buffer, sizeof(buffer)) > 0);
                fcntl(fd, F_SETFL, flags);

                std::cout << "\n\033[1;35m" << ">> Still watching..." << "\033[0m\n";
                break; // Exit the for-loop to wait for the next fresh event
            }
        }
    }
}
