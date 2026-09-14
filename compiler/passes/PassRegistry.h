#pragma once
// compiler/passes/PassRegistry.h
//
// Static registry of all known compiler passes and analyses.
//
// Design:
//   Passes and analyses self-register via PassRegistry::registerPass() and
//   PassRegistry::registerAnalysis() called from PipelineBuilder (rather than
//   using static-initialiser tricks which have fragile ordering on MSVC/MinGW).
//   The registry is populated once at program start and then read-only.
//
// Usage — querying:
//   PassRegistry::get().printPasses(std::cout);
//   PassRegistry::get().printAnalyses(std::cout);
//   const PassEntry *e = PassRegistry::get().lookupPass("desugar-for");
//
// Usage — validation:
//   if (!PassRegistry::get().isKnownPass("foo"))
//       error("unknown pass 'foo'");
//
// This registry is used by:
//   - `nano passes` CLI command
//   - `nano analyses` CLI command
//   - --stop-after / --disable-pass validation
//   - future: AI advisor allowlist, JSON API

#include <algorithm>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "PassInfo.h"

namespace nanopass {

// ── Analysis entry (mirrors PassInfo but for analyses) ────────────────────

struct AnalysisEntry {
    std::string id;          // "ast-node-count"
    std::string name;        // "NodeCount"
    std::string description; // One-line description
    std::string category;    // "ast"  |  "cfg"  |  "ssa"  (future)
    IRUnitKind  unit;
};

// ── Pass entry (owned copy of PassInfo + mutable description for registry) ─

struct PassEntry {
    std::string id;
    std::string name;
    std::string description;
    std::string category;
    IRUnitKind  unit;
};

// ── Registry singleton ────────────────────────────────────────────────────

class PassRegistry {
public:
    static PassRegistry &get() {
        static PassRegistry instance;
        return instance;
    }

    // ── Registration -------------------------------------------------------

    void registerPass(PassEntry entry) {
        const std::string id = entry.id;
        passes_[id] = std::move(entry);
        passOrder_.push_back(id);
    }

    void registerAnalysis(AnalysisEntry entry) {
        const std::string id = entry.id;
        analyses_[id] = std::move(entry);
        analysisOrder_.push_back(id);
    }

    // ── Lookup -------------------------------------------------------------

    // Returns nullptr if not found.
    const PassEntry *lookupPass(std::string_view id) const {
        auto it = passes_.find(std::string(id));
        return it != passes_.end() ? &it->second : nullptr;
    }

    const AnalysisEntry *lookupAnalysis(std::string_view id) const {
        auto it = analyses_.find(std::string(id));
        return it != analyses_.end() ? &it->second : nullptr;
    }

    bool isKnownPass(std::string_view id) const {
        return passes_.count(std::string(id)) > 0;
    }

    bool isKnownAnalysis(std::string_view id) const {
        return analyses_.count(std::string(id)) > 0;
    }

    // ── Enumeration (insertion order preserved) ----------------------------

    std::vector<const PassEntry *> allPasses() const {
        std::vector<const PassEntry *> out;
        out.reserve(passOrder_.size());
        for (const auto &id : passOrder_)
            out.push_back(&passes_.at(id));
        return out;
    }

    std::vector<const AnalysisEntry *> allAnalyses() const {
        std::vector<const AnalysisEntry *> out;
        out.reserve(analysisOrder_.size());
        for (const auto &id : analysisOrder_)
            out.push_back(&analyses_.at(id));
        return out;
    }

    // ── CLI output helpers -------------------------------------------------

    // Prints `nano passes` output.
    void printPasses(std::ostream &out) const {
        out << "Available compiler passes\n\n";

        // Group by category.
        std::vector<std::string> categories;
        for (const auto &id : passOrder_) {
            const std::string &cat = passes_.at(id).category;
            if (std::find(categories.begin(), categories.end(), cat)
                    == categories.end())
                categories.push_back(cat);
        }

        for (const auto &cat : categories) {
            // Capitalise first letter for display.
            std::string catDisplay = cat;
            if (!catDisplay.empty())
                catDisplay[0] = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(catDisplay[0])));
            out << catDisplay << "\n";
            for (const auto &id : passOrder_) {
                const PassEntry &e = passes_.at(id);
                if (e.category != cat) continue;
                out << "  " << e.id << "\n"
                    << "    Unit:  " << toString(e.unit) << "\n"
                    << "    " << e.description << "\n\n";
            }
        }
    }

    // Prints `nano analyses` output.
    void printAnalyses(std::ostream &out) const {
        out << "Available compiler analyses\n\n";

        std::vector<std::string> categories;
        for (const auto &id : analysisOrder_) {
            const std::string &cat = analyses_.at(id).category;
            if (std::find(categories.begin(), categories.end(), cat)
                    == categories.end())
                categories.push_back(cat);
        }

        for (const auto &cat : categories) {
            std::string catDisplay = cat;
            if (!catDisplay.empty())
                catDisplay[0] = static_cast<char>(
                    std::toupper(static_cast<unsigned char>(catDisplay[0])));
            out << catDisplay << "\n";
            for (const auto &id : analysisOrder_) {
                const AnalysisEntry &e = analyses_.at(id);
                if (e.category != cat) continue;
                out << "  " << e.id << "\n"
                    << "    Unit:  " << toString(e.unit) << "\n"
                    << "    " << e.description << "\n\n";
            }
        }
    }

    // Validate a list of pass IDs (e.g., from --passes=).
    // Returns false and writes errors to `err` if any ID is unknown.
    bool validatePassIds(const std::vector<std::string> &ids,
                         std::ostream &err) const {
        bool ok = true;
        for (const auto &id : ids) {
            if (!isKnownPass(id)) {
                err << "error: unknown compiler pass '" << id << "'\n";
                ok = false;
            }
        }
        if (!ok) {
            err << "\nAvailable passes:\n";
            for (const auto *e : allPasses())
                err << "  " << e->id << "\n";
        }
        return ok;
    }

private:
    PassRegistry() = default;

    std::unordered_map<std::string, PassEntry>     passes_;
    std::vector<std::string>                        passOrder_;   // insertion order
    std::unordered_map<std::string, AnalysisEntry> analyses_;
    std::vector<std::string>                        analysisOrder_;
};

} // namespace nanopass
