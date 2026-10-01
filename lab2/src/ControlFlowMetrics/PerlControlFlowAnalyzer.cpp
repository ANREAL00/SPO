#include "PerlControlFlowAnalyzer.h"
#include <algorithm>
#include <cwctype>
#include <map>
#include <set>

namespace {
bool identStart(wchar_t c) { return std::iswalpha(c) || c == L'_'; }
bool identChar(wchar_t c) { return std::iswalnum(c) || c == L'_'; }

struct RawToken {
    std::wstring text; int start; int end;
    enum class Kind { Word, Variable, Symbol, Semicolon, OpenBrace, CloseBrace } kind;
};

size_t skipQuoted(const std::wstring& s, size_t i, wchar_t quote) {
    ++i;
    while (i < s.size()) {
        if (s[i] == L'\\' && i + 1 < s.size()) { i += 2; continue; }
        if (s[i++] == quote) return i;
    }
    return i;
}

wchar_t closingDelimiter(wchar_t open) {
    switch (open) {
    case L'(': return L')';
    case L'[': return L']';
    case L'{': return L'}';
    case L'<': return L'>';
    default: return open;
    }
}

size_t skipDelimited(const std::wstring& s, size_t i, wchar_t open) {
    const wchar_t close = closingDelimiter(open);
    const bool paired = open != close;
    int depth = 1;
    ++i;
    while (i < s.size()) {
        if (s[i] == L'\\' && i + 1 < s.size()) { i += 2; continue; }
        if (open == L'/' && s[i] == L'[') {
            ++i;
            while (i < s.size()) {
                if (s[i] == L'\\' && i + 1 < s.size()) { i += 2; continue; }
                if (s[i++] == L']') break;
            }
            continue;
        }
        if (paired && s[i] == open) ++depth;
        if (s[i] == close && --depth == 0) return i + 1;
        ++i;
    }
    return i;
}

bool isQuoteLike(const std::wstring& w) {
    return w == L"q" || w == L"qq" || w == L"qw" || w == L"qx" ||
           w == L"qr" || w == L"m" || w == L"s" || w == L"tr" || w == L"y";
}

bool regexMayStartAfter(const std::vector<RawToken>& out) {
    if (out.empty()) return true;
    const std::wstring& p = out.back().text;
    return p == L"=~" || p == L"!~" || p == L"split" || p == L"grep" ||
           p == L"(" || p == L"," || p == L"=" || p == L"=>" ||
           p == L"return" || p == L"!" || p == L"&&" || p == L"||";
}

std::vector<RawToken> lex(const std::wstring& s) {
    std::vector<RawToken> out;
    size_t i = 0;
    bool lineStart = true;
    while (i < s.size()) {
        const wchar_t c = s[i];
        if (c == L'\n') { ++i; lineStart = true; continue; }
        if (std::iswspace(c)) { ++i; continue; }
        const bool startOfLine = lineStart;
        lineStart = false;
        if (startOfLine && c == L'=' && i + 1 < s.size() && identStart(s[i + 1])) {
            size_t end = s.find_first_of(L" \t\r\n", i);
            std::wstring directive = s.substr(i, end == std::wstring::npos ? 4 : end - i);
            if (directive == L"=pod" || directive == L"=begin" || directive == L"=head1" ||
                directive == L"=head2" || directive == L"=over" || directive == L"=item") {
                size_t cut = s.find(L"\n=cut", i);
                if (cut == std::wstring::npos) break;
                size_t eol = s.find(L'\n', cut + 1);
                i = eol == std::wstring::npos ? s.size() : eol + 1;
                lineStart = true;
                continue;
            }
        }
        if (c == L'#') {
            size_t eol = s.find(L'\n', i);
            i = eol == std::wstring::npos ? s.size() : eol;
            continue;
        }
        if (c == L'\'' || c == L'"' || c == L'`') {
            i = skipQuoted(s, i, c);
            continue;
        }
        if ((c == L'$' || c == L'@' || c == L'%') && i + 1 < s.size() &&
            (identChar(s[i + 1]) || s[i + 1] == L'@')) {
            size_t start = i++;
            while (i < s.size() && identChar(s[i])) ++i;
            if (i == start + 1) ++i;
            out.push_back({s.substr(start, i - start), (int)start, (int)i, RawToken::Kind::Variable});
            continue;
        }
        if (identStart(c)) {
            size_t start = i++;
            while (i < s.size() && identChar(s[i])) ++i;
            std::wstring word = s.substr(start, i - start);
            if (startOfLine && word == L"__END__") break;
            size_t delimiterAt = i;
            while (delimiterAt < s.size() && std::iswspace(s[delimiterAt])) ++delimiterAt;
            if (isQuoteLike(word) && delimiterAt < s.size() &&
                !identChar(s[delimiterAt]) && s[delimiterAt] != L';' &&
                s[delimiterAt] != L')' && s[delimiterAt] != L'=' &&
                s[delimiterAt] != L',') {
                const wchar_t delimiter = s[delimiterAt];
                i = skipDelimited(s, delimiterAt, delimiter);
                if (word == L"s" || word == L"tr" || word == L"y") {
                    if (closingDelimiter(delimiter) != delimiter) {
                        while (i < s.size() && std::iswspace(s[i])) ++i;
                        if (i < s.size() && s[i] == delimiter)
                            i = skipDelimited(s, i, delimiter);
                    } else {
                        i = skipDelimited(s, i - 1, delimiter);
                    }
                }
                while (i < s.size() && identChar(s[i])) ++i;
                continue;
            }
            out.push_back({word, (int)start, (int)i, RawToken::Kind::Word});
            continue;
        }
        if (c == L'/' && i + 1 < s.size() && s[i + 1] != L'/' && regexMayStartAfter(out)) {
            i = skipDelimited(s, i, L'/');
            while (i < s.size() && identChar(s[i])) ++i;
            continue;
        }
        static const std::wstring multi[] = {L"=~", L"!~", L"//", L"=>", L"->", L"&&", L"||"};
        bool matched = false;
        for (const auto& op : multi) {
            if (s.compare(i, op.size(), op) == 0) {
                out.push_back({op, (int)i, (int)(i + op.size()), RawToken::Kind::Symbol});
                i += op.size();
                matched = true;
                break;
            }
        }
        if (matched) continue;
        RawToken::Kind kind = RawToken::Kind::Symbol;
        if (c == L';') kind = RawToken::Kind::Semicolon;
        else if (c == L'{') kind = RawToken::Kind::OpenBrace;
        else if (c == L'}') kind = RawToken::Kind::CloseBrace;
        out.push_back({std::wstring(1, c), (int)i, (int)i + 1, kind});
        ++i;
    }
    return out;
}

bool subscriptBrace(const std::vector<RawToken>& ts, size_t i) {
    if (i == 0) return false;
    const RawToken& prev = ts[i - 1];
    return prev.kind == RawToken::Kind::Variable || prev.text == L"->" ||
           prev.text == L"grep" || prev.text == L"map" || prev.text == L"sort";
}

int matchingBrace(const std::vector<RawToken>& ts, size_t open) {
    int depth = 0;
    for (size_t i = open; i < ts.size(); ++i) {
        if (ts[i].kind == RawToken::Kind::OpenBrace) ++depth;
        else if (ts[i].kind == RawToken::Kind::CloseBrace && --depth == 0) return (int)i;
    }
    return -1;
}

int bodyBraceAfter(const std::vector<RawToken>& ts, size_t keyword) {
    int parens = 0, brackets = 0;
    for (size_t i = keyword + 1; i < ts.size(); ++i) {
        const RawToken& t = ts[i];
        if (t.text == L"(") { ++parens; continue; }
        if (t.text == L")") { --parens; continue; }
        if (t.text == L"[") { ++brackets; continue; }
        if (t.text == L"]") { --brackets; continue; }
        if (parens || brackets) continue;
        if (t.kind == RawToken::Kind::Semicolon || t.kind == RawToken::Kind::CloseBrace) return -1;
        if (t.kind == RawToken::Kind::OpenBrace) {
            if (!subscriptBrace(ts, i)) return (int)i;
            int end = matchingBrace(ts, i);
            if (end < 0) return -1;
            i = (size_t)end;
        }
    }
    return -1;
}

void markForHeaderSemicolons(const std::vector<RawToken>& ts, size_t word,
                             int bodyBrace, std::set<size_t>& excluded) {
    size_t limit = bodyBrace < 0 ? ts.size() : (size_t)bodyBrace;
    size_t open = word + 1;
    while (open < limit && ts[open].text != L"(" && ts[open].kind != RawToken::Kind::Semicolon) ++open;
    if (open == limit || ts[open].text != L"(") return;
    int depth = 1;
    for (size_t i = open + 1; i < limit; ++i) {
        if (ts[i].text == L"(") ++depth;
        else if (ts[i].text == L")" && --depth == 0) return;
        else if (ts[i].kind == RawToken::Kind::Semicolon && depth == 1) excluded.insert(i);
    }
}

void add(std::vector<Token>& v, const std::wstring& name, TokenKind kind, const RawToken& t) {
    v.push_back({name, kind, t.start, t.end - t.start});
}

void makeEntries(AnalysisResult& r) {
    std::map<std::wstring, size_t> li, ri;
    for (size_t i = 0; i < r.tokens.size(); ++i) {
        const Token& t = r.tokens[i];
        auto& table = t.kind == TokenKind::Operator ? r.operators : r.operands;
        auto& index = t.kind == TokenKind::Operator ? li : ri;
        auto it = index.find(t.name);
        if (it == index.end()) { index[t.name] = table.size(); table.push_back(Entry{t.name}); it = index.find(t.name); }
        Entry& e = table[it->second]; ++e.count; e.tokenIndexes.push_back((int)i);
    }
    auto desc = [](const Entry& a, const Entry& b) { return a.count > b.count; };
    std::stable_sort(r.operators.begin(), r.operators.end(), desc);
    std::stable_sort(r.operands.begin(), r.operands.end(), desc);
}
}

