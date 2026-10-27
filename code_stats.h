#ifndef FKC_CODE_STATS_H
#define FKC_CODE_STATS_H

#include <wx/string.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace fkcstats {

struct TodoItem {
    wxString path;
    wxString displayPath;
    wxString text;
    int line{0};
    bool fixme{false};
};

struct FileStats {
    wxString path;
    wxString displayPath;
    wxString language;
    int totalLines{0};
    int codeLines{0};
    int commentLines{0};
    int blankLines{0};
    int complexity{0};
    int todoCount{0};
    int fixmeCount{0};
    bool astUsed{false};
    bool readError{false};
    wxString error;
};

struct ScanTotals {
    int files{0};
    int totalLines{0};
    int codeLines{0};
    int commentLines{0};
    int blankLines{0};
    int complexity{0};
    int todoCount{0};
    int fixmeCount{0};
    int failedFiles{0};
    int astFiles{0};
    double commentRate{0.0};
    double qualityScore{100.0};
};

struct ScanProgress {
    int processed{0};
    int total{0};
    wxString current;
    wxString phase;
};

struct ScanRequest {
    wxString root;
    wxString scopeLabel;
    wxString singleFile;
    bool useLibclang{true};
};

struct ScanResult {
    std::vector<FileStats> files;
    std::vector<TodoItem> todos;
    ScanTotals totals;
    wxString root;
    wxString scopeLabel;
    wxString engineLabel;
    wxString message;
    long elapsedMs{0};
    bool cancelled{false};
};

class CodeStatsScanner {
public:
    static bool IsLibclangAvailable();

    static std::shared_ptr<ScanResult> Scan(const ScanRequest& request,
                                            const std::atomic<bool>& cancel,
                                            const std::function<void(const ScanProgress&)>& onProgress);

    static wxString FormatNumber(long long value);
    static wxString FormatPercent(double value);
    static wxString QualityGrade(double score);
};

} // namespace fkcstats

#endif // FKC_CODE_STATS_H
