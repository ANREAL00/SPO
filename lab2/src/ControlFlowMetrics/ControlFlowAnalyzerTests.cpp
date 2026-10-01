#include "PerlControlFlowAnalyzer.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    PerlControlFlowAnalyzer a;
    auto nested = a.analyze(L"if ($a) { while ($b) { $x++; } } else { $x--; }\n");
    assert(nested.eta1 == 2);
    assert(nested.eta2 == 1);
    assert(nested.N1 == 4); // два простых и два управляющих оператора
    assert(std::abs(nested.V - 0.5) < 1e-9);

    auto ignored = a.analyze(L"# if while ;\nprint \"if; while\";\n");
    assert(ignored.eta1 == 0 && ignored.N1 == 1);

    auto regex = a.analyze(L"my $x = $s =~ /^if\\d+(?:while)?$/ ? 1 : 0;\n"
                           L"$s =~ s/if\\?/#while/g;\n$s =~ tr/a-z/A-Z/;\n"
                           L"my $q = qr{if(?:while)?};\n");
    assert(regex.eta1 == 1 && regex.N1 == 4);

    auto spacedQuotes = a.analyze(L"my $a = q {if while}; my $b = m /if\\?while/; "
                                  L"$a =~ s {if?} {while};\n");
    assert(spacedQuotes.eta1 == 0 && spacedQuotes.N1 == 3);

    auto chain = a.analyze(L"sub grade { if ($x) { return 1; } elsif ($y) { return 2; } "
                           L"elsif ($z) { return 3; } else { if ($w) { return 4; } } }");
    assert(chain.eta1 == 4);
    assert(chain.eta2 == 3); // вложенное условие в ветви else
    assert(chain.N1 == 9);  // sub, четыре условия и четыре return

    auto choice = a.analyze(L"given ($x) { when (1) { f(); } when (2) { g(); } when (3) { h(); } default { z(); } }");
    assert(choice.eta1 == 3); // given/default не условия
    assert(choice.eta2 == 2); // n=4 ветви -> n-2=2
    assert(choice.N1 == 8); // given + три when + четыре вызова

    auto choiceNested = a.analyze(L"given ($x) { when (1) { a(); } when (2) { if ($y) { b(); } } default { c(); } }");
    assert(choiceNested.eta1 == 3 && choiceNested.eta2 == 2);

    auto loop = a.analyze(L"for (my $i=0; $i<3; $i++) { print $i; }");
    assert(loop.eta1 == 1 && loop.N1 == 2); // ';' заголовка не операторы программы
    auto postLoop = a.analyze(L"do { $x++; } while ($x < 3);");
    assert(postLoop.eta1 == 1 && postLoop.N1 == 2);
    auto shortCircuit = a.analyze(L"if ($a && $b) { print 1; }");
    assert(shortCircuit.eta1 == 1 && shortCircuit.N1 == 2);
    auto empty = a.analyze(L"");
    assert(empty.eta1 == 0 && empty.N1 == 0);
    std::wcout << L"All analyzer tests passed\n";
}
