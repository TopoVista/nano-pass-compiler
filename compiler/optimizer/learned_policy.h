#pragma once

// A deliberately small, portable learned-policy format.  The model is not a
// transformation engine: it only estimates inline profitability for a
// candidate that the deterministic inliner has already proven legal.

#include <cmath>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "optimizer/optimization_policy.h"

namespace nanopass::opt {

struct InlinePolicyContextV1 {
  static constexpr uint64_t kSchemaVersion = 1;
  uint64_t estimatedInlineCost = 0;
  uint64_t callerInstructionCount = 0;
};

struct InlinePolicyDecisionV1 {
  bool inlineCall = false;
  double confidence = 0.0;
  std::string modelVersion;
  std::string detail;
};

class InlineProfitabilityPolicy {
public:
  virtual ~InlineProfitabilityPolicy() = default;
  virtual InlinePolicyDecisionV1 decide(const OptimizationFeaturesV1 &features,
                                        const InlinePolicyContextV1 &context) const = 0;
};

namespace detail {
inline std::optional<std::string> policyJsonString(std::string_view text,
                                                    std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const size_t keyStart = text.find(needle);
  if (keyStart == std::string_view::npos) return std::nullopt;
  const size_t colon = text.find(':', keyStart + needle.size());
  if (colon == std::string_view::npos) return std::nullopt;
  const size_t quote = text.find('"', colon + 1);
  if (quote == std::string_view::npos) return std::nullopt;
  const size_t end = text.find('"', quote + 1);
  if (end == std::string_view::npos) return std::nullopt;
  return std::string(text.substr(quote + 1, end - quote - 1));
}
inline std::optional<double> policyJsonNumber(std::string_view text,
                                              std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const size_t keyStart = text.find(needle);
  if (keyStart == std::string_view::npos) return std::nullopt;
  const size_t colon = text.find(':', keyStart + needle.size());
  if (colon == std::string_view::npos) return std::nullopt;
  size_t begin = colon + 1;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
  size_t end = begin;
  while (end < text.size() && (std::isdigit(static_cast<unsigned char>(text[end])) ||
      text[end] == '-' || text[end] == '+' || text[end] == '.' || text[end] == 'e' || text[end] == 'E')) ++end;
  try {
    size_t used = 0;
    const double value = std::stod(std::string(text.substr(begin, end - begin)), &used);
    if (used != end - begin || !std::isfinite(value)) return std::nullopt;
    return value;
  } catch (...) { return std::nullopt; }
}
inline std::optional<std::vector<double>> policyJsonNumbers(std::string_view text,
                                                            std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const size_t keyStart = text.find(needle);
  if (keyStart == std::string_view::npos) return std::nullopt;
  const size_t open = text.find('[', keyStart + needle.size());
  const size_t close = open == std::string_view::npos ? std::string_view::npos : text.find(']', open + 1);
  if (open == std::string_view::npos || close == std::string_view::npos) return std::nullopt;
  std::vector<double> values;
  std::stringstream input(std::string(text.substr(open + 1, close - open - 1)));
  std::string token;
  while (std::getline(input, token, ',')) {
    try {
      size_t used = 0;
      const double value = std::stod(token, &used);
      while (used < token.size() && std::isspace(static_cast<unsigned char>(token[used]))) ++used;
      if (used != token.size() || !std::isfinite(value)) return std::nullopt;
      values.push_back(value);
    } catch (...) { return std::nullopt; }
  }
  return values;
}
inline std::optional<std::vector<std::string>> policyJsonStrings(std::string_view text,
                                                                   std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const size_t keyStart = text.find(needle);
  if (keyStart == std::string_view::npos) return std::nullopt;
  const size_t open = text.find('[', keyStart + needle.size());
  const size_t close = open == std::string_view::npos ? std::string_view::npos : text.find(']', open + 1);
  if (open == std::string_view::npos || close == std::string_view::npos) return std::nullopt;
  std::vector<std::string> values;
  size_t cursor = open + 1;
  while (cursor < close) {
    while (cursor < close && (std::isspace(static_cast<unsigned char>(text[cursor])) || text[cursor] == ',')) ++cursor;
    if (cursor == close) break;
    if (text[cursor++] != '"') return std::nullopt;
    const size_t end = text.find('"', cursor);
    if (end == std::string_view::npos || end > close) return std::nullopt;
    values.emplace_back(text.substr(cursor, end - cursor));
    cursor = end + 1;
  }
  return values;
}
} // namespace detail

class LearnedInlinePolicy final : public InlineProfitabilityPolicy {
public:
  static constexpr std::string_view kModelKind = "logistic-regression";
  static constexpr std::string_view kDefaultModelVersion = "inline-imitation-v1";
  static inline const std::vector<std::string> kFeatureNames{
      "instruction_count", "block_count", "dynamic_call_count",
      "callee_count", "estimated_inline_cost"};

