#include "code_stats.h"

#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/ffile.h>
#include <wx/filename.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#if defined(__has_include)
#    if __has_include(<clang-c/Index.h>)
#        define FKC_HAVE_LIBCLANG_HEADERS 1
#        include <clang-c/Index.h>
#    endif
#endif

#ifndef FKC_HAVE_LIBCLANG_HEADERS
#    define FKC_HAVE_LIBCLANG_HEADERS 0
#endif

namespace {

using Clock = std::chrono::steady_clock;

constexpr wxFileOffset kMaxFileSize = 4 * 1024 * 1024;

enum class CommentStyle {
    None,
    CLike,
    Hash,
    Sql,
    Lua,
    Html,
};

struct LangInfo {
    wxString name;
    wxString astLanguage;
    CommentStyle style{CommentStyle::None};
    bool tripleQuotes{false};
    bool backtickStrings{false};
    bool luaLongStrings{false};
    bool python{false};

    bool supported() const { return !name.empty(); }
};

LangInfo MakeLang(const wxString& name, CommentStyle style, const wxString& astLanguage = wxEmptyString)
{
    LangInfo info;
    info.name = name;
    info.style = style;
    info.astLanguage = astLanguage;
    return info;
}

LangInfo DetectLanguage(const wxString& path)
{
    const wxString fullName = wxFileName(path).GetFullName().Lower();
    const wxString ext = wxFileName(path).GetExt().Lower();

    if (fullName == "cmakelists.txt") {
        return MakeLang("CMake", CommentStyle::Hash);
    }
    if (fullName == "makefile" || fullName == "gnumakefile") {
        return MakeLang("Makefile", CommentStyle::Hash);
    }

    if (ext == "c") {
        return MakeLang("C", CommentStyle::CLike, "c");
    }
    if (ext == "h" || ext == "hpp" || ext == "hh" || ext == "hxx" || ext == "h++" || ext == "inl" ||
        ext == "ipp") {
        return MakeLang("C/C++ Header", CommentStyle::CLike, "c++");
    }
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c++") {
        return MakeLang("C++", CommentStyle::CLike, "c++");
    }
    if (ext == "m") {
        return MakeLang("Objective-C", CommentStyle::CLike, "objective-c");
    }
    if (ext == "mm") {
        return MakeLang("Objective-C++", CommentStyle::CLike, "objective-c++");
    }
    if (ext == "cs") {
        return MakeLang("C#", CommentStyle::CLike);
    }
    if (ext == "java") {
        return MakeLang("Java", CommentStyle::CLike);
    }
    if (ext == "js" || ext == "mjs" || ext == "cjs" || ext == "jsx") {
        LangInfo info = MakeLang("JavaScript", CommentStyle::CLike);
        info.backtickStrings = true;
        return info;
    }
    if (ext == "ts" || ext == "tsx") {
        LangInfo info = MakeLang("TypeScript", CommentStyle::CLike);
        info.backtickStrings = true;
        return info;
    }
    if (ext == "go") {
        LangInfo info = MakeLang("Go", CommentStyle::CLike, "c++");
        info.backtickStrings = true;
        return info;
    }
    if (ext == "rs") {
        return MakeLang("Rust", CommentStyle::CLike, "c++");
    }
    if (ext == "kt" || ext == "kts") {
        return MakeLang("Kotlin", CommentStyle::CLike);
    }
    if (ext == "swift") {
        return MakeLang("Swift", CommentStyle::CLike);
    }
    if (ext == "php") {
        return MakeLang("PHP", CommentStyle::CLike);
    }
    if (ext == "dart") {
        return MakeLang("Dart", CommentStyle::CLike);
    }
    if (ext == "scala") {
        return MakeLang("Scala", CommentStyle::CLike);
    }
    if (ext == "groovy" || ext == "gradle") {
        return MakeLang("Groovy", CommentStyle::CLike);
    }
    if (ext == "proto") {
        return MakeLang("Protocol Buffers", CommentStyle::CLike);
    }
    if (ext == "sol") {
        return MakeLang("Solidity", CommentStyle::CLike);
    }
    if (ext == "v" || ext == "sv" || ext == "svh") {
        return MakeLang("Verilog", CommentStyle::CLike);
    }
    if (ext == "py" || ext == "pyw") {
        LangInfo info = MakeLang("Python", CommentStyle::Hash);
        info.tripleQuotes = true;
        info.python = true;
        return info;
    }
    if (ext == "rb") {
        return MakeLang("Ruby", CommentStyle::Hash);
    }
    if (ext == "sh" || ext == "bash" || ext == "zsh" || ext == "ksh") {
        return MakeLang("Shell", CommentStyle::Hash);
    }
    if (ext == "pl" || ext == "pm") {
        return MakeLang("Perl", CommentStyle::Hash);
    }
    if (ext == "tcl") {
        return MakeLang("Tcl", CommentStyle::Hash);
    }
    if (ext == "lua") {
        LangInfo info = MakeLang("Lua", CommentStyle::Lua);
        info.luaLongStrings = true;
        return info;
    }
    if (ext == "sql") {
        return MakeLang("SQL", CommentStyle::Sql);
    }
    if (ext == "css" || ext == "scss" || ext == "sass" || ext == "less") {
        return MakeLang("CSS", CommentStyle::CLike);
    }
    if (ext == "html" || ext == "htm" || ext == "xml" || ext == "svg" || ext == "xaml" || ext == "vue" ||
        ext == "svelte") {
        return MakeLang("HTML/XML", CommentStyle::Html);
    }
    if (ext == "cmake") {
        return MakeLang("CMake", CommentStyle::Hash);
    }
    if (ext == "mk" || ext == "mak") {
        return MakeLang("Makefile", CommentStyle::Hash);
    }
    if (ext == "yaml" || ext == "yml") {
        return MakeLang("YAML", CommentStyle::Hash);
    }
    if (ext == "toml") {
        return MakeLang("TOML", CommentStyle::Hash);
    }
    if (ext == "ps1" || ext == "psm1") {
        return MakeLang("PowerShell", CommentStyle::Hash);
    }

