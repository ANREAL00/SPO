#include "PerlBoundaryAnalyzer.h"

#include <algorithm>
#include <cwctype>
#include <iterator>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

struct Token {
    std::wstring text;
    size_t start;
    size_t end;
    int line;
};

struct Edge {
    int to;
    std::string label;
};

struct Node {
    int id;
    std::wstring kind;
    std::wstring label;
    std::vector<Edge> edges;
};

struct Port {
    int id;
    std::string label;
};

struct Fragment {
    int entry = 0;
    std::vector<Port> exits;
};

std::wstring trim(const std::wstring& text) {
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    const size_t last = text.find_last_not_of(L" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool wordStart(wchar_t c) { return std::iswalpha(c) || c == L'_'; }
bool wordPart(wchar_t c) { return std::iswalnum(c) || c == L'_'; }

std::vector<Token> lex(const std::wstring& source) {
    std::vector<Token> result;
    size_t i = 0;
    int line = 1;
    while (i < source.size()) {
        const wchar_t c = source[i];
        if (c == L'\n') { ++line; ++i; continue; }
        if (std::iswspace(c)) { ++i; continue; }
        if (c == L'#') {
            while (i < source.size() && source[i] != L'\n') ++i;
            continue;
        }
        const size_t begin = i;
        const int beginLine = line;
        if (c == L'\'' || c == L'"' || c == L'`') {
            const wchar_t quote = c;
            ++i;
            bool closed = false;
            while (i < source.size()) {
                if (source[i] == L'\\' && i + 1 < source.size()) { i += 2; continue; }
                if (source[i] == L'\n') ++line;
                if (source[i++] == quote) { closed = true; break; }
            }
            if (!closed) throw std::runtime_error("Unclosed string at line " + std::to_string(beginLine));
        } else if ((c == L'$' || c == L'@' || c == L'%') &&
                   i + 1 < source.size() && wordPart(source[i + 1])) {
            i += 2;
            while (i < source.size() && wordPart(source[i])) ++i;
        } else if (wordStart(c)) {
            ++i;
            while (i < source.size() && wordPart(source[i])) ++i;
        } else {
            static const wchar_t* multi[] = {
                L"<=>", L"++", L"--", L"+=", L"-=", L".=", L"==", L"!=",
                L">=", L"<=", L"&&", L"||", L"=>", L"=~", L"!~", L"//"
            };
            bool matched = false;
            for (const wchar_t* op : multi) {
                const std::wstring candidate(op);
                if (source.compare(i, candidate.size(), candidate) == 0) {
                    i += candidate.size();
                    matched = true;
                    break;
                }
            }
            if (!matched) ++i;
        }
        result.push_back({source.substr(begin, i - begin), begin, i, beginLine});
    }
    result.push_back({L"<eof>", source.size(), source.size(), line});
    return result;
}

class Parser {
public:
    explicit Parser(const std::wstring& code) : source(code), tokens(lex(code)) {
        addNode(L"start", L"Start");
    }

    std::map<int, Node> parse() {
        const Fragment body = sequence(false);
        if (!at(L"<eof>")) fail("Unexpected token after program");
        const int end = addNode(L"end", L"End");
        if (body.entry) {
            addEdge(1, body.entry);
            connect(body.exits, end);
        } else {
            addEdge(1, end);
        }
        return nodes;
    }

private:
    const std::wstring& source;
    const std::vector<Token> tokens;
    size_t pos = 0;
    std::map<int, Node> nodes;

    bool at(const wchar_t* text) const { return tokens[pos].text == text; }
    bool accept(const wchar_t* text) { if (!at(text)) return false; ++pos; return true; }
    void expect(const wchar_t* text) {
        if (!accept(text)) fail("Expected a different Perl symbol");
    }
    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string(message) + " at line " +
                                 std::to_string(tokens[pos].line));
    }
    std::wstring text(size_t first, size_t afterLast) const {
        if (first >= afterLast) return L"";
        return trim(source.substr(tokens[first].start,
                                  tokens[afterLast - 1].end - tokens[first].start));
    }
    int addNode(const std::wstring& kind, const std::wstring& label) {
        const int id = static_cast<int>(nodes.size()) + 1;
        nodes.emplace(id, Node{id, kind, label, {}});
        return id;
    }
    void addEdge(int from, int to, const std::string& label = "") {
        nodes.at(from).edges.push_back({to, label});
    }
    void connect(const std::vector<Port>& exits, int target) {
        for (const auto& exit : exits) addEdge(exit.id, target, exit.label);
    }
    static void append(std::vector<Port>& target, const std::vector<Port>& more) {
        target.insert(target.end(), more.begin(), more.end());
    }
    Fragment process(const std::wstring& label) {
        const int id = addNode(L"process", label);
        return {id, {{id, ""}}};
    }
    Fragment sequence(bool braced) {
        Fragment combined;
        while (!at(L"<eof>") && !(braced && at(L"}"))) {
            if (!braced && at(L"}")) fail("Unexpected closing brace");
            Fragment part = statement();
            if (!part.entry) continue;
            if (!combined.entry) combined = std::move(part);
            else {
                connect(combined.exits, part.entry);
                combined.exits = std::move(part.exits);
            }
        }
        if (braced && at(L"<eof>")) fail("Unclosed block");
        return combined;
    }
    Fragment block() {
        expect(L"{");
        Fragment result = sequence(true);
        expect(L"}");
        return result;
    }
    std::pair<size_t, size_t> parentheses() {
        expect(L"(");
        const size_t first = pos;
        int depth = 1;
        while (depth && !at(L"<eof>")) {
            if (at(L"(")) ++depth;
            else if (at(L")")) --depth;
            if (depth) ++pos;
        }
        if (depth) fail("Unclosed parentheses");
        const size_t last = pos;
        ++pos;
        return {first, last};
    }
    std::wstring condition() {
        const auto range = parentheses();
        return text(range.first, range.second);
    }
    Fragment statement() {
        if (accept(L";")) return {};
        if (at(L"{")) return block();
        if (at(L"if") || at(L"unless")) return conditional();
        if (at(L"while") || at(L"until")) return preTestLoop();
        if (at(L"do")) return postTestLoop();
        if (at(L"foreach")) return foreachLoop();
        if (at(L"for")) {
            if (pos + 1 < tokens.size() && tokens[pos + 1].text == L"(") return forLoop();
            return foreachLoop();
        }
        if (at(L"given")) return given();
        if (at(L"elsif") || at(L"else") || at(L"when") || at(L"default"))
            fail("Branch without its opening statement");
        return simple();
    }
    Fragment conditional() {
        const bool unless = accept(L"unless");
        if (!unless) expect(L"if");
        const int first = addNode(L"decision", (unless ? L"unless (" : L"if (") + condition() + L")");
        Fragment branch = block();
        std::vector<Port> exits;
        const std::string bodyLabel = unless ? "no" : "yes";
        const std::string skipLabel = unless ? "yes" : "no";
        if (branch.entry) { addEdge(first, branch.entry, bodyLabel); append(exits, branch.exits); }
        else exits.push_back({first, bodyLabel});
        int previous = first;
        while (accept(L"elsif")) {
            const int next = addNode(L"decision", L"elsif (" + condition() + L")");
            addEdge(previous, next, skipLabel);
            branch = block();
            if (branch.entry) { addEdge(next, branch.entry, "yes"); append(exits, branch.exits); }
            else exits.push_back({next, "yes"});
            previous = next;
        }
        if (accept(L"else")) {
            branch = block();
            if (branch.entry) { addEdge(previous, branch.entry, skipLabel); append(exits, branch.exits); }
            else exits.push_back({previous, skipLabel});
        } else exits.push_back({previous, skipLabel});
        return {first, exits};
    }
    Fragment preTestLoop() {
        const bool until = accept(L"until");
        if (!until) expect(L"while");
        const int test = addNode(L"decision", (until ? L"until (" : L"while (") + condition() + L")");
        Fragment body = block();
        const std::string repeat = until ? "no" : "yes";
        const std::string leave = until ? "yes" : "no";
        if (body.entry) { addEdge(test, body.entry, repeat); connect(body.exits, test); }
        else addEdge(test, test, repeat);
        return {test, {{test, leave}}};
    }
    Fragment postTestLoop() {
        expect(L"do");
        Fragment body = block();
        if (!at(L"while") && !at(L"until")) fail("Expected while or until after do block");
        const bool until = accept(L"until");
        if (!until) expect(L"while");
        const int test = addNode(L"decision", (until ? L"until (" : L"while (") + condition() + L")");
        expect(L";");
        if (!body.entry) body = process(L"empty do body");
        connect(body.exits, test);
        addEdge(test, body.entry, until ? "no" : "yes");
        return {body.entry, {{test, until ? "yes" : "no"}}};
    }
    Fragment forLoop() {
        expect(L"for");
        const auto range = parentheses();
        std::vector<size_t> parts{range.first};
        int depth = 0;
        for (size_t i = range.first; i < range.second; ++i) {
            if (tokens[i].text == L"(") ++depth;
            else if (tokens[i].text == L")") --depth;
            else if (tokens[i].text == L";" && depth == 0) parts.push_back(i + 1);
        }
        if (parts.size() != 3) fail("for needs initialization, condition, and step");
        const std::wstring init = text(parts[0], parts[1] - 1);
        const std::wstring check = text(parts[1], parts[2] - 1);
        const std::wstring stepText = text(parts[2], range.second);
        if (check.empty()) fail("A for loop without a condition is not supported");
        const int initId = init.empty() ? 0 : addNode(L"process", init);
        const int test = addNode(L"decision", L"for (" + check + L")");
        if (initId) addEdge(initId, test);
        Fragment body = block();
        const int step = stepText.empty() ? 0 : addNode(L"process", stepText);
        const int repeatTarget = body.entry ? body.entry : step ? step : test;
        addEdge(test, repeatTarget, "yes");
        if (body.entry) connect(body.exits, step ? step : test);
        if (step) addEdge(step, test);
        return {initId ? initId : test, {{test, "no"}}};
    }
    Fragment foreachLoop() {
        ++pos; // foreach, or for used as its synonym
        const size_t headerBegin = pos;
        int parens = 0;
        while (!at(L"<eof>")) {
            if (at(L"(") ) ++parens;
            else if (at(L")")) --parens;
            if (at(L"{") && parens == 0) break;
            ++pos;
        }
        if (at(L"<eof>")) fail("Expected foreach body");
        const std::wstring header = text(headerBegin, pos);
        std::wstring variable = L"item";
        for (size_t i = headerBegin; i < pos; ++i) {
            if (!tokens[i].text.empty() && tokens[i].text[0] == L'$') {
                variable = tokens[i].text;
                break;
            }
        }
        const int init = addNode(L"process", L"foreach iterator: " + header);
        const int test = addNode(L"decision", L"next item exists?");
        const int assign = addNode(L"process", variable + L" = next item");
        addEdge(init, test);
        addEdge(test, assign, "yes");
        Fragment body = block();
        const int advance = addNode(L"process", L"advance iterator");
        addEdge(assign, body.entry ? body.entry : advance);
        if (body.entry) connect(body.exits, advance);
        addEdge(advance, test);
        return {init, {{test, "no"}}};
    }
    Fragment given() {
        expect(L"given");
        const int selector = addNode(L"process", L"given (" + condition() + L")");
        expect(L"{");
        std::vector<Port> exits;
        int previous = 0;
        bool hasDefault = false;
        while (!at(L"}") && !at(L"<eof>")) {
            if (accept(L"when")) {
                if (hasDefault) fail("when after default");
                const int test = addNode(L"decision", L"when (" + condition() + L")");
                if (previous) addEdge(previous, test, "no");
                else addEdge(selector, test);
                Fragment branch = block();
                if (branch.entry) { addEdge(test, branch.entry, "yes"); append(exits, branch.exits); }
                else exits.push_back({test, "yes"});
                previous = test;
            } else if (accept(L"default")) {
                if (hasDefault) fail("Duplicate default branch");
                hasDefault = true;
                Fragment branch = block();
                if (branch.entry) {
                    if (previous) addEdge(previous, branch.entry, "no");
                    else addEdge(selector, branch.entry);
                    append(exits, branch.exits);
                } else if (previous) exits.push_back({previous, "no"});
                else exits.push_back({selector, ""});
            } else fail("Only when and default are supported inside given");
        }
        expect(L"}");
        if (!hasDefault && previous) exits.push_back({previous, "no"});
        else if (!hasDefault && !previous) exits.push_back({selector, ""});
        return {selector, exits};
    }
    Fragment simple() {
        const size_t first = pos;
        int parens = 0;
        while (!at(L"<eof>")) {
            if (at(L"(")) ++parens;
            else if (at(L")")) --parens;
            if (at(L";") && parens == 0) break;
            if (at(L"}") && parens == 0) fail("Expected semicolon");
            ++pos;
        }
        if (!at(L";")) fail("Expected semicolon");
        const size_t last = pos++;
        if (first == last) return {};
        static const wchar_t* unsupported[] = {L"next", L"last", L"redo", L"goto", L"return", L"exit", L"die"};
        for (const wchar_t* word : unsupported)
            if (tokens[first].text == word) fail("This control transfer is not supported");

        parens = 0;
        for (size_t i = first; i < last; ++i) {
            if (tokens[i].text == L"(") ++parens;
            else if (tokens[i].text == L")") --parens;
            else if (parens == 0 && (tokens[i].text == L"unless" || tokens[i].text == L"if")) {
                const bool unless = tokens[i].text == L"unless";
                const int test = addNode(L"decision", tokens[i].text + L" (" + text(i + 1, last) + L")");
                const int action = addNode(L"process", text(first, i));
                addEdge(test, action, unless ? "no" : "yes");
                return {test, {{action, ""}, {test, unless ? "yes" : "no"}}};
            }
        }

        size_t question = last, colon = last, assignment = last;
        parens = 0;
        for (size_t i = first; i < last; ++i) {
            if (tokens[i].text == L"(") ++parens;
            else if (tokens[i].text == L")") --parens;
            else if (parens == 0 && tokens[i].text == L"=" && assignment == last) assignment = i;
            else if (parens == 0 && tokens[i].text == L"?" && question == last) question = i;
            else if (parens == 0 && tokens[i].text == L":" && question != last && colon == last) colon = i;
        }
        if (question != last) {
            if (colon == last) fail("Ternary operator needs colon");
            const std::wstring prefix = assignment < question ? text(first, assignment + 1) + L" " : L"";
            const std::wstring check = text(assignment < question ? assignment + 1 : first, question);
            const int test = addNode(L"decision", check + L" ?:");
            const int yes = addNode(L"process", prefix + text(question + 1, colon));
            const int no = addNode(L"process", prefix + text(colon + 1, last));
            addEdge(test, yes, "yes");
            addEdge(test, no, "no");
            return {test, {{yes, ""}, {no, ""}}};
        }
        return process(text(first, last));
    }
};

