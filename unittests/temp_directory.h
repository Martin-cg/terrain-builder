#pragma once

#include <filesystem>
#include <string_view>

// A fresh directory under the unit test output tree, removed again when the test ends.
class TempDirectory {
public:
    explicit TempDirectory(const std::string_view name)
        : _path(std::filesystem::path("./unittests/output") / name) {
        std::filesystem::remove_all(this->_path);
        std::filesystem::create_directories(this->_path);
    }

    ~TempDirectory() {
        std::filesystem::remove_all(this->_path);
    }

    TempDirectory(const TempDirectory &) = delete;
    TempDirectory &operator=(const TempDirectory &) = delete;

    const std::filesystem::path &path() const {
        return this->_path;
    }

private:
    std::filesystem::path _path;
};