    return LangInfo{};
}

bool IsHiddenName(const wxString& name)
{
    return name.StartsWith(".") || name.StartsWith("$");
}

bool IsSkippedDirName(const wxString& name)
{
    const wxString lower = name.Lower();
    static const wxString skipped[] = {
        wxT("node_modules"), wxT("bower_components"), wxT("jspm_packages"), wxT("vendor"), wxT("third_party"),
        wxT("third-party"), wxT("3rdparty"), wxT("external"), wxT("deps"), wxT("packages"), wxT("bin"),
        wxT("obj"), wxT("out"), wxT("output"), wxT("build"), wxT("builds"), wxT("dist"), wxT("target"),
        wxT("coverage"), wxT("cmakefiles"), wxT("cmake-build-debug"), wxT("cmake-build-release"),
        wxT("cmake-build-relwithdebinfo"), wxT("cmake-build-minsizerel"), wxT("debug"), wxT("release"),
        wxT("relwithdebinfo"), wxT("minsizerel"), wxT("x64"), wxT("x86"), wxT("arm"), wxT("arm64"),
        wxT("win_x64_debug"), wxT("win_x64_release"), wxT("tmp"), wxT("temp"), wxT(".cache"),
        wxT(".build"), wxT(".build-release"), wxT(".build-debug"), wxT(".vs"), wxT(".vscode"), wxT(".idea"),
        wxT(".git"), wxT(".svn"), wxT(".hg"),
    };

    if (IsHiddenName(name)) {
        return true;
    }
    for (const wxString& candidate : skipped) {
        if (lower == candidate) {
            return true;
        }
    }
    return false;
}

bool IsSkippedFileName(const wxString& name)
{
    const wxString lower = name.Lower();
    if (IsHiddenName(name) || lower.Contains(".min.")) {
        return true;
    }
    return lower.StartsWith("moc_") || lower.StartsWith("qrc_") || lower.StartsWith("ui_") ||
           lower.EndsWith(".designer.cs") || lower.EndsWith(".g.cs") || lower.EndsWith(".g.i.cs") ||
           lower.EndsWith(".pb.cc") || lower.EndsWith(".pb.h");
}

void CollectFiles(const wxString& directory, std::vector<wxString>& files, int depth)
{
    if (depth > 64) {
        return;
    }

    wxDir dir(directory);
    if (!dir.IsOpened()) {
        return;
    }

    wxString name;
    bool more = dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS | wxDIR_FILES);
    while (more) {
        const wxString fullPath = wxFileName(directory, name).GetFullPath();
        if (wxFileName::DirExists(fullPath)) {
            if (!IsSkippedDirName(name)) {
                CollectFiles(fullPath, files, depth + 1);
            }
        } else if (!IsSkippedFileName(name)) {
            const LangInfo lang = DetectLanguage(fullPath);
            if (lang.supported()) {
                wxFileName file(fullPath);
                if (file.GetSize() <= static_cast<wxULongLong>(kMaxFileSize)) {
                    files.push_back(fullPath);
                }
            }
        }
        more = dir.GetNext(&name);
    }
}

