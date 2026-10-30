#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "interpreter/core_interpreter.h"
#include "testing/differential_runner.h"
#include "testing/fuzz_runner.h"

using namespace nanopass;

static std::string readFile(const char *path) { std::ifstream in(path);std::ostringstream out;out<<in.rdbuf();return out.str(); }
static std::vector<std::string> splitPasses(std::string text) { std::vector<std::string> passes;std::stringstream stream(text);for(std::string item;std::getline(stream,item,',');)if(!item.empty())passes.push_back(std::move(item));return passes; }
int main(int argc,char **argv) {
  if(argc<2){std::cerr<<"usage: nano-correctness interpret|differential <file> | differential-custom <file> <pass,...> | fuzz [seed] [cases] [save-dir] | malformed-fuzz [seed] [cases] | generate [seed] [case]\n";return 2;}
  try {
    const std::string command=argv[1];
    if(command=="generate"){GeneratorOptions options;options.seed=argc>2?std::stoull(argv[2]):42;options.maxDepth=3;std::cout<<NanoProgramGenerator(options).generate(argc>3?std::stoull(argv[3]):0).source;return 0;}
    if(command=="fuzz"){GeneratorOptions options;options.seed=argc>2?std::stoull(argv[2]):42;options.maxDepth=3;options.maxStatements=8;const size_t cases=argc>3?std::stoull(argv[3]):1000;const std::filesystem::path failures=argc>4?argv[4]:"";auto summary=DifferentialFuzzRunner{}.run(options,cases,failures);std::cout<<fuzzSummaryJson(summary)<<"\n";return summary.mismatches||summary.crashes?1:0;}
    if(command=="malformed-fuzz"){const uint64_t seed=argc>2?std::stoull(argv[2]):42;const size_t cases=argc>3?std::stoull(argv[3]):1000;auto summary=MalformedInputFuzzRunner{}.run(seed,cases);std::cout<<"{\"seed\":"<<summary.seed<<",\"generated\":"<<summary.generated<<",\"rejected\":"<<summary.rejected<<",\"accepted\":"<<summary.accepted<<",\"crashes\":"<<summary.crashes<<"}\n";return summary.crashes?1:0;}
    if(argc<3){std::cerr<<"missing source file\n";return 2;}auto source=readFile(argv[2]);
    if(command=="interpret"){auto ir=buildIRForDifferential(source);auto executed=CoreInterpreter{}.run(ir.core);std::cout<<executed.stdoutText;if(executed.returnValue)std::cout<<"return "<<runtimeValueText(*executed.returnValue)<<"\n";return executed.status==ExecutionStatus::Success?0:1;}
    if(command=="differential"){auto result=DifferentialRunner{}.run(source);if(!result.compiled){std::cerr<<result.diagnostic<<"\n";return 1;}for(const auto&level:result.levels)std::cout<<"O"<<level.level<<": "<<(level.equivalent?"PASS":"FAIL")<<"\n";return result.equivalent?0:1;}
    if(command=="differential-custom"){if(argc<4){std::cerr<<"missing custom pass list\n";return 2;}auto result=DifferentialRunner{}.runCustom(source,{"custom",splitPasses(argv[3])});if(!result.compiled){std::cerr<<result.diagnostic<<"\n";return 1;}std::cout<<"custom: "<<(result.equivalent?"PASS":"FAIL")<<"\n";return result.equivalent?0:1;}
    std::cerr<<"unknown command\n";return 2;
  } catch(const std::exception&error){std::cerr<<error.what()<<"\n";return 1;}
}
