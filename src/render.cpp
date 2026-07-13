#include "render.hpp"
#include "terminal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {

// Forks `cmd` with `args`, writes `bytes` to its stdin, and (if
// capture_stdout) reads its stdout back into `out`. Returns true if the
// child was spawned and exited successfully.
bool run_viewer(const std::string &cmd, const std::vector<std::string> &args,
                 const std::vector<uint8_t> &bytes, bool capture_stdout,
                 std::vector<uint8_t> &out) {
    int in_pipe[2];
    int out_pipe[2] = {-1, -1};
    if (pipe(in_pipe) != 0) return false;
    if (capture_stdout && pipe(out_pipe) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        if (capture_stdout) { close(out_pipe[0]); close(out_pipe[1]); }
        return false;
    }

    if (pid == 0) {
        // Child
        dup2(in_pipe[0], STDIN_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        if (capture_stdout) {
            dup2(out_pipe[1], STDOUT_FILENO);
            close(out_pipe[0]);
            close(out_pipe[1]);
        }

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(cmd.c_str()));
        for (const auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);

        execvp(cmd.c_str(), argv.data());
        _exit(127); // cmd not found or exec failed
    }

    // Parent
    close(in_pipe[0]);
    if (capture_stdout) close(out_pipe[1]);

    std::thread writer([&bytes, fd = in_pipe[1]]() {
        size_t written = 0;
        while (written < bytes.size()) {
            ssize_t n = write(fd, bytes.data() + written, bytes.size() - written);
            if (n <= 0) break;
            written += static_cast<size_t>(n);
        }
        close(fd);
    });

    if (capture_stdout) {
        uint8_t buf[65536];
        ssize_t n;
        while ((n = read(out_pipe[0], buf, sizeof(buf))) > 0) {
            out.insert(out.end(), buf, buf + n);
        }
        close(out_pipe[0]);
    }

    writer.join();

    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

namespace render {

bool render_image(const std::vector<uint8_t> &bytes, int cols, int rows, bool interactive, bool upscale) {
    std::string title = "--meow-cli--";
    int title_padding = cols > static_cast<int>(title.size()) ? (cols - static_cast<int>(title.size())) / 2 : 0;
    term::move_to_column(title_padding);
    std::printf("%s\n\n", title.c_str());
    std::fflush(stdout);

    std::string h_val = std::to_string(rows > 4 ? rows - 4 : rows);
    std::string w_val = std::to_string(cols);
    std::string place_val = w_val + "x" + h_val + "@0x1";
    std::string chafa_size = w_val + "x" + h_val;

    std::vector<std::string> kitty_args = {"+kitten", "icat", "--stdin", "yes"};
    if (upscale) kitty_args.push_back("--scale-up");
    if (interactive) {
        kitty_args.push_back("--place");
        kitty_args.push_back(place_val);
    }

    struct Viewer {
        std::string cmd;
        std::vector<std::string> args;
        bool capture_stdout;
    };

    std::vector<Viewer> viewers = {
        {"kitty", kitty_args, false},
        {"wezterm", {"imgcat", "--width", w_val, "--height", h_val}, true},
        {"viu", {"-w", w_val, "-h", h_val, "-"}, true},
        {"chafa", {"--size", chafa_size, "-"}, true},
    };

    for (const auto &v : viewers) {
        std::vector<uint8_t> out;
        bool ok = run_viewer(v.cmd, v.args, bytes, v.capture_stdout, out);
        if (!ok) continue;

        if (v.capture_stdout) {
            if (out.empty()) continue;
            if (interactive) term::move_to(0, 2);
            fwrite(out.data(), 1, out.size(), stdout);
            std::fflush(stdout);
        }
        return true;
    }
    return false;
}

} // namespace render
