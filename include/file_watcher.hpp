#pragma once
#include <string>
#include <filesystem>
#include <functional>
#include <map>
#include <chrono>

class FileWatcher {
public:
    FileWatcher(const std::string& path);
    ~FileWatcher();
    void watch(std::function<void()> callback);

private:
    int fd;
    std::map<int, std::string> watch_descriptors;
    std::string rootPath;

    // Debounce state
    std::chrono::steady_clock::time_point last_event_time;
    const std::chrono::milliseconds debounce_duration{100};

    void add_dir_to_watch(const std::filesystem::path& path);
};