void validate(const std::map<int, Node>& graph) {
    if (graph.empty()) throw std::runtime_error("Empty graph");
    std::map<int, std::vector<int>> reverse;
    for (const auto& pair : graph) {
        const Node& node = pair.second;
        if (node.kind == L"end" && !node.edges.empty())
            throw std::runtime_error("End vertex has an outgoing edge");
        if (node.kind != L"end" && node.edges.empty())
            throw std::runtime_error("Vertex has no path to the end");
        for (const auto& edge : node.edges) {
            if (!graph.count(edge.to)) throw std::runtime_error("Unknown graph vertex");
            reverse[edge.to].push_back(node.id);
        }
    }
    auto visit = [](int root, const auto& next) {
        std::set<int> seen{root};
        std::queue<int> pending;
        pending.push(root);
        while (!pending.empty()) {
            int id = pending.front(); pending.pop();
            for (int target : next(id)) if (seen.insert(target).second) pending.push(target);
        }
        return seen;
    };
    const auto reachable = visit(1, [&](int id) {
        std::vector<int> targets;
        for (const auto& edge : graph.at(id).edges) targets.push_back(edge.to);
        return targets;
    });
    const auto canExit = visit(static_cast<int>(graph.size()), [&](int id) { return reverse[id]; });
    if (reachable.size() != graph.size()) throw std::runtime_error("Some statements are unreachable");
    if (canExit.size() != graph.size()) throw std::runtime_error("Some statements cannot reach the end");
}

