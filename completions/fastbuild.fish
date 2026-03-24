complete -c fastbuild -f

function __fastbuild_in_project
    test -f meson.build
end

function __fastbuild_list_targets
    if __fastbuild_in_project
        grep -oP "executable\('\K[^']+" meson.build 2>/dev/null
        grep -oP "library\('\K[^']+" meson.build 2>/dev/null
    end
end

complete -c fastbuild -n __fish_use_subcommand -a add -d "Add a Meson dependency"
complete -c fastbuild -n __fish_use_subcommand -a add-remote -d "Add a remote git dependency"
complete -c fastbuild -n __fish_use_subcommand -a add-exe -d "Add a new executable target"
complete -c fastbuild -n __fish_use_subcommand -a add-lib -d "Add a new library target"
complete -c fastbuild -n __fish_use_subcommand -a doctor -d "Check system dependencies"
complete -c fastbuild -n __fish_use_subcommand -a help -d "Show help message"
complete -c fastbuild -n __fish_use_subcommand -l watch -d "Watch for changes and rebuild"
complete -c fastbuild -n __fish_use_subcommand -l help -d "Show help message"
complete -c fastbuild -n __fish_use_subcommand -s h -d "Show help message"
complete -c fastbuild -n "__fish_seen_subcommand_from --watch" -l run -d "Auto-run after build"
complete -c fastbuild -n "__fish_seen_subcommand_from --watch" -l debug -d "Attach debugger after build"

set -l common_deps \
    "fmt:Formatting library" \
    "spdlog:Logging library" \
    "catch2:Testing framework" \
    "boost:Boost C++ libraries" \
    "nlohmann_json:JSON library" \
    "gtest:Google Test framework" \
    "gmock:Google Mock framework" \
    "benchmark:Google Benchmark" \
    "opencv:Computer vision" \
    "glfw3:OpenGL framework" \
    "vulkan:Vulkan graphics API" \
    "sdl2:Media library" \
    "raylib:Game dev library" \
    "sqlite3:SQLite database" \
    "curl:HTTP client" \
    "openssl:Cryptography" \
    "zlib:Compression library" \
    "protobuf:Protocol buffers" \
    "grpc:RPC framework" \
    "eigen3:Linear algebra" \
    "asio:Async I/O" \
    "cli11:Command-line parser" \
    "doctest:Testing framework" \
    "imgui:GUI library" \
    "glm:OpenGL math" \
    "yaml-cpp:YAML parser"

for dep_desc in $common_deps
    set -l dep (string split -m1 ":" $dep_desc)[1]
    set -l desc (string split -m1 ":" $dep_desc)[2]
    complete -c fastbuild -n "__fish_seen_subcommand_from add" -a "$dep" -d "$desc"
end

complete -c fastbuild -n "__fish_seen_subcommand_from add-exe" -f
complete -c fastbuild -n "__fish_seen_subcommand_from add-lib" -f
