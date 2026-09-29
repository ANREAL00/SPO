#pragma once

#include <string>
#include <vector>

enum class TokenKind { Operator, Operand };

struct Token { std::wstring name; TokenKind kind; int start; int length; };
struct Entry { std::wstring name; int count = 0; std::vector<int> tokenIndexes; };

struct AnalysisResult
{
    std::vector<Token> tokens;
    std::vector<Entry> operators; // виды ветвлений
    std::vector<Entry> operands;  // учитываемые операторы программы
    int eta1 = 0;       // CL - абсолютная сложность Джилба
    int N1 = 0;         // общее количество операторов
    int eta2 = 0;       // CLI - максимальная вложенность (внешний уровень = 0)
    int N2 = 0;         // оценка Маккейба по точкам решения и компонентам
    int eta = 0;        // количество точек решения
    int N = 0;          // компоненты связности
    double V = 0.0;     // cl = CL / N1
    int lineCount = 0;
};

class PerlControlFlowAnalyzer
{
public:
    AnalysisResult analyze(const std::wstring& code);
};
