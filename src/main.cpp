#include "api.hpp"
#include "render.hpp"
#include "terminal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <mutex>
#include <optional>
#include <fcntl.h>
#include <queue>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char *VERSION = "0.1.0";

struct CategoryFilter {
    int id;
    std::string name;
};

struct DownloadFilters {
    std::optional<double> min_size_kb;
    std::optional<int> min_width;
    std::optional<int> min_height;
};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

void open_url(const std::string &url) {
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
#if defined(__APPLE__)
        execlp("open", "open", url.c_str(), nullptr);
#else
        execlp("xdg-open", "xdg-open", url.c_str(), nullptr);
#endif
        _exit(127);
    }
}

std::string filename_from_url(const std::string &url, const std::string &fallback) {
    auto pos = url.find_last_of('/');
    std::string name = pos == std::string::npos ? url : url.substr(pos + 1);
    return name.empty() ? fallback : name;
}

void print_help() {
    std::cout <<
        "Usage: meow <command>\n\n"
        "A simple CLI to fetch cat pictures from thecatapi.com.\n\n"
        "Commands:\n"
        "  -c, --category <name>   Fetch an image from a specific category\n"
        "  -n, --batch <amount>    Use '-n <amount>' after category to batch download (e.g. -c boxes -n 50)\n"
        "  -l, --list              List all available categories\n"
        "  -r, --random            Fetch a random image from any category\n"
        "  -v, --version           Show version information\n"
        "  -o                      Open a random image URL in the default system viewer\n"
        "  -t, --test              Test connectivity\n"
        "  --min-size <KB>         Filter batch downloads by minimum file size\n"
        "  --min-width <pixels>    Filter batch downloads by minimum width\n"
        "  --min-height <pixels>   Filter batch downloads by minimum height\n"
        "  --no-upscale            Don't upscale small images to fit the terminal\n"
        "  --check-links           Perform a deep check of category endpoints\n"
        "  -h, --help              Show this help message\n";
}

void show_stats() {
    const char *GREEN = "\x1b[32m";
    const char *RED = "\x1b[31m";
    const char *RESET = "\x1b[0m";

    std::cout << "Running Program Functional Test...\n";
    auto start = std::chrono::steady_clock::now();

    std::vector<catapi::Category> categories;
    try {
        categories = catapi::fetch_categories();
    } catch (const catapi::ApiError &e) {
        std::cout << RED << "Program Test: Failed" << RESET << "\n" << e.what() << "\n";
        return;
    }
    if (categories.empty()) {
        std::cout << RED << "Program Test: Failed" << RESET << "\nNo categories found from API.\n";
        return;
    }
    std::cout << "Endpoints: " << categories.size() << " categories found\n";

    catapi::CatImage img;
    try {
        img = catapi::fetch_image(categories.front().id);
    } catch (const catapi::ApiError &e) {
        std::cout << RED << "Program Test: Failed" << RESET << "\n"
                  << "Failed to fetch image metadata for '" << categories.front().name << "'\n";
        return;
    }
    std::cout << "Single Image Fetch: " << GREEN << "Passed" << RESET << "\n";
    std::cout << "Image URL: " << img.url << "\n";

    std::vector<uint8_t> bytes;
    try {
        bytes = catapi::download_bytes(img.url);
    } catch (const catapi::ApiError &) {
        std::cout << RED << "Program Test: Failed" << RESET << "\n"
                  << "Failed to download image from '" << img.url << "'\n";
        return;
    }
    std::cout << "Image Bytes: " << GREEN << "OK" << RESET << "\n";
    std::printf("Size: %.2f KB\n", bytes.size() / 1024.0);

    std::vector<catapi::CatImage> batch;
    try {
        batch = catapi::fetch_images(5, categories.front().id);
    } catch (const catapi::ApiError &) {
        std::cout << RED << "Program Test: Failed" << RESET << "\nFailed to fetch batch URLs.\n";
        return;
    }
    if (batch.size() < 2) {
        std::cout << RED << "Program Test: Failed" << RESET << "\nBatch download returned less than 2 images.\n";
        return;
    }
    std::cout << "Batch Download (" << batch.size() << " images): " << GREEN << "Passed" << RESET << "\n";
    std::cout << "Program Test: " << GREEN << "Passed" << RESET << "\n";

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("Time Taken: %.2fs\n", elapsed);
}