wxString MakeDisplayPath(const wxString& path, const wxString& root)
{
    if (root.empty()) {
        return wxFileName(path).GetFullName();
    }

    wxFileName file(path);
    file.MakeRelativeTo(root);
    wxString relative = file.GetFullPath();
    if (relative.StartsWith("..")) {
        return path;
    }
    return relative;
}

bool ReadTextFile(const wxString& path, wxString& content, wxString& error)
{
    wxFFile file(path, "rb");
    if (!file.IsOpened()) {
        error = wxT("无法打开文件");
        return false;
    }

    if (!file.ReadAll(&content, wxConvAuto())) {
        error = wxT("读取文件失败");
        return false;
    }

    if (content.find(wxUniChar(0)) != wxString::npos) {
        error = wxT("二进制文件已跳过");
        return false;
    }
    return true;
}

void SplitLines(const wxString& content, std::vector<wxString>& lines)
{
    if (content.empty()) {
        return;
    }

    size_t start = 0;
    const size_t length = content.length();
    while (start < length) {
        const size_t pos = content.find(wxT('\n'), start);
        wxString line = (pos == wxString::npos) ? content.Mid(start) : content.Mid(start, pos - start);
        if (!line.empty() && line.Last() == wxT('\r')) {
            line.RemoveLast();
        }
        lines.push_back(line);
        if (pos == wxString::npos) {
            break;
        }
        start = pos + 1;
    }
}

bool MatchAt(const wxString& text, size_t pos, const wxString& token)
{
    if (pos + token.length() > text.length()) {
        return false;
    }
    for (size_t i = 0; i < token.length(); ++i) {
        if (text[pos + i] != token[i]) {
            return false;
        }
    }
    return true;
}

bool IsWordChar(wxUniChar ch)
{
    if (ch >= wxT('a') && ch <= wxT('z')) {
        return true;
    }
    if (ch >= wxT('A') && ch <= wxT('Z')) {
        return true;
    }
    if (ch >= wxT('0') && ch <= wxT('9')) {
        return true;
    }
    return ch == wxT('_') || ch > 127;
}

bool IsIdentifierChar(wxUniChar ch)
{
    return IsWordChar(ch) || ch == wxT('$');
}

bool IsWhitespace(wxUniChar ch)
{
    return ch == wxT(' ') || ch == wxT('\t') || ch == wxT('\r') || ch == wxT('\f') || ch == wxT('\v');
}

bool IsQuoteChar(wxUniChar ch, const LangInfo& lang)
{
    if (ch == wxT('"') || ch == wxT('\'')) {
        return true;
    }
    return lang.backtickStrings && ch == wxT('`');
}

bool IsBranchKeyword(const wxString& word, const LangInfo& lang)
{
    if (word == wxT("if") || word == wxT("for") || word == wxT("while") || word == wxT("case") ||
        word == wxT("catch")) {
        return true;
    }
    if (lang.python) {
        return word == wxT("elif") || word == wxT("and") || word == wxT("or");
    }
    return false;
}

int CountComplexity(const wxString& code, const LangInfo& lang)
{
    int complexity = 0;
    wxString identifier;

    const size_t length = code.length();
    for (size_t i = 0; i < length; ++i) {
        const wxUniChar ch = code[i];
        if (IsIdentifierChar(ch)) {
            identifier += ch;
            continue;
        }

        if (!identifier.empty()) {
            if (IsBranchKeyword(identifier.Lower(), lang)) {
                ++complexity;
            }
            identifier.clear();
        }

        if ((ch == wxT('&') || ch == wxT('|')) && (i + 1 < length) && code[i + 1] == ch) {
            ++complexity;
            ++i;
        } else if (ch == wxT('?')) {
            ++complexity;
        }
    }

    if (!identifier.empty() && IsBranchKeyword(identifier.Lower(), lang)) {
        ++complexity;
    }
    return complexity;
}

