#include "api.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstdlib>

using json = nlohmann::json;

namespace {

size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *buf = static_cast<std::string *>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t write_cb_bytes(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *buf = static_cast<std::vector<uint8_t> *>(userdata);
    auto *bytes = reinterpret_cast<uint8_t *>(ptr);
    buf->insert(buf->end(), bytes, bytes + size * nmemb);
    return size * nmemb;
}

// Performs a GET request and returns the response body as a string.
// Throws catapi::ApiError on transport failure or non-2xx status.
std::string http_get(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw catapi::ApiError("Failed to initialize HTTP client");
    }

    std::string body;
    struct curl_slist *headers = nullptr;
    if (const char *key = std::getenv("CAT_API_KEY")) {
        headers = curl_slist_append(headers, (std::string("x-api-key: ") + key).c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "meow-cli/0.1.0");
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (headers) {
        curl_slist_free_all(headers);
    }

    if (res != CURLE_OK) {
        std::string err = curl_easy_strerror(res);
        curl_easy_cleanup(curl);
        throw catapi::ApiError("Request failed: " + err);
    }
    curl_easy_cleanup(curl);

    if (status < 200 || status >= 300) {
        throw catapi::ApiError("API returned error status: " + std::to_string(status));
    }

    return body;
}

catapi::CatImage image_from_json(const json &j) {
    catapi::CatImage img;
    img.id = j.value("id", "");
    img.url = j.value("url", "");
    img.width = j.value("width", 0);
    img.height = j.value("height", 0);
    return img;
}

} // namespace

namespace catapi {

void init() { curl_global_init(CURL_GLOBAL_DEFAULT); }
void cleanup() { curl_global_cleanup(); }

std::vector<Category> fetch_categories() {
    std::string body = http_get(std::string(API) + "/categories");
    json j = json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array()) {
        throw ApiError("Failed to decode categories response");
    }

    std::vector<Category> categories;
    categories.reserve(j.size());
    for (const auto &item : j) {
        Category c;
        c.id = item.value("id", 0);
        c.name = item.value("name", "");
        if (!c.name.empty()) {
            categories.push_back(std::move(c));
        }
    }
    return categories;
}

std::vector<CatImage> fetch_images(int count, std::optional<int> category_id) {
    std::string url = std::string(API) + "/images/search?limit=" + std::to_string(count);
    if (category_id) {
        url += "&category_ids=" + std::to_string(*category_id);
    }

    std::string body = http_get(url);
    json j = json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_array()) {
        throw ApiError("Failed to decode image response");
    }

    std::vector<CatImage> images;
    images.reserve(j.size());
    for (const auto &item : j) {
        images.push_back(image_from_json(item));
    }
    return images;
}

CatImage fetch_image(std::optional<int> category_id) {
    auto images = fetch_images(1, category_id);
    if (images.empty()) {
        throw ApiError("No images returned");
    }
    return images.front();
}

std::vector<uint8_t> download_bytes(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        throw ApiError("Failed to initialize HTTP client");
    }

    std::vector<uint8_t> bytes;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb_bytes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &bytes);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "meow-cli/0.1.0");

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw ApiError(std::string("Failed to download image: ") + curl_easy_strerror(res));
    }
    if (status < 200 || status >= 300) {
        throw ApiError("Failed to download image, status: " + std::to_string(status));
    }
    return bytes;
}

bool ping() {
    try {
        http_get(std::string(API) + "/categories");
        return true;
    } catch (const ApiError &) {
        return false;
    }
}

} // namespace catapi
