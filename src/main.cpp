//
// Created by liu on 18-10-21.
//

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include "db/Database.h"
#include "query/Query.h"
#include "scheduler/InputReader.h"
#include "scheduler/OutputBuffer.h"
#include "scheduler/QueryDispatcher.h"
#include "scheduler/QueryManager.h"
#include "scheduler/QueryTypes.h"
#include "threads/CompletionQueue.h"
#include "threads/QueryRunner.h"
#include "threads/TaskQueue.h"
#include "threads/ThreadPoolManager.h"

namespace {
using std::array;

struct ParsedArgs {
  std::string listen;
  std::int64_t threads = 0;
};

auto parseArgs(int argc, char **argv) -> ParsedArgs {
  ParsedArgs parsedArgs;
  constexpr int kDecimalBase = 10;
  const array<option, 3> longOpts = {
      {
          {.name = "listen",
           .has_arg = required_argument,
           .flag = nullptr,
           .val = 'l'},
          {.name = "threads",
           .has_arg = required_argument,
           .flag = nullptr,
           .val = 't'},
          {.name = nullptr, .has_arg = no_argument, .flag = nullptr, .val = 0},
      },
  };
  const char *shortOpts = "l:t:";
  int opt = 0;
  int longIndex = 0;
  while ((opt = getopt_long(argc, argv, shortOpts, longOpts.data(),
                            &longIndex)) != -1) {
    if (opt == 'l') {
      parsedArgs.listen = optarg;
    } else if (opt == 't') {
      parsedArgs.threads = std::strtol(optarg, nullptr, kDecimalBase);
    } else {
      std::cerr << "mangoDB: warning: unknown argument "
                << longOpts.at(static_cast<std::size_t>(longIndex)).name
                << '\n';
    }
  }
  return parsedArgs;
}

void executeDirect(Query::Ptr query, OutputBuffer &output) {
  output.accept(query->getId(), QueryRunner::executeQuery(*query));
  output.flushConsecutive();
}

void singlethread_run(InputReader &reader, OutputBuffer &output) {
  bool inputClosed = false;
  while (!inputClosed) {
    // Parse after the preceding Query finishes, including its file changes.
    auto batch = reader.readQueries(1);
    for (auto &query : batch.queries) {
      executeDirect(std::move(query), output);
    }
    inputClosed = batch.inputClosed;
  }
}

void runQueries(std::istream const &input, std::size_t threadLimit) {
  if (threadLimit == 1) {
    InputReader reader(input);
    OutputBuffer output;
    singlethread_run(reader, output);
    return;
  }
  auto reader = std::make_unique<InputReader>(input);
  auto output = std::make_unique<OutputBuffer>();
  constexpr std::size_t kBatchQueryLimit = 32;
  constexpr std::size_t kOutstandingQueriesPerWorker = 64;
  constexpr std::size_t kRowsPerTask = 4096;
  const QueryManager::Limits limits{
      .batchQueryLimit = kBatchQueryLimit,
      .queryHighWaterMark = kOutstandingQueriesPerWorker * threadLimit,
  };
  Database::getInstance();
  TaskQueue tasks;
  CompletionQueue completions;
  ThreadPoolManager pool(
      {.workerLimit = threadLimit,
       .split = {.rowsPerTask = kRowsPerTask, .enabled = true}},
      tasks, completions);
  QueryManager manager(std::move(reader), std::make_unique<QueryDispatcher>(),
                       std::move(output), pool, limits);
  manager.run();
}
}  // namespace

auto main(int argc, char *argv[]) -> int {
  // Assume only C++ style I/O input used in mangoDB
  // Do not use printf/fprintf in <cstdio> with this line
  std::ios_base::sync_with_stdio(false);

  const auto parsedArgs = parseArgs(argc, argv);

  std::ifstream fin;
  if (!parsedArgs.listen.empty()) {
    fin.open(parsedArgs.listen);
    if (!fin.is_open()) {
      std::cerr << "mangoDB: error: " << parsedArgs.listen
                << ": no such file or directory" << '\n';
      return -1;
    }
  }
  std::istream input(fin.rdbuf());

#ifdef NDEBUG
  // In production mode, listen argument must be defined
  if (parsedArgs.listen.empty()) {
    std::cerr << "mangoDB: error: --listen argument not found, not allowed in "
                 "production mode"
              << '\n';
    return -1;
  }
#else
  // In debug mode, use stdin as input if no listen file input found
  if (parsedArgs.listen.empty()) {
    std::cerr << "mangoDB: warning: --listen argument not found, use stdin "
                 "instead in debug mode"
              << '\n';
    input.rdbuf(std::cin.rdbuf());
  }
#endif

  if (parsedArgs.threads < 0) {
    std::cerr << "mangoDB: error: threads num can not be negative value "
              << parsedArgs.threads << '\n';
    return -1;
  }
  if (parsedArgs.threads == 0) {
    // @TODO Auto detect the thread num
    std::cerr << "mangoDB: info: auto detect thread num" << '\n';
  } else {
    std::cerr << "mangoDB: info: requested up to " << parsedArgs.threads
              << " threads" << '\n';
  }

  try {
    const auto threadLimit =
        parsedArgs.threads == 0
            ? std::max<std::size_t>(1, std::thread::hardware_concurrency())
            : static_cast<std::size_t>(parsedArgs.threads);
    runQueries(input, threadLimit);
  } catch (const std::exception &error) {
    std::cerr << "mangoDB: error: " << error.what() << '\n';
    return -1;
  }

  return 0;
}