void CollectMarkers(const wxString& commentText,
                    int line,
                    const wxString& path,
                    const wxString& displayPath,
                    fkcstats::FileStats& stats,
                    std::vector<fkcstats::TodoItem>& todos)
{
    if (commentText.empty()) {
        return;
    }

    wxString summary = commentText;
    summary.Trim(true).Trim(false);
    if (summary.length() > 180) {
        summary = summary.Left(177) + wxT("...");
    }

    const wxString upper = commentText.Upper();
    for (int pass = 0; pass < 2; ++pass) {
        const wxString marker = (pass == 0) ? wxT("TODO") : wxT("FIXME");
        const bool fixme = (pass == 1);
        size_t pos = 0;
        while ((pos = upper.find(marker, pos)) != wxString::npos) {
            const size_t end = pos + marker.length();
            const bool leftOk = (pos == 0) || !IsWordChar(commentText[pos - 1]);
            const bool rightOk = (end >= commentText.length()) || !IsWordChar(commentText[end]);
            if (leftOk && rightOk) {
                if (fixme) {
                    ++stats.fixmeCount;
                } else {
                    ++stats.todoCount;
                }

                fkcstats::TodoItem item;
                item.path = path;
                item.displayPath = displayPath;
                item.text = summary;
                item.line = line;
                item.fixme = fixme;
                todos.push_back(item);
            }
            pos = end;
        }
    }
}

struct AnalysisState {
    bool inBlockComment{false};
    wxString blockEnd;
    bool inString{false};
    bool tripleString{false};
    bool multilineString{false};
    wxUniChar quote{0};
    bool inLuaLongString{false};
};

void AnalyzeSource(const wxString& path,
                   const wxString& displayPath,
                   const LangInfo& lang,
                   const wxString& content,
                   fkcstats::FileStats& stats,
                   std::vector<fkcstats::TodoItem>& todos)
{
    std::vector<wxString> lines;
    SplitLines(content, lines);
    stats.totalLines = static_cast<int>(lines.size());

    AnalysisState state;
    wxString codeOnly;
    codeOnly.reserve(content.length() / 2);

    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const wxString& line = lines[lineIndex];
        const int lineNumber = static_cast<int>(lineIndex) + 1;
        bool hasCode = false;
        bool hasComment = false;
        wxString commentText;

        size_t i = 0;
        while (i < line.length()) {
            const wxUniChar ch = line[i];

            if (state.inLuaLongString) {
                hasCode = true;
                codeOnly += wxT(' ');
                if (MatchAt(line, i, wxT("]]"))) {
                    state.inLuaLongString = false;
                    i += 2;
                } else {
                    ++i;
                }
                continue;
            }

            if (state.inBlockComment) {
                hasComment = true;
                const size_t endPos = line.find(state.blockEnd, i);
                if (endPos == wxString::npos) {
                    commentText += line.Mid(i);
                    i = line.length();
                } else {
                    commentText += line.Mid(i, endPos - i);
                    i = endPos + state.blockEnd.length();
                    state.inBlockComment = false;
                    state.blockEnd.clear();
                }
                continue;
            }

            if (state.inString) {
                hasCode = true;
                codeOnly += wxT(' ');
                if (state.tripleString) {
                    wxString tripleQuote;
                    tripleQuote += state.quote;
                    tripleQuote += state.quote;
                    tripleQuote += state.quote;
                    if (MatchAt(line, i, tripleQuote)) {
                        i += 3;
                        state.inString = false;
                        state.tripleString = false;
                        state.multilineString = false;
                    } else if (ch == wxT('\\') && (i + 1 < line.length())) {
                        i += 2;
                    } else {
                        ++i;
                    }
                } else {
                    if (ch == wxT('\\') && (i + 1 < line.length())) {
                        i += 2;
                    } else if (ch == state.quote) {
                        ++i;
                        state.inString = false;
                        state.multilineString = false;
                    } else {
                        ++i;
                    }
                }
                continue;
            }

            if (lang.luaLongStrings && MatchAt(line, i, wxT("[["))) {
                hasCode = true;
                state.inLuaLongString = true;
                i += 2;
                continue;
            }

            bool blockStarted = false;
            if (lang.style == CommentStyle::CLike || lang.style == CommentStyle::Sql) {
                if (MatchAt(line, i, wxT("/*"))) {
                    hasComment = true;
                    state.inBlockComment = true;
                    state.blockEnd = wxT("*/");
                    i += 2;
                    blockStarted = true;
                }
            } else if (lang.style == CommentStyle::Html) {
                if (MatchAt(line, i, wxT("<!--"))) {
                    hasComment = true;
                    state.inBlockComment = true;
                    state.blockEnd = wxT("-->");
                    i += 4;
                    blockStarted = true;
                }
            } else if (lang.style == CommentStyle::Lua) {
                if (MatchAt(line, i, wxT("--[["))) {
                    hasComment = true;
                    state.inBlockComment = true;
                    state.blockEnd = wxT("]]");
                    i += 4;
                    blockStarted = true;
                }
            }
            if (blockStarted) {
                continue;
            }

            wxString lineCommentMarker;
            if (lang.style == CommentStyle::CLike) {
                lineCommentMarker = wxT("//");
            } else if (lang.style == CommentStyle::Hash) {
                lineCommentMarker = wxT("#");
            } else if (lang.style == CommentStyle::Sql || lang.style == CommentStyle::Lua) {
                lineCommentMarker = wxT("--");
            }

            if (!lineCommentMarker.empty() && MatchAt(line, i, lineCommentMarker)) {
                hasComment = true;
                commentText += line.Mid(i + lineCommentMarker.length());
                codeOnly += wxT(' ');
                i = line.length();
                break;
            }

            if (IsQuoteChar(ch, lang)) {
                hasCode = true;
                codeOnly += wxT(' ');
                state.inString = true;
                state.quote = ch;
                if (lang.tripleQuotes && (i + 2 < line.length()) && line[i + 1] == ch && line[i + 2] == ch) {
                    state.tripleString = true;
                    state.multilineString = true;
                    i += 3;
                } else {
                    state.tripleString = false;
                    state.multilineString = (ch == wxT('`'));
                    ++i;
                }
                continue;
            }

            if (!IsWhitespace(ch)) {
                hasCode = true;
            }
            codeOnly += ch;
            ++i;
        }

        if (state.inString && !state.multilineString) {
            state.inString = false;
            state.tripleString = false;
        }

        if (hasCode) {
            ++stats.codeLines;
        } else if (hasComment) {
            ++stats.commentLines;
        } else {
            ++stats.blankLines;
        }

        if (hasComment && !commentText.empty()) {
            CollectMarkers(commentText, lineNumber, path, displayPath, stats, todos);
        }

        codeOnly += wxT('\n');
    }

    stats.complexity = CountComplexity(codeOnly, lang);
}

