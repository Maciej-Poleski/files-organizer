#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

std::string TwoDigits(int value) {
  std::string result = std::to_string(value);
  if (result.size() == 1) {
    return "0" + result;
  } else {
    return result;
  }
}

void MoveSubprocess(const std::filesystem::path &file,
                    const std::filesystem::path &destination) {
  std::string arg0 = "mv";
  std::string arg1 = file.native();
  std::string arg2 = destination.native();
  std::array<char *, 4> argv = {arg0.data(), arg1.data(), arg2.data(), nullptr};
  pid_t pid;
  int error = posix_spawnp(&pid, "mv", nullptr, nullptr, argv.data(), environ);
  if (error) {
    throw std::runtime_error(std::string("Move subprocess: ") +
                             strerror(error));
  }
  int stat;
  waitpid(pid, &stat, 0);
  if (WIFEXITED(stat)) {
    return;
  }
  throw std::runtime_error("Move exit status: " +
                           std::to_string(WEXITSTATUS(stat)));
}

void Move(const std::filesystem::path &file,
          const std::filesystem::path &destination) {
  std::error_code ec;
  rename(file, destination / file.filename(), ec);
  if (ec) {
    if (ec.value() == EXDEV) {
      MoveSubprocess(file, destination);
    } else {
      throw std::filesystem::filesystem_error(ec.message(), ec);
    }
  }
}

void PrepareMove(const std::filesystem::path &file,
                 std::filesystem::path destination) {
  auto time = std::chrono::system_clock::to_time_t(
      std::chrono::file_clock::to_sys(std::filesystem::last_write_time(file)));
  std::tm *tm = localtime(&time);
  destination /= std::to_string(tm->tm_year + 1900) + "_" +
                 TwoDigits(tm->tm_mon + 1) + "_" + TwoDigits(tm->tm_mday);
  std::filesystem::file_status destination_status =
      std::filesystem::status(destination);
  if (std::filesystem::exists(destination_status)) {
    if (!std::filesystem::is_directory(destination_status)) {
      throw std::runtime_error("Not a directory: " + destination.native());
    }
  } else {
    if (!std::filesystem::create_directory(destination)) {
      throw std::runtime_error("Unable to create directory: " +
                               destination.native());
    }
  }
  Move(file, destination);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << argv[0] << " [input directory] [output directory]\n";
  }
  const std::filesystem::path source = argv[1];
  const std::filesystem::path destination = argv[2];

  std::vector<std::filesystem::path> files;
  for (std::filesystem::directory_entry entry :
       std::filesystem::recursive_directory_iterator(source)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    files.push_back(entry);
  }
  for (const auto &path : files) {
    PrepareMove(path, destination);
  }
  return 0;
}
