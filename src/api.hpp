#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace catapi {

constexpr const char *API = "https://api.thecatapi.com/v1";

struct ApiError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct Category {
    int id = 0;
    std::string name;
};

struct CatImage {
    std::string id;
    std::string url;
    int width = 0;
    int height = 0;
};

// Must be called once before any other function, and cleanup() at exit.
void init();
void cleanup();

std::vector<Category> fetch_categories();
std::vector<CatImage> fetch_images(int count, std::optional<int> category_id);
CatImage fetch_image(std::optional<int> category_id);
std::vector<uint8_t> download_bytes(const std::string &url);

// Lightweight HEAD/GET check that the API is reachable.
bool ping();

} // namespace catapi