#if FKC_HAVE_LIBCLANG_HEADERS

class LibclangApi {
public:
    static LibclangApi* Get()
    {
        static LibclangApi api;
        return api.m_loaded ? &api : nullptr;
    }

    bool IsLoaded() const { return m_loaded; }

    CXIndex CreateIndex() const { return m_createIndex(0, 0); }
    void DisposeIndex(CXIndex index) const { m_disposeIndex(index); }

    CXErrorCode ParseTranslationUnit(CXIndex index,
                                     const char* source,
                                     const char* const* args,
                                     int argCount,
                                     CXTranslationUnit* tu) const
    {
        return m_parseTranslationUnit2(index,
                                       source,
                                       args,
                                       argCount,
                                       nullptr,
                                       0,
                                       CXTranslationUnit_SingleFileParse | CXTranslationUnit_KeepGoing |
                                           CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles,
                                       tu);
    }

    void DisposeTranslationUnit(CXTranslationUnit tu) const { m_disposeTranslationUnit(tu); }
    CXCursor GetTranslationUnitCursor(CXTranslationUnit tu) const { return m_getTranslationUnitCursor(tu); }
    unsigned VisitChildren(CXCursor cursor, CXCursorVisitor visitor, CXClientData data) const
    {
        return m_visitChildren(cursor, visitor, data);
    }
    CXCursorKind GetCursorKind(CXCursor cursor) const { return m_getCursorKind(cursor); }
    CXSourceLocation GetCursorLocation(CXCursor cursor) const { return m_getCursorLocation(cursor); }
    int LocationIsFromMainFile(CXSourceLocation location) const { return m_locationIsFromMainFile(location); }

private:
    LibclangApi() { m_loaded = LoadLibrary(); }

    template <typename T>
    bool Resolve(T& fn, const char* name)
    {
        fn = reinterpret_cast<T>(m_library.GetSymbol(name));
        return fn != nullptr;
    }