void check_all_endpoints(const std::vector<catapi::Category> &categories) {
    std::cout << "Thorough Connectivity Check (" << categories.size() << " categories):\n";
    auto start = std::chrono::steady_clock::now();

    std::mutex print_mutex;
    std::atomic<int> passed{0};
    std::vector<std::thread> threads;

    for (const auto &cat : categories) {
        threads.emplace_back([&, cat]() {
            bool ok = false;
            try {
                ok = !catapi::fetch_images(1, cat.id).empty();
            } catch (const catapi::ApiError &) {
                ok = false;
            }
            std::lock_guard<std::mutex> lock(print_mutex);
            std::printf("  Checking %-12s ... ", cat.name.c_str());
            if (ok) {
                std::cout << "\x1b[32mPASSED\x1b[0m\n";
                ++passed;
            } else {
                std::cout << "\x1b[31mFAILED\x1b[0m\n";
            }
            std::fflush(stdout);
        });
    }
    for (auto &t : threads) t.join();

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("\nSummary: %d/%zu endpoints are healthy. Time Taken: %.2fs\n", passed.load(), categories.size(), elapsed);
}

bool render_or_fallback(const std::vector<uint8_t> &bytes, const std::string &url, bool interactive, bool upscale) {
    auto [cols, rows] = term::size();
    if (!render::render_image(bytes, cols, rows, interactive, upscale)) {
        std::cout << "No terminal image viewer found (kitty, wezterm, viu, chafa).\n";
        std::cout << "Image URL: " << url << "\n";
        return false;
    }
    return true;
}

void fetch_and_display_image(const std::optional<CategoryFilter> &filter, bool upscale) {
    term::enter_alt_screen();
    term::hide_cursor();
    std::vector<uint8_t> last_bytes;
    std::string last_url;

    while (true) {
        term::clear_all();
        term::move_to(0, 0);

        catapi::CatImage img;
        try {
            img = catapi::fetch_image(filter ? std::optional<int>(filter->id) : std::nullopt);
        } catch (const catapi::ApiError &e) {
            std::cerr << "Error: " << e.what() << "\n";
            break;
        }

        std::vector<uint8_t> bytes;
        try {
            bytes = catapi::download_bytes(img.url);
        } catch (const catapi::ApiError &e) {
            std::cerr << "Error: " << e.what() << "\n";
            break;
        }

        render_or_fallback(bytes, img.url, true, upscale);

        bool continue_fetching = false;
        term::enable_raw_mode();

        auto print_prompt = []() {
            std::string prompt = "[s]ave | [u]rl | [a]info | [o]pen | [n]ext | [q]uit:";
            auto [c, r] = term::size();
            int padding = c > static_cast<int>(prompt.size()) ? (c - static_cast<int>(prompt.size())) / 2 : 0;
            term::move_to(padding, r - 1);
            term::clear_current_line();
            std::fputs(prompt.c_str(), stdout);
            std::fflush(stdout);
        };
        print_prompt();

        bool quit = false;
        while (!quit) {
            term::Event ev = term::poll_event(200);
            if (ev.type == term::EventType::Resize) {
                term::clear_all();
                term::move_to(0, 0);
                render_or_fallback(bytes, img.url, true, upscale);
                print_prompt();
            } else if (ev.type == term::EventType::Key) {
                switch (ev.key) {
                    case 's': {
                        term::disable_raw_mode();
                        term::move_to_column(0);
                        term::clear_current_line();
                        std::string filename = filename_from_url(img.url, "meow.jpg");
                        FILE *f = std::fopen(filename.c_str(), "wb");
                        if (f) {
                            std::fwrite(bytes.data(), 1, bytes.size(), f);
                            std::fclose(f);
                            std::cout << "Image saved as " << filename << "\n\n";
                        } else {
                            std::cerr << "Error: Failed to save image.\n";
                        }
                        term::enable_raw_mode();
                        print_prompt();
                        break;
                    }
                    case 'u': {
                        term::disable_raw_mode();
                        term::move_to_column(0);
                        term::clear_current_line();
                        std::cout << "\nImage URL: " << img.url << "\n";
                        term::enable_raw_mode();
                        print_prompt();
                        break;
                    }
                    case 'a': {
                        term::disable_raw_mode();
                        term::move_to_column(0);
                        term::clear_current_line();
                        std::cout << "--- Info ---\n";
                        std::cout << "ID: " << img.id << "\n";
                        std::cout << "Size: " << img.width << "x" << img.height << "\n";
                        std::cout << "Category: " << (filter ? filter->name : "any") << "\n";
                        std::cout << "URL: " << img.url << "\n";
                        std::cout << "------------\n";
                        std::cout << "(Press any key to return)\n";
                        std::fflush(stdout);
                        term::poll_event(30000);
                        term::enable_raw_mode();
                        term::clear_from_cursor_up();
                        print_prompt();
                        break;
                    }
                    case 'o':
                        open_url(img.url);
                        break;
                    case 'n':
                        continue_fetching = true;
                        quit = true;
                        break;
                    case 'q':
                    case '\r':
                    case '\n':
                        quit = true;
                        break;
                    default:
                        break;
                }
            }
        }

        term::disable_raw_mode();
        std::cout << "\n";

        last_bytes = bytes;
        last_url = img.url;
        if (!continue_fetching) break;
    }

    term::leave_alt_screen();
    term::show_cursor();

    if (!last_bytes.empty()) {
        render_or_fallback(last_bytes, last_url, false, upscale);
    }
}

