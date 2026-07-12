#pragma once

#include <filesystem>
#include <cstdint>
#include <fstream>
#include <sstream>

#include "common/config.h"

template <typename Loader>
void load_json_config(Loader loader, Config& config);

std::ifstream obtain_stream_from_file(const std::filesystem::path& path);

struct LoadFromFile
{
    LoadFromFile(const std::filesystem::path& path) : path_(path) {}

    void assign_path(const std::string& path)
    {
        path_ = path;
    }

    std::ifstream operator()() const
    {
        return obtain_stream_from_file(path_);
    }

    std::string source() const
    {
        return path_.string();
    }

    std::filesystem::path path_;
};

struct LoadFromString
{
    LoadFromString(std::string_view str) : str_(str.data(), str.size()) {}

    void assign_path(const std::string& path)
    {
        path_ = path;
    }

    std::stringstream operator()() const
    {
        return std::stringstream(str_);
    }

    std::string source() const
    {
        return str_;
    }

    const std::string str_;
    std::filesystem::path path_;
};
