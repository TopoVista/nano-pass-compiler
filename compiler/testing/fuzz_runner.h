#pragma once

#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>

#include "testing/differential_runner.h"
#include "testing/nano_generator.h"
#include "support/project_version.h"

namespace nanopass {

struct FuzzSummary { uint64_t seed=0; size_t generated=0, valid=0, rejected=0, compared=0, unsupported=0, mismatches=0, crashes=0, timeouts=0, distinctFailures=0, durationUs=0; size_t arithmetic=0, comparisons=0, conditionals=0, loops=0, functions=0, nestedCalls=0, mutableLocals=0, returns=0; };
inline std::string fuzzSummaryJson(const FuzzSummary &s) { return "{\"seed\":"+std::to_string(s.seed)+",\"generated\":"+std::to_string(s.generated)+",\"valid\":"+std::to_string(s.valid)+",\"rejected\":"+std::to_string(s.rejected)+",\"compared\":"+std::to_string(s.compared)+",\"unsupported\":"+std::to_string(s.unsupported)+",\"mismatches\":"+std::to_string(s.mismatches)+",\"crashes\":"+std::to_string(s.crashes)+",\"timeouts\":"+std::to_string(s.timeouts)+",\"distinct_failures\":"+std::to_string(s.distinctFailures)+",\"duration_us\":"+std::to_string(s.durationUs)+",\"features\":{\"arithmetic\":"+std::to_string(s.arithmetic)+",\"comparisons\":"+std::to_string(s.comparisons)+",\"conditionals\":"+std::to_string(s.conditionals)+",\"loops\":"+std::to_string(s.loops)+",\"functions\":"+std::to_string(s.functions)+",\"nested_calls\":"+std::to_string(s.nestedCalls)+",\"mutable_locals\":"+std::to_string(s.mutableLocals)+",\"returns\":"+std::to_string(s.returns)+"}}"; }
inline uint64_t fuzzHash(std::string_view input) { uint64_t hash=1469598103934665603ull;for(unsigned char c:input){hash^=c;hash*=1099511628211ull;}return hash; }

class DifferentialFuzzRunner {
public:
  FuzzSummary run(GeneratorOptions options, size_t cases, const std::filesystem::path &failureDirectory={}) const {
    DifferentialRunner differential;
    return runWithAdapter(options, cases, failureDirectory,
                          [&](const std::string &source) { return differential.run(source); });
  }
  using Adapter = std::function<DifferentialResult(const std::string &)>;
  FuzzSummary runWithAdapter(GeneratorOptions options, size_t cases,
                             const std::filesystem::path &failureDirectory,
                             const Adapter &adapter) const {
    const auto start=std::chrono::steady_clock::now(); NanoProgramGenerator generator(options); FuzzSummary summary;summary.seed=options.seed;std::set<uint64_t> failures;
    for(size_t index=0;index<cases;++index){auto generated=generator.generate(index);++summary.generated;countFeatures(summary,generated.features);auto result=adapter(generated.source);if(!result.compiled){++summary.rejected;continue;}++summary.valid;if(unsupported(result)){++summary.unsupported;continue;}if(limited(result)){++summary.timeouts;continue;}summary.compared+=result.levels.size();if(!result.equivalent){++summary.mismatches;failures.insert(fuzzHash(generated.source));persist(failureDirectory,options,generated,result);}}
    summary.distinctFailures=failures.size();
    summary.durationUs=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    return summary;
  }
private:
  static bool isLimit(ExecutionStatus status) { return status==ExecutionStatus::StepLimitExceeded||status==ExecutionStatus::TimeLimitExceeded||status==ExecutionStatus::OutputLimitExceeded||status==ExecutionStatus::RecursionLimitExceeded; }
  static bool unsupported(const DifferentialResult &result) { if(result.reference.status==ExecutionStatus::UnsupportedFeature)return true;return std::any_of(result.levels.begin(),result.levels.end(),[](const auto &level){return level.execution.status==ExecutionStatus::UnsupportedFeature;}); }
  static bool limited(const DifferentialResult &result) { if(isLimit(result.reference.status))return true;return std::any_of(result.levels.begin(),result.levels.end(),[](const auto &level){return isLimit(level.execution.status);}); }
  static void countFeatures(FuzzSummary &summary,const GeneratedFeatures &features) { summary.arithmetic+=features.arithmetic;summary.comparisons+=features.comparisons;summary.conditionals+=features.conditionals;summary.loops+=features.loops;summary.functions+=features.functions;summary.nestedCalls+=features.nestedCalls;summary.mutableLocals+=features.mutableLocals;summary.returns+=features.returns; }
  static std::string escape(std::string_view value) { std::string out;for(unsigned char character:value){switch(character){case '\\':out+="\\\\";break;case '"':out+="\\\"";break;case '\n':out+="\\n";break;case '\r':out+="\\r";break;case '\t':out+="\\t";break;default:if(character<0x20){const char hex[]="0123456789abcdef";out+="\\u00";out+=hex[character>>4];out+=hex[character&15];}else out+=static_cast<char>(character);}}return out; }
  static std::string executionJson(const ExecutionResult &execution) { std::string out="{\"status\":"+std::to_string(static_cast<int>(execution.status))+",\"stdout\":\""+escape(execution.stdoutText)+"\",\"stderr\":\""+escape(execution.stderrText)+"\",\"steps\":"+std::to_string(execution.stepsExecuted);if(execution.returnValue)out+=",\"return\":\""+escape(runtimeValueText(*execution.returnValue))+"\"";if(execution.error)out+=",\"error\":{\"category\":\""+escape(execution.error->category)+"\",\"message\":\""+escape(execution.error->message)+"\"}";return out+"}"; }
  static void persist(const std::filesystem::path &directory, const GeneratorOptions &options,const GeneratedProgram &program, const DifferentialResult &result) {
    if(directory.empty()) return;
    std::filesystem::create_directories(directory);auto id=std::to_string(fuzzHash(program.source));auto base=directory/("failure_"+id);std::ofstream(base.string()+".nano")<<program.source;std::ofstream metadata(base.string()+".json");metadata<<"{\"generation\":{\"seed\":"<<program.seed<<",\"case\":"<<program.caseIndex<<",\"max_depth\":"<<options.maxDepth<<",\"max_statements\":"<<options.maxStatements<<",\"max_functions\":"<<options.maxFunctions<<",\"max_call_depth\":"<<options.maxCallDepth<<",\"max_loop_nesting\":"<<options.maxLoopNesting<<",\"max_source_bytes\":"<<options.maxSourceBytes<<"},\"compiler_revision\":\""<<compilerRevision()<<"\",\"compiled\":"<<(result.compiled?"true":"false")<<",\"equivalent\":"<<(result.equivalent?"true":"false")<<",\"diagnostic\":\""<<escape(result.diagnostic)<<"\",\"reference\":"<<executionJson(result.reference)<<",\"levels\":[";for(size_t index=0;index<result.levels.size();++index){const auto &level=result.levels[index];if(index)metadata<<',';metadata<<"{\"level\":"<<level.level<<",\"pipeline\":\""<<escape(level.pipeline)<<"\",\"equivalent\":"<<(level.equivalent?"true":"false")<<",\"execution\":"<<executionJson(level.execution)<<"}";}metadata<<"]}";
  }
  static std::string compilerRevision() {
#ifdef NANOPASS_GIT_REVISION
    return NANOPASS_GIT_REVISION;
#else
    return std::string(build::kCompilerRevision);
#endif
  }
};

// Deterministic lexical-statement reducer. The caller owns the real
// parse/type-check/rebuild/execution predicate; invalid candidates therefore
// never replace a valid semantic mismatch.
class SourceReducer {
public:
  std::string reduce(std::string source, const std::function<bool(const std::string&)> &stillFails, size_t budget=128) const {
    size_t attempts=0;for(size_t start=0;start<source.size()&&attempts<budget;){auto end=source.find('\n',start);if(end==std::string::npos)end=source.size();auto candidate=source.substr(0,start)+source.substr(end==source.size()?end:end+1);++attempts;if(!candidate.empty()&&stillFails(candidate)){source=std::move(candidate);start=0;}else start=end==source.size()?end:end+1;}return source;
  }
};

// AST-guided reduction chooses only lines that begin parsed statements.  The
// legacy AST stores line/column locations rather than byte spans, so removal is
// line-granular; DifferentialReducer supplies the semantic rebuild gate below.
class AstAwareReducer {
  static std::vector<size_t> statementLines(const std::string &source) {
    std::set<size_t> lines;
    try {
      Lexer lexer(source); Parser parser(lexer.scanTokens()); auto program = parser.parseProgram();
      std::function<void(const Stmt *)> visit = [&](const Stmt *statement) {
        if (!statement) return;
        if (statement->loc.line > 0) lines.insert(static_cast<size_t>(statement->loc.line - 1));
        if (const auto *block = dynamic_cast<const BlockStmt *>(statement))
          for (const auto &child : block->stmts) visit(child.get());
        else if (const auto *function = dynamic_cast<const FunctionStmt *>(statement)) visit(function->body.get());
        else if (const auto *conditional = dynamic_cast<const IfStmt *>(statement)) { visit(conditional->thenBranch.get()); visit(conditional->elseBranch.get()); }
        else if (const auto *loop = dynamic_cast<const WhileStmt *>(statement)) visit(loop->body.get());
        else if (const auto *loop = dynamic_cast<const ForStmt *>(statement)) { visit(loop->init.get()); visit(loop->body.get()); }
      };
      for (const auto &statement : program) visit(statement.get());
    } catch (const std::exception &) {}
    return {lines.begin(), lines.end()};
  }
  static std::string removeLine(const std::string &source, size_t line) {
    size_t start = 0;
    for (size_t current=0; current<line && start<source.size(); ++current) {
      start = source.find('\n', start);
      if (start == std::string::npos) return source;
      ++start;
    }
    const auto end = source.find('\n', start);
    return source.substr(0, start) + source.substr(end == std::string::npos ? source.size() : end + 1);
  }
  static std::vector<std::pair<size_t,size_t>> integerRanges(const std::string &source) {
    std::vector<std::pair<size_t,size_t>> ranges;
    for(size_t index=0;index<source.size();) {
      if(!std::isdigit(static_cast<unsigned char>(source[index]))) { ++index; continue; }
      const size_t begin=index; while(index<source.size()&&std::isdigit(static_cast<unsigned char>(source[index])))++index;
      ranges.emplace_back(begin,index);
    }
    return ranges;
  }
public:
  std::string reduce(std::string source, const std::function<bool(const std::string &)> &stillFails,
                     size_t budget = 128) const {
    for (size_t attempt=0; attempt<budget; ++attempt) {
      bool changed = false;
      for (const auto line : statementLines(source)) {
        const auto candidate = removeLine(source, line);
        if (candidate != source && !candidate.empty() && stillFails(candidate)) {
          source = candidate; changed = true; break;
        }
      }
      if (!changed) for (const auto &[begin,end] : integerRanges(source)) {
        const auto candidate=source.substr(0,begin)+"0"+source.substr(end);
        if (candidate != source && stillFails(candidate)) { source=candidate;changed=true;break; }
      }
      if (!changed) break;
    }
    return source;
  }
};

// A semantic reducer layered on the source reducer.  Each candidate is run
// through the actual frontend and differential pipeline before it may be kept.
// It is intentionally separate from production optimization code so tests can
// inject a faulty adapter without corrupting real passes.
class DifferentialReducer {
public:
  using Adapter = std::function<DifferentialResult(const std::string &)>;
  std::string reduce(const std::string &source, const Adapter &adapter,
                     size_t budget = 128) const {
    const DifferentialResult baseline = adapter(source);
    if (!baseline.compiled || baseline.equivalent) return source;
    const auto signature = failureSignature(baseline);
    return AstAwareReducer{}.reduce(source, [&](const std::string &candidate) {
      const auto result = adapter(candidate);
      return result.compiled && !result.equivalent &&
             failureSignature(result) == signature;
    }, budget);
  }
private:
  static std::string failureSignature(const DifferentialResult &result) {
    if (!result.compiled) return "compile:" + result.diagnostic;
    for (const auto &level : result.levels) if (!level.equivalent) {
      const auto category = level.execution.error ? level.execution.error->category : "success";
      return "O" + std::to_string(level.level) + ":" +
             std::to_string(static_cast<int>(level.execution.status)) + ":" + category;
    }
    return "equivalent";
  }
};

struct MalformedFuzzSummary { uint64_t seed=0; size_t generated=0, rejected=0, accepted=0, crashes=0; };

// Malformed-input fuzzing is intentionally a separate category from semantic
// differential fuzzing: parser rejection is a successful expected outcome.
class MalformedInputFuzzRunner {
public:
  MalformedFuzzSummary run(uint64_t seed, size_t cases) const {
    static constexpr std::string_view alphabet = "@#$%^&*()[]{};,+-=/\\\"abc012 \n";
    std::mt19937_64 random(seed); MalformedFuzzSummary summary; summary.seed=seed;
    for (size_t index=0; index<cases; ++index) {
      std::string input; const size_t length = 1 + random() % 96;
      for (size_t character=0; character<length; ++character)
        input.push_back(alphabet[random() % alphabet.size()]);
      ++summary.generated;
      try {
        Lexer lexer(input); Parser parser(lexer.scanTokens()); (void)parser.parseProgram();
        ++summary.accepted;
      } catch (const std::exception &) {
        ++summary.rejected;
      } catch (...) {
        ++summary.crashes;
      }
    }
    return summary;
  }
};

} // namespace nanopass
