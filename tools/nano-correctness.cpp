#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>

#include "interpreter/core_interpreter.h"
#include "testing/differential_runner.h"
#include "testing/fuzz_runner.h"

using namespace nanopass;

static std::string readFile(const char *path) { std::ifstream in(path);std::ostringstream out;out<<in.rdbuf();return out.str(); }
static std::vector<std::string> splitPasses(std::string text) { std::vector<std::string> passes;std::stringstream stream(text);for(std::string item;std::getline(stream,item,',');)if(!item.empty())passes.push_back(std::move(item));return passes; }
static bool optionValue(std::string_view argument, std::string_view name, std::string &value) {
  const std::string prefix = std::string(name) + "=";
  if (!argument.starts_with(prefix)) return false;
  value = std::string(argument.substr(prefix.size()));
  if (value.empty()) throw std::runtime_error("missing value for " + std::string(name));
  return true;
}

struct FuzzCommandOptions {
  GeneratorOptions generator;
  size_t cases = 1000;
  std::filesystem::path failures;
};

static FuzzCommandOptions parseFuzzOptions(int argc, char **argv, int first) {
  FuzzCommandOptions parsed;
  parsed.generator.maxDepth = 3;
  parsed.generator.maxStatements = 8;
  size_t positional = 0;
  for (int index = first; index < argc; ++index) {
    const std::string_view argument = argv[index];
    std::string value;
    if (optionValue(argument, "--seed", value)) parsed.generator.seed = std::stoull(value);
    else if (optionValue(argument, "--cases", value)) parsed.cases = std::stoull(value);
    else if (optionValue(argument, "--max-depth", value)) parsed.generator.maxDepth = std::stoull(value);
    else if (optionValue(argument, "--max-statements", value)) parsed.generator.maxStatements = std::stoull(value);
    else if (optionValue(argument, "--max-functions", value)) parsed.generator.maxFunctions = std::stoull(value);
    else if (optionValue(argument, "--max-call-depth", value)) parsed.generator.maxCallDepth = std::stoull(value);
    else if (optionValue(argument, "--max-loop-nesting", value)) parsed.generator.maxLoopNesting = std::stoull(value);
    else if (optionValue(argument, "--max-source-bytes", value)) parsed.generator.maxSourceBytes = std::stoull(value);
    else if (optionValue(argument, "--save-failures", value)) parsed.failures = value;
    else if (argument.starts_with("--")) throw std::runtime_error("unknown fuzz option: " + std::string(argument));
    else if (positional++ == 0) parsed.generator.seed = std::stoull(std::string(argument));
    else if (positional == 2) parsed.cases = std::stoull(std::string(argument));
    else if (positional == 3) parsed.failures = std::string(argument);
    else throw std::runtime_error("too many fuzz arguments");
  }
  return parsed;
}

struct MalformedCommandOptions { uint64_t seed = 42; size_t cases = 1000; };
static MalformedCommandOptions parseMalformedOptions(int argc, char **argv, int first) {
  MalformedCommandOptions parsed;
  size_t positional = 0;
  for (int index = first; index < argc; ++index) {
    const std::string_view argument = argv[index];
    std::string value;
    if (optionValue(argument, "--seed", value)) parsed.seed = std::stoull(value);
    else if (optionValue(argument, "--cases", value)) parsed.cases = std::stoull(value);
    else if (argument.starts_with("--")) throw std::runtime_error("unknown malformed-fuzz option: " + std::string(argument));
    else if (positional++ == 0) parsed.seed = std::stoull(std::string(argument));
    else if (positional == 2) parsed.cases = std::stoull(std::string(argument));
    else throw std::runtime_error("too many malformed-fuzz arguments");
  }
  return parsed;
}
int main(int argc,char **argv) {
  if(argc<2){std::cerr<<"usage: nano-correctness interpret|differential <file> | differential-custom <file> <pass,...> | fuzz [--seed=N] [--cases=N] [--max-depth=N] [--max-statements=N] [--max-functions=N] [--max-call-depth=N] [--max-loop-nesting=N] [--max-source-bytes=N] [--save-failures=dir] | malformed-fuzz [--seed=N] [--cases=N] | generate [seed] [case]\n";return 2;}
  try {
    const std::string command=argv[1];
    if(command=="generate"){GeneratorOptions options;options.seed=argc>2?std::stoull(argv[2]):42;options.maxDepth=3;std::cout<<NanoProgramGenerator(options).generate(argc>3?std::stoull(argv[3]):0).source;return 0;}
    if(command=="fuzz"){const auto options=parseFuzzOptions(argc,argv,2);auto summary=DifferentialFuzzRunner{}.run(options.generator,options.cases,options.failures);std::cout<<fuzzSummaryJson(summary)<<"\n";return summary.mismatches||summary.crashes?1:0;}
    if(command=="malformed-fuzz"){const auto options=parseMalformedOptions(argc,argv,2);auto summary=MalformedInputFuzzRunner{}.run(options.seed,options.cases);std::cout<<"{\"seed\":"<<summary.seed<<",\"generated\":"<<summary.generated<<",\"rejected\":"<<summary.rejected<<",\"accepted\":"<<summary.accepted<<",\"crashes\":"<<summary.crashes<<"}\n";return summary.crashes?1:0;}
    if(argc<3){std::cerr<<"missing source file\n";return 2;}auto source=readFile(argv[2]);
    if(command=="interpret"){auto ir=buildIRForDifferential(source);auto executed=CoreInterpreter{}.run(ir.core);std::cout<<executed.stdoutText;if(executed.returnValue)std::cout<<"return "<<runtimeValueText(*executed.returnValue)<<"\n";return executed.status==ExecutionStatus::Success?0:1;}
    if(command=="differential"){auto result=DifferentialRunner{}.run(source);if(!result.compiled){std::cerr<<result.diagnostic<<"\n";return 1;}for(const auto&level:result.levels)std::cout<<"O"<<level.level<<": "<<(level.equivalent?"PASS":"FAIL")<<"\n";return result.equivalent?0:1;}
    if(command=="differential-custom"){if(argc<4){std::cerr<<"missing custom pass list\n";return 2;}auto result=DifferentialRunner{}.runCustom(source,{"custom",splitPasses(argv[3])});if(!result.compiled){std::cerr<<result.diagnostic<<"\n";return 1;}std::cout<<"custom: "<<(result.equivalent?"PASS":"FAIL")<<"\n";return result.equivalent?0:1;}
    std::cerr<<"unknown command\n";return 2;
  } catch(const std::exception&error){std::cerr<<error.what()<<"\n";return 1;}
}