    bool LoadLibrary()
    {
#ifdef __WXMSW__
        const wxString candidates[] = {wxT("libclang.dll")};
#elif defined(__WXOSX__)
        const wxString candidates[] = {wxT("libclang.dylib")};
#else
        const wxString candidates[] = {
            wxT("libclang.so"),
            wxT("libclang.so.1"),
            wxT("libclang-22.so.1"),
            wxT("libclang-21.so.1"),
            wxT("libclang-20.so.1"),
            wxT("libclang-19.so.1"),
            wxT("libclang-18.so.1"),
        };
#endif

        bool loaded = false;
        for (const wxString& candidate : candidates) {
            if (m_library.Load(candidate, wxDL_VERBATIM)) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            return false;
        }

        const bool ok = Resolve(m_createIndex, "clang_createIndex") &&
                        Resolve(m_disposeIndex, "clang_disposeIndex") &&
                        Resolve(m_parseTranslationUnit2, "clang_parseTranslationUnit2") &&
                        Resolve(m_disposeTranslationUnit, "clang_disposeTranslationUnit") &&
                        Resolve(m_getTranslationUnitCursor, "clang_getTranslationUnitCursor") &&
                        Resolve(m_visitChildren, "clang_visitChildren") &&
                        Resolve(m_getCursorKind, "clang_getCursorKind") &&
                        Resolve(m_getCursorLocation, "clang_getCursorLocation") &&
                        Resolve(m_locationIsFromMainFile, "clang_Location_isFromMainFile");

        if (!ok) {
            m_library.Unload();
            return false;
        }
        return true;
    }

    wxDynamicLibrary m_library;
    bool m_loaded{false};

    using FnCreateIndex = CXIndex (*)(int, int);
    using FnDisposeIndex = void (*)(CXIndex);
    using FnParseTranslationUnit2 = CXErrorCode (*)(CXIndex,
                                                    const char*,
                                                    const char* const*,
                                                    int,
                                                    struct CXUnsavedFile*,
                                                    unsigned,
                                                    unsigned,
                                                    CXTranslationUnit*);
    using FnDisposeTranslationUnit = void (*)(CXTranslationUnit);
    using FnGetTranslationUnitCursor = CXCursor (*)(CXTranslationUnit);
    using FnVisitChildren = unsigned (*)(CXCursor, CXCursorVisitor, CXClientData);
    using FnGetCursorKind = CXCursorKind (*)(CXCursor);
    using FnGetCursorLocation = CXSourceLocation (*)(CXCursor);
    using FnLocationIsFromMainFile = int (*)(CXSourceLocation);

    FnCreateIndex m_createIndex{nullptr};
    FnDisposeIndex m_disposeIndex{nullptr};
    FnParseTranslationUnit2 m_parseTranslationUnit2{nullptr};
    FnDisposeTranslationUnit m_disposeTranslationUnit{nullptr};
    FnGetTranslationUnitCursor m_getTranslationUnitCursor{nullptr};
    FnVisitChildren m_visitChildren{nullptr};
    FnGetCursorKind m_getCursorKind{nullptr};
    FnGetCursorLocation m_getCursorLocation{nullptr};
    FnLocationIsFromMainFile m_locationIsFromMainFile{nullptr};
};

struct AstCounter {
    int branches{0};
};

CXChildVisitResult AstVisitor(CXCursor cursor, CXCursor parent, CXClientData clientData)
{
    wxUnusedVar(parent);
    LibclangApi* api = LibclangApi::Get();
    if (!api) {
        return CXChildVisit_Break;
    }

    const CXCursorKind kind = api->GetCursorKind(cursor);
    if (kind != CXCursor_TranslationUnit && !api->LocationIsFromMainFile(api->GetCursorLocation(cursor))) {
        return CXChildVisit_Continue;
    }

    AstCounter* counter = static_cast<AstCounter*>(clientData);
    switch (kind) {
    case CXCursor_IfStmt:
    case CXCursor_ForStmt:
    case CXCursor_WhileStmt:
    case CXCursor_DoStmt:
    case CXCursor_CaseStmt:
    case CXCursor_CXXCatchStmt:
    case CXCursor_ConditionalOperator:
    case CXCursor_CXXForRangeStmt:
    case CXCursor_ObjCForCollectionStmt:
        ++counter->branches;
        break;
    default:
        break;
    }

    return CXChildVisit_Recurse;
}

