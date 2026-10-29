#pragma once

#include <filesystem>
#include <fstream>
#include <functional>

#include "testing/differential_runner.h"
#include "testing/nano_generator.h"

namespace nanopass {

struct FuzzSummary { uint64_t seed=0; size_t generated=0, valid=0, compared=0, unsupported=0, mismatches=0, crashes=0, timeouts=0; };
inline std::string fuzzSummaryJson(const FuzzSummary &s) { return "{\"seed\":"+std::to_string(s.seed)+",\"generated\":"+std::to_string(s.generated)+",\"valid\":"+std::to_string(s.valid)+",\"compared\":"+std::to_string(s.compared)+",\"unsupported\":"+std::to_string(s.unsupported)+",\"mismatches\":"+std::to_string(s.mismatches)+",\"crashes\":"+std::to_string(s.crashes)+",\"timeouts\":"+std::to_string(s.timeouts)+"}"; }
inline uint64_t fuzzHash(std::string_view input) { uint64_t hash=1469598103934665603ull;for(unsigned char c:input){hash^=c;hash*=1099511628211ull;}return hash; }

class DifferentialFuzzRunner {
public:
  FuzzSummary run(GeneratorOptions options, size_t cases, const std::filesystem::path &failureDirectory={}) const {
    NanoProgramGenerator generator(options); DifferentialRunner differential; FuzzSummary summary;summary.seed=options.seed;
    for(size_t index=0;index<cases;++index){auto generated=generator.generate(index);++summary.generated;auto result=differential.run(generated.source);if(!result.compiled){++summary.crashes;persist(failureDirectory,generated,result);continue;}++summary.valid;if(result.reference.status==ExecutionStatus::UnsupportedFeature){++summary.unsupported;continue;}summary.compared+=result.levels.size();if(!result.equivalent){++summary.mismatches;persist(failureDirectory,generated,result);}}
    return summary;
  }
private:
  static void persist(const std::filesystem::path &directory, const GeneratedProgram &program, const DifferentialResult &result) {
    if(directory.empty())return;std::filesystem::create_directories(directory);auto id=std::to_string(fuzzHash(program.source));auto base=directory/("failure_"+id);std::ofstream(base.string()+".nano")<<program.source;std::ofstream metadata(base.string()+".json");metadata<<"{\"seed\":"<<program.seed<<",\"case\":"<<program.caseIndex<<",\"compiled\":"<<(result.compiled?"true":"false")<<",\"equivalent\":"<<(result.equivalent?"true":"false")<<",\"diagnostic\":\""<<result.diagnostic<<"\"}";
  }
};

// Deterministic line-oriented source reducer.  The predicate owns real
// reparsing/re-execution; invalid candidates therefore never replace a valid
// semantic mismatch unless the caller explicitly elects to accept them.
class SourceReducer {
public:
  std::string reduce(std::string source, const std::function<bool(const std::string&)> &stillFails, size_t budget=128) const {
    size_t attempts=0;for(size_t start=0;start<source.size()&&attempts<budget;){auto end=source.find('\n',start);if(end==std::string::npos)end=source.size();auto candidate=source.substr(0,start)+source.substr(end==source.size()?end:end+1);++attempts;if(!candidate.empty()&&stillFails(candidate)){source=std::move(candidate);start=0;}else start=end==source.size()?end:end+1;}return source;
  }
};

} // namespace nanopass