  static std::optional<LearnedInlinePolicy> loadJson(std::istream &input,
                                                     std::string &error) {
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    const auto kind = detail::policyJsonString(text, "model_kind");
    const auto version = detail::policyJsonString(text, "model_version");
    const auto schema = detail::policyJsonNumber(text, "feature_schema_version");
    const auto transform = detail::policyJsonString(text, "feature_transform");
    const auto names = detail::policyJsonStrings(text, "features");
    const auto intercept = detail::policyJsonNumber(text, "intercept");
    const auto coefficients = detail::policyJsonNumbers(text, "coefficients");
    const auto threshold = detail::policyJsonNumber(text, "threshold");
    if (!kind || *kind != kModelKind || !version || !schema || !transform || !names || !intercept || !coefficients || !threshold) {
      error = "model is missing a required learned-inline field"; return std::nullopt;
    }
    if (*schema != static_cast<double>(OptimizationFeaturesV1::kSchemaVersion) || *transform != "log1p" ||
        *names != kFeatureNames || coefficients->size() != kFeatureNames.size() ||
        *threshold < 0.0 || *threshold > 1.0) {
      error = "model feature schema is incompatible with learned-inline-v1"; return std::nullopt;
    }
    LearnedInlinePolicy model;
    model.modelVersion_ = *version;
    model.intercept_ = *intercept;
    model.coefficients_ = std::move(*coefficients);
    model.threshold_ = *threshold;
    return model;
  }
  static std::optional<LearnedInlinePolicy> loadFile(const std::string &path,
                                                      std::string &error) {
    std::ifstream input(path);
    if (!input) { error = "could not open learned policy model '" + path + "'"; return std::nullopt; }
    return loadJson(input, error);
  }
  // The service never accepts a model path from HTTP clients.  Its one
  // allowlisted policy is this reviewed, generated coefficient table, kept in
  // sync with models/inline-imitation-v1.json by tools/train_policy.py.
  static LearnedInlinePolicy builtinInlineImitationV1() {
    LearnedInlinePolicy model;
    model.modelVersion_ = "inline-imitation-v1";
    model.intercept_ = 3.7265650812435824;
    model.coefficients_ = {-1.1874353499147114, -1.4034442296632523,
                           0.0, 0.34822259348750323, -0.2514083521080062};
    model.threshold_ = 0.5;
    return model;
  }

  InlinePolicyDecisionV1 decide(const OptimizationFeaturesV1 &features,
                                const InlinePolicyContextV1 &context) const override {
    const std::vector<double> input{
        std::log1p(static_cast<double>(features.instructionCount)),
        std::log1p(static_cast<double>(features.blockCount)),
        std::log1p(static_cast<double>(features.dynamicCallCount)),
        std::log1p(static_cast<double>(features.calleeCount)),
        std::log1p(static_cast<double>(context.estimatedInlineCost))};
    double score = intercept_;
    for (size_t index = 0; index < coefficients_.size(); ++index) score += coefficients_[index] * input[index];
    const double probability = score >= 0 ? 1.0 / (1.0 + std::exp(-score))
                                          : std::exp(score) / (1.0 + std::exp(score));
    return {probability >= threshold_, probability, modelVersion_,
            probability >= threshold_ ? "model approved legal inline candidate" : "model declined legal inline candidate"};
  }
  const std::string &modelVersion() const noexcept { return modelVersion_; }
  double threshold() const noexcept { return threshold_; }

private:
  std::string modelVersion_ = std::string(kDefaultModelVersion);
  double intercept_ = 0.0;
  std::vector<double> coefficients_;
  double threshold_ = 0.5;
};

} // namespace nanopass::opt