void batch_download(const std::optional<CategoryFilter> &filter, int count, const DownloadFilters &filters) {
    std::cout << "Starting batch download of " << count << " images"
              << (filter ? " from category '" + filter->name + "'" : "") << "...\n";
    auto start = std::chrono::steady_clock::now();

    std::unordered_set<std::string> seen_ids;
    std::vector<catapi::CatImage> collected;

    int rounds = 0;
    while (static_cast<int>(collected.size()) < count && rounds < 25) {
        rounds++;
        int remaining = count - static_cast<int>(collected.size());
        int request_amount = std::min(100, std::max(remaining * 2, 10));

        std::vector<catapi::CatImage> batch;
        try {
            batch = catapi::fetch_images(request_amount, filter ? std::optional<int>(filter->id) : std::nullopt);
        } catch (const catapi::ApiError &) {
            break;
        }
        if (batch.empty()) break;

        for (auto &img : batch) {
            if (!seen_ids.insert(img.id).second) continue;
            bool width_ok = !filters.min_width || img.width >= *filters.min_width;
            bool height_ok = !filters.min_height || img.height >= *filters.min_height;
            if (width_ok && height_ok) collected.push_back(img);
            if (static_cast<int>(collected.size()) >= count) break;
        }
    }

    if (static_cast<int>(collected.size()) > count) collected.resize(count);
    count = static_cast<int>(collected.size());

    if (count == 0) {
        std::cerr << "Error: Failed to fetch image URLs from the API.\n";
        return;
    }

    int workers = std::min(count, 16);
    std::atomic<int> next_index{0};
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::queue<double> speeds;

    std::vector<std::thread> threads;
    for (int w = 0; w < workers; w++) {
        threads.emplace_back([&]() {
            while (true) {
                int idx = next_index.fetch_add(1);
                if (idx >= count) break;
                const auto &img = collected[idx];

                double speed = 0.0;
                for (int attempt = 0; attempt < 3; attempt++) {
                    try {
                        auto start_dl = std::chrono::steady_clock::now();
                        std::vector<uint8_t> bytes = catapi::download_bytes(img.url);
                        double size_kb = bytes.size() / 1024.0;
                        if (filters.min_size_kb && size_kb < *filters.min_size_kb) break;

                        std::string filename = filename_from_url(img.url, img.id + ".jpg");
                        FILE *f = std::fopen(filename.c_str(), "wb");
                        if (!f) {
                            std::cerr << "\nError saving '" << filename << "'\n";
                            break;
                        }
                        std::fwrite(bytes.data(), 1, bytes.size(), f);
                        std::fclose(f);

                        double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_dl).count();
                        speed = elapsed > 0 ? size_kb / elapsed : size_kb;
                        break;
                    } catch (const catapi::ApiError &) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                }

                std::lock_guard<std::mutex> lock(queue_mutex);
                speeds.push(speed);
                queue_cv.notify_one();
            }
        });
    }

    for (int i = 0; i < count; i++) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        queue_cv.wait(lock, [&] { return !speeds.empty(); });
        double speed_kb = speeds.front();
        speeds.pop();
        lock.unlock();

        int percentage = ((i + 1) * 100) / count;
        int bar_len = 30;
        int filled = bar_len * (i + 1) / count;
        std::string bar = std::string(filled, '=') + std::string(bar_len - filled, ' ');

        std::printf("\r[%s] %d%% | Downloaded %d/%d | Speed: %.2f MB/s",
                     bar.c_str(), percentage, i + 1, count, speed_kb / 1024.0);
        std::fflush(stdout);
    }

    for (auto &t : threads) t.join();

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("\nCompleted %d images in %.2fs\n", count, elapsed);
}