AnalysisResult PerlControlFlowAnalyzer::analyze(const std::wstring& code) {
    AnalysisResult r;
    const std::vector<RawToken> ts = lex(code);
    enum class BlockKind { Other, IfBranch, ElsifBranch, ElseBranch, Loop, Given, When, Default, Do };
    struct Body { BlockKind kind; int depth; int conditionDepth; };
    struct Block { BlockKind kind; int previousDepth; int conditionDepth; int whenCount; };
    std::map<size_t, Body> scheduled;
    std::set<size_t> forHeaderSemicolons;
    std::vector<Block> blocks;
    int currentDepth = 0;
    int nextChainDepth = -1;
    int ternaryDepth = 0;
    bool justClosedDo = false;
    bool postTestTerminator = false;

    auto decision = [&](const std::wstring& name, const RawToken& t, int depth) {
        add(r.tokens, name, TokenKind::Operator, t);
        ++r.eta1;
        r.eta2 = std::max(r.eta2, depth);
    };
    auto statement = [&](const std::wstring& name, const RawToken& t) {
        add(r.tokens, name, TokenKind::Operand, t);
        ++r.N1;
    };
    auto schedule = [&](size_t i, BlockKind kind, int bodyDepth, int conditionDepth) {
        int brace = bodyBraceAfter(ts, i);
        if (brace >= 0) scheduled[(size_t)brace] = {kind, bodyDepth, conditionDepth};
        return brace;
    };

    for (size_t i = 0; i < ts.size(); ++i) {
        const RawToken& t = ts[i];
        const bool followsChain = nextChainDepth >= 0 && (t.text == L"elsif" || t.text == L"else");
        if (!followsChain) nextChainDepth = -1;
        const bool followsDo = justClosedDo && (t.text == L"while" || t.text == L"until");
        justClosedDo = false;

        if (t.kind == RawToken::Kind::Word) {
            if (t.text == L"sub") {
                statement(L"sub", t);
                continue;
            }
            if (t.text == L"given") {
                statement(L"given", t);
                schedule(i, BlockKind::Given, currentDepth, currentDepth);
                continue;
            }
            if (t.text == L"when") {
                int baseDepth = currentDepth;
                int branch = 0;
                for (auto b = blocks.rbegin(); b != blocks.rend(); ++b) {
                    if (b->kind == BlockKind::Given) {
                        baseDepth = b->previousDepth;
                        branch = b->whenCount++;
                        break;
                    }
                }
                decision(L"when", t, baseDepth + branch);
                statement(L"when", t);
                schedule(i, BlockKind::When, baseDepth + branch + 1, baseDepth + branch);
                continue;
            }
            if (t.text == L"default") {
                for (auto b = blocks.rbegin(); b != blocks.rend(); ++b) {
                    if (b->kind == BlockKind::Given) {
                        schedule(i, BlockKind::Default, b->previousDepth + b->whenCount, currentDepth);
                        break;
                    }
                }
                continue;
            }
            if (t.text == L"else") {
                if (followsChain) schedule(i, BlockKind::ElseBranch, nextChainDepth, nextChainDepth);
                nextChainDepth = -1;
                continue;
            }
            if (t.text == L"do") {
                schedule(i, BlockKind::Do, currentDepth, currentDepth);
                continue;
            }
            const bool isIf = t.text == L"if" || t.text == L"unless" || t.text == L"elsif";
            const bool isLoop = t.text == L"while" || t.text == L"until" ||
                                t.text == L"for" || t.text == L"foreach";
            if (isIf || isLoop) {
                const int depth = t.text == L"elsif" && followsChain ? nextChainDepth : currentDepth;
                decision(t.text, t, depth);
                statement(t.text, t);
                const BlockKind kind = t.text == L"elsif" ? BlockKind::ElsifBranch :
                    isIf ? BlockKind::IfBranch : BlockKind::Loop;
                const int brace = schedule(i, kind, depth + 1, depth);
                if (t.text == L"for" || t.text == L"foreach")
                    markForHeaderSemicolons(ts, i, brace, forHeaderSemicolons);
                if (followsDo && brace < 0) postTestTerminator = true;
                nextChainDepth = -1;
                continue;
            }
        }

        if (t.text == L"?") {
            decision(L"?:", t, currentDepth + ternaryDepth);
            ++ternaryDepth;
            continue;
        }
        if (t.text == L":" && ternaryDepth > 0) { --ternaryDepth; continue; }
        if (t.kind == RawToken::Kind::OpenBrace) {
            Body body{BlockKind::Other, currentDepth, currentDepth};
            auto it = scheduled.find(i);
            if (it != scheduled.end()) body = it->second;
            blocks.push_back({body.kind, currentDepth, body.conditionDepth, 0});
            currentDepth = body.depth;
            continue;
        }
        if (t.kind == RawToken::Kind::CloseBrace) {
            if (!blocks.empty()) {
                Block closed = blocks.back();
                blocks.pop_back();
                currentDepth = closed.previousDepth;
                if (closed.kind == BlockKind::IfBranch || closed.kind == BlockKind::ElsifBranch)
                    nextChainDepth = closed.conditionDepth + 1;
                if (closed.kind == BlockKind::Do) justClosedDo = true;
            }
            continue;
        }
        if (t.kind == RawToken::Kind::Semicolon) {
            if (forHeaderSemicolons.count(i)) continue;
            if (postTestTerminator) { postTestTerminator = false; continue; }
            statement(L"простой оператор", t);
            ternaryDepth = 0;
        }
    }
    r.V = r.N1 ? (double)r.eta1 / r.N1 : 0.0;
    for (wchar_t c : code) if (c == L'\n') ++r.lineCount;
    if (!code.empty() && code.back() != L'\n') ++r.lineCount;
    makeEntries(r); return r;
}
