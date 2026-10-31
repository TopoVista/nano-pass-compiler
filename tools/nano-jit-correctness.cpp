#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "backend/llvm/ssa_llvm_jit.h"
#include "interpreter/core_interpreter.h"
#include "ir/pipeline.h"
#include "lexer/lexer.h"
#include "optimizer/ssa_optimizer.h"
#include "parser/parser.h"
#include "sema/resolve_scopes.h"
#include "sema/type_check.h"
#include "testing/nano_generator.h"

using namespace nanopass;

namespace {

struct Options {
  GeneratorOptions generator{};
  size_t startCase = 0;
  size_t cases = 1000;
  std::filesystem::path failureDirectory;
};

bool optionValue(std::string_view argument, std::string_view name,
                 std::string &value) {
  const std::string prefix = std::string(name) + "=";
  if (!argument.starts_with(prefix)) return false;
  value = std::string(argument.substr(prefix.size()));
  if (value.empty()) throw std::runtime_error("missing value for " + std::string(name));
  return true;
}

Options parseOptions(int argc, char **argv) {
  Options options;
  options.generator.maxDepth = 3;
  options.generator.maxStatements = 8;
  for (int index = 2; index < argc; ++index) {
    const std::string_view argument = argv[index];
    std::string value;
    if (optionValue(argument, "--seed", value)) options.generator.seed = std::stoull(value);
    else if (optionValue(argument, "--start-case", value)) options.startCase = std::stoull(value);
    else if (optionValue(argument, "--cases", value)) options.cases = std::stoull(value);
    else if (optionValue(argument, "--max-depth", value)) options.generator.maxDepth = std::stoull(value);
    else if (optionValue(argument, "--max-statements", value)) options.generator.maxStatements = std::stoull(value);
    else if (optionValue(argument, "--max-functions", value)) options.generator.maxFunctions = std::stoull(value);
    else if (optionValue(argument, "--max-call-depth", value)) options.generator.maxCallDepth = std::stoull(value);
    else if (optionValue(argument, "--max-loop-nesting", value)) options.generator.maxLoopNesting = std::stoull(value);
    else if (optionValue(argument, "--max-source-bytes", value)) options.generator.maxSourceBytes = std::stoull(value);
    else if (optionValue(argument, "--save-failures", value)) options.failureDirectory = value;
    else throw std::runtime_error("unknown jit-fuzz option: " + std::string(argument));
  }
  return options;
}

IRPipelineResult compilePipeline(const std::string &source) {
  Lexer lexer(source);
  Parser parser(lexer.scanTokens());
  auto program = parser.parseProgram();
  ResolveScopesPass resolver;
  resolver.resolve(program);
  TypeCheckPass checker;
  checker.check(program);
  return IRPipeline::runAll(program);
}

void saveFailure(const Options &options, size_t caseIndex, const std::string &source) {
  if (options.failureDirectory.empty()) return;
  std::filesystem::create_directories(options.failureDirectory);
  std::ofstream out(options.failureDirectory / ("jit-seed-" +
      std::to_string(options.generator.seed) + "-case-" + std::to_string(caseIndex) + ".nano"));
  out << source;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2 || std::string_view(argv[1]) != "fuzz") {
    std::cerr << "usage: nano-jit-correctness fuzz [--seed=N] [--start-case=N] [--cases=N] "
                 "[--max-depth=N] [--max-statements=N] [--max-functions=N] "
                 "[--max-call-depth=N] [--max-loop-nesting=N] "
                 "[--max-source-bytes=N] [--save-failures=dir]\n";
    return 2;
  }
  try {
    const Options options = parseOptions(argc, argv);
    NanoProgramGenerator generator(options.generator);
    size_t compared = 0;
    for (size_t offset = 0; offset < options.cases; ++offset) {
      const size_t caseIndex = options.startCase + offset;
      const auto generated = generator.generate(caseIndex);
      try {
        auto pipeline = compilePipeline(generated.source);
        const auto oracle = CoreInterpreter{}.run(pipeline.core);
        if (oracle.status != ExecutionStatus::Success || !oracle.returnValue)
          throw std::runtime_error("CoreIR reference execution did not succeed");
        for (auto &function : pipeline.ssa.functions) {
          nanopass::opt::SSAOptimizationContext context;
          nanopass::opt::buildPipeline(3).run(function, context);
          const auto verification = verifySSA(function);
          if (!verification.ok) throw std::runtime_error("optimized SSA verification failed: " + verification.errors.front());
        }
        const auto native = llvm_backend::executeWithLLJIT(pipeline.ssa);
        const auto expectedReturn = std::stoll(runtimeValueText(*oracle.returnValue));
        if (!native.error.empty() || native.stdoutText != oracle.stdoutText ||
            native.returnValue != expectedReturn) {
          std::ostringstream error;
          error << "native mismatch";
          if (!native.error.empty()) error << ": " << native.error;
          else error << " (expected return=" << expectedReturn
                     << ", native return=" << native.returnValue << ')';
          throw std::runtime_error(error.str());
        }
      } catch (const std::exception &error) {
        saveFailure(options, caseIndex, generated.source);
        std::cerr << "jit-fuzz failure: seed=" << options.generator.seed
                  << " case=" << caseIndex << ": " << error.what() << "\n"
                  << generated.source;
        return 1;
      }
      ++compared;
    }
    std::cout << "{\"seed\":" << options.generator.seed
              << ",\"generated\":" << options.cases
              << ",\"startCase\":" << options.startCase
              << ",\"compared\":" << compared
              << ",\"optimization\":\"O3\""
              << ",\"mismatches\":0}\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