std::optional<CategoryFilter> find_category(const std::vector<catapi::Category> &categories, const std::string &name) {
    std::string target = lower(name);
    for (const auto &c : categories) {
        if (lower(c.name) == target) return CategoryFilter{c.id, c.name};
    }
    return std::nullopt;
}

} // namespace

int main(int argc, char **argv) {
    std::signal(SIGPIPE, SIG_IGN);
    catapi::init();
    term::install_resize_handler();

    std::vector<std::string> args(argv + 1, argv + argc);

    bool upscale = true;
    auto no_upscale_it = std::find(args.begin(), args.end(), "--no-upscale");
    if (no_upscale_it != args.end()) {
        upscale = false;
        args.erase(no_upscale_it);
    }

    if (args.empty()) {
        fetch_and_display_image(std::nullopt, upscale);
        catapi::cleanup();
        return 0;
    }

    const std::string &command = args[0];

    if (command == "-v" || command == "--version") {
        std::cout << "meow-cli version " << VERSION << "\n";
    } else if (command == "-h" || command == "--help") {
        print_help();
    } else if (command == "-r" || command == "--random") {
        fetch_and_display_image(std::nullopt, upscale);
    } else if (command == "-o") {
        try {
            catapi::CatImage img = catapi::fetch_image(std::nullopt);
            std::cout << "Opening random image: " << img.url << "\n";
            open_url(img.url);
        } catch (const catapi::ApiError &e) {
            std::cerr << "Error: Failed to fetch image for opening.\n";
        }
    } else if (command == "-t" || command == "--test") {
        show_stats();
    } else if (command == "-l" || command == "--list") {
        try {
            auto categories = catapi::fetch_categories();
            std::sort(categories.begin(), categories.end(), [](auto &a, auto &b) { return a.name < b.name; });
            std::cout << "Available categories:\n";
            for (const auto &c : categories) std::cout << "  " << c.name << "\n";
        } catch (const catapi::ApiError &e) {
            std::cerr << "API Error: " << e.what() << "\n";
            catapi::cleanup();
            return 1;
        }
    } else if (command == "--check-links") {
        try {
            auto categories = catapi::fetch_categories();
            check_all_endpoints(categories);
        } catch (const catapi::ApiError &e) {
            std::cerr << "API Error: " << e.what() << "\n";
            catapi::cleanup();
            return 1;
        }
    } else if (command == "-c" || command == "--category") {
        if (args.size() < 2) {
            std::cerr << "Error: The '-c' flag requires a category name.\n";
            std::cerr << "Usage: meow -c <category_name>\n";
            catapi::cleanup();
            return 1;
        }
        std::vector<catapi::Category> categories;
        try {
            categories = catapi::fetch_categories();
        } catch (const catapi::ApiError &e) {
            std::cerr << "API Error: " << e.what() << "\n";
            catapi::cleanup();
            return 1;
        }

        auto filter = find_category(categories, args[1]);
        if (!filter) {
            std::cerr << "Error: Invalid category '" << args[1] << "'.\n";
            std::cerr << "Run 'meow -l' to see available categories.\n";
            catapi::cleanup();
            return 1;
        }

        if (args.size() >= 4 && (args[2] == "-n" || args[2] == "--batch")) {
            try {
                int amount = std::stoi(args[3]);
                DownloadFilters filters;
                size_t i = 4;
                while (i < args.size()) {
                    if (args[i] == "--min-size" && i + 1 < args.size()) {
                        filters.min_size_kb = std::stod(args[i + 1]);
                        i += 2;
                    } else if (args[i] == "--min-width" && i + 1 < args.size()) {
                        filters.min_width = std::stoi(args[i + 1]);
                        i += 2;
                    } else if (args[i] == "--min-height" && i + 1 < args.size()) {
                        filters.min_height = std::stoi(args[i + 1]);
                        i += 2;
                    } else {
                        i += 1;
                    }
                }
                batch_download(filter, amount, filters);
            } catch (const std::exception &) {
                std::cerr << "Error: Invalid amount '" << args[3] << "'.\n";
                catapi::cleanup();
                return 1;
            }
        } else {
            fetch_and_display_image(filter, upscale);
        }
    } else {
        std::cerr << "Error: Unknown command '" << command << "'.\n";
        std::cerr << "Run 'meow --help' for a list of available commands.\n";
        catapi::cleanup();
        return 1;
    }

    catapi::cleanup();
    return 0;
}