bool AnalyzeWithLibclang(const wxString& path, const LangInfo& lang, int& complexity)
{
    LibclangApi* api = LibclangApi::Get();
    if (!api || lang.astLanguage.empty()) {
        return false;
    }

    const wxScopedCharBuffer pathUtf8 = path.utf8_str();
    const wxScopedCharBuffer languageUtf8 = lang.astLanguage.utf8_str();
    const char* args[] = {"-x",
                          languageUtf8.data(),
                          lang.astLanguage == wxT("c") ? "-std=c11" : "-std=c++17",
                          "-Wno-everything"};
    const int argCount = static_cast<int>(sizeof(args) / sizeof(args[0]));

    CXIndex index = api->CreateIndex();
    if (!index) {
        return false;
    }

    CXTranslationUnit tu = nullptr;
    const CXErrorCode error = api->ParseTranslationUnit(index, pathUtf8.data(), args, argCount, &tu);
    if (error != CXError_Success || !tu) {
        api->DisposeIndex(index);
        return false;
    }

    AstCounter counter;
    api->VisitChildren(api->GetTranslationUnitCursor(tu), &AstVisitor, &counter);
    api->DisposeTranslationUnit(tu);
    api->DisposeIndex(index);

    complexity = counter.branches;
    return true;
}

#else

bool AnalyzeWithLibclang(const wxString& path, const LangInfo& lang, int& complexity)
{
    wxUnusedVar(path);
    wxUnusedVar(lang);
    wxUnusedVar(complexity);
    return false;
}

#endif // FKC_HAVE_LIBCLANG_HEADERS

fkcstats::FileStats AnalyzeFile(const wxString& path,
                                const wxString& displayPath,
                                const wxString& root,
                                bool useLibclang,
                                std::vector<fkcstats::TodoItem>& todos)
{
    fkcstats::FileStats stats;
    stats.path = path;
    stats.displayPath = displayPath;

    const LangInfo lang = DetectLanguage(path);
    stats.language = lang.name;

    wxString content;
    wxString error;
    if (!ReadTextFile(path, content, error)) {
        stats.readError = true;
        stats.error = error;
        return stats;
    }

    AnalyzeSource(path, displayPath, lang, content, stats, todos);

    if (useLibclang && !lang.astLanguage.empty() && stats.codeLines > 0) {
        int astComplexity = 0;
        if (AnalyzeWithLibclang(path, lang, astComplexity)) {
            stats.complexity = astComplexity;
            stats.astUsed = true;
        }
    }

    wxUnusedVar(root);
    return stats;
}

double ComputeQualityScore(const fkcstats::ScanTotals& totals)
{
    if (totals.files == 0) {
        return 0.0;
    }

    double score = 100.0;
    const double commentRate = totals.commentRate;
    if (commentRate < 10.0) {
        score -= 22.0;
    } else if (commentRate < 20.0) {
        score -= (20.0 - commentRate) * 1.0;
    } else if (commentRate < 30.0) {
        score -= (30.0 - commentRate) * 0.35;
    }

    const double complexityDensity =
        (totals.codeLines > 0) ? static_cast<double>(totals.complexity) * 100.0 / totals.codeLines : 0.0;
    if (complexityDensity > 8.0) {
        score -= std::min(25.0, (complexityDensity - 8.0) * 2.0);
    }

    const int markers = totals.todoCount + totals.fixmeCount;
    if (totals.codeLines > 0) {
        const double markersPerK = static_cast<double>(markers) * 1000.0 / totals.codeLines;
        score -= std::min(20.0, markersPerK * 1.2);
    }

    if (totals.failedFiles > 0) {
        score -= std::min(5.0, totals.failedFiles * 0.5);
    }

    return std::max(0.0, std::min(100.0, score));
}

} // namespace