BoundaryReport measure(const std::map<int, Node>& graph) {
    validate(graph);
    BoundaryReport report;
    report.vertices = static_cast<int>(graph.size());
    const int end = report.vertices;
    std::set<int> all;
    for (const auto& pair : graph) all.insert(pair.first);
    std::map<int, std::set<int>> postdom;
    for (const auto& pair : graph)
        postdom[pair.first] = pair.first == end ? std::set<int>{end} : all;
    bool changed;
    do {
        changed = false;
        for (const auto& pair : graph) {
            const Node& node = pair.second;
            if (node.id == end) continue;
            std::set<int> common = postdom.at(node.edges.front().to);
            for (size_t i = 1; i < node.edges.size(); ++i) {
                std::set<int> intersection;
                const auto& other = postdom.at(node.edges[i].to);
                std::set_intersection(common.begin(), common.end(), other.begin(), other.end(),
                                      std::inserter(intersection, intersection.begin()));
                common = std::move(intersection);
            }
            common.insert(node.id);
            if (common != postdom[node.id]) { postdom[node.id] = std::move(common); changed = true; }
        }
    } while (changed);

    for (const auto& pair : graph) {
        const Node& node = pair.second;
        int complexity = node.id == end ? 0 : 1;
        if (node.kind == L"decision") {
            std::set<int> strict = postdom.at(node.id);
            strict.erase(node.id);
            int boundary = 0;
            for (int candidate : strict) {
                bool closest = true;
                for (int other : strict)
                    if (other != candidate && !postdom.at(candidate).count(other)) closest = false;
                if (closest) {
                    if (boundary) throw std::runtime_error("Ambiguous decision boundary");
                    boundary = candidate;
                }
            }
            if (!boundary) throw std::runtime_error("Missing decision boundary");
            std::set<int> inside;
            std::queue<int> pending;
            for (const auto& edge : node.edges) pending.push(edge.to);
            while (!pending.empty()) {
                const int current = pending.front(); pending.pop();
                if (current == boundary || !inside.insert(current).second) continue;
                for (const auto& edge : graph.at(current).edges) pending.push(edge.to);
            }
            complexity = static_cast<int>(inside.size()) + 1;
            BoundaryChoice choice;
            choice.id = node.id;
            choice.boundary = boundary;
            choice.complexity = complexity;
            for (const auto& edge : node.edges) choice.successors.push_back(edge.to);
            for (int id : inside) choice.subgraph.push_back(id);
            report.choices.push_back(std::move(choice));
        }
        report.nodes.push_back({node.id, node.kind, node.label, complexity});
        report.Sa += complexity;
    }
    report.So = report.Sa ? 1.0 - static_cast<double>(report.vertices - 1) / report.Sa : 0.0;
    return report;
}

} // namespace

BoundaryReport analyzePerlBoundary(const std::wstring& source) {
    Parser parser(source);
    return measure(parser.parse());
}