namespace fkcstats {

bool CodeStatsScanner::IsLibclangAvailable()
{
#if FKC_HAVE_LIBCLANG_HEADERS
    return LibclangApi::Get() != nullptr;
#else
    return false;
#endif
}

std::shared_ptr<ScanResult> CodeStatsScanner::Scan(const ScanRequest& request,
                                                   const std::atomic<bool>& cancel,
                                                   const std::function<void(const ScanProgress&)>& onProgress)
{
    const auto startedAt = Clock::now();
    auto result = std::make_shared<ScanResult>();
    result->root = request.root;
    result->scopeLabel = request.scopeLabel;

    const bool libclangFound = request.useLibclang && IsLibclangAvailable();

    std::vector<wxString> files;
    if (!request.singleFile.empty()) {
        files.push_back(request.singleFile);
    } else if (!request.root.empty()) {
        if (onProgress) {
            onProgress(ScanProgress{0, 0, wxEmptyString, wxT("正在遍历工作区...")});
        }
        CollectFiles(request.root, files, 0);
    }

    std::sort(files.begin(), files.end(), [](const wxString& a, const wxString& b) {
        return a.CmpNoCase(b) < 0;
    });

    if (cancel) {
        result->cancelled = true;
        result->message = wxT("扫描已取消");
    } else if (files.empty()) {
        result->message = wxT("未找到可统计的源代码文件");
    }

    const int total = static_cast<int>(files.size());
    auto report = [&](int processed, const wxString& current, const wxString& phase) {
        if (onProgress) {
            onProgress(ScanProgress{processed, total, current, phase});
        }
    };

    if (!result->cancelled && !files.empty()) {
        report(0, wxEmptyString, wxT("准备分析"));
    }

    auto lastReport = Clock::now() - std::chrono::seconds(1);
    for (int i = 0; i < total; ++i) {
        if (cancel) {
            result->cancelled = true;
            break;
        }

        const wxString displayPath = MakeDisplayPath(files[i], result->root);
        const auto now = Clock::now();
        if (i == 0 || now - lastReport > std::chrono::milliseconds(120)) {
            report(i, displayPath, wxT("正在分析"));
            lastReport = now;
        }

        FileStats stats = AnalyzeFile(files[i], displayPath, result->root, libclangFound, result->todos);
        result->files.push_back(stats);

        ++result->totals.files;
        result->totals.totalLines += stats.totalLines;
        result->totals.codeLines += stats.codeLines;
        result->totals.commentLines += stats.commentLines;
        result->totals.blankLines += stats.blankLines;
        result->totals.complexity += stats.complexity;
        result->totals.todoCount += stats.todoCount;
        result->totals.fixmeCount += stats.fixmeCount;
        if (stats.readError) {
            ++result->totals.failedFiles;
        }
        if (stats.astUsed) {
            ++result->totals.astFiles;
        }

        const auto after = Clock::now();
        if (i + 1 == total || after - lastReport > std::chrono::milliseconds(120)) {
            report(i + 1, displayPath, wxT("正在分析"));
            lastReport = after;
        }
    }

    if (cancel) {
        result->cancelled = true;
    }

    const int meaningfulLines = result->totals.codeLines + result->totals.commentLines;
    result->totals.commentRate =
        (meaningfulLines > 0) ? static_cast<double>(result->totals.commentLines) * 100.0 / meaningfulLines : 0.0;
    result->totals.qualityScore = ComputeQualityScore(result->totals);

    if (request.useLibclang && !libclangFound) {
        result->engineLabel = wxT("内置词法分析（未找到 libclang）");
    } else if (result->totals.astFiles > 0) {
        result->engineLabel = (result->totals.astFiles >= result->totals.files)
                                  ? wxT("libclang AST")
                                  : wxT("libclang AST + 内置词法分析");
    } else {
        result->engineLabel = wxT("内置词法分析");
    }

    result->elapsedMs =
        static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startedAt).count());

    if (result->cancelled) {
        result->message = wxT("扫描已取消，以下为已完成的部分结果");
    } else if (!files.empty()) {
        result->message = wxString::Format(wxT("已分析 %d 个文件"), result->totals.files);
    }

    return result;
}

wxString CodeStatsScanner::FormatNumber(long long value)
{
    wxString text = wxString::Format(wxT("%lld"), value);
    if (text.length() <= 3) {
        return text;
    }

    for (int pos = static_cast<int>(text.length()) - 3; pos > 0; pos -= 3) {
        text = text.Left(pos) + wxT(",") + text.Mid(pos);
    }
    return text;
}

wxString CodeStatsScanner::FormatPercent(double value)
{
    return wxString::Format(wxT("%.1f%%"), value);
}

wxString CodeStatsScanner::QualityGrade(double score)
{
    if (score >= 90.0) {
        return wxT("优秀");
    }
    if (score >= 75.0) {
        return wxT("良好");
    }
    if (score >= 60.0) {
        return wxT("一般");
    }
    return wxT("需改进");
}

} // namespace fkcstats
