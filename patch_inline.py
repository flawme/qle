with open('tests/test_main.cpp', 'r') as f:
    content = f.read()

import re

# find TestInlineFunctions
old_func = """void TestInlineFunctions() {
    std::cout << "Running Inline Functions Tests..." << std::endl;
    std::string queries[] = {
        "from test.csv select upper()",
        "from test.csv select lower(\\"abc\\", \\"def\\")",
        "from test.csv select length()",
        "from test.csv select upper(\\"\\xFF\\xFF\\")",
        "from test.csv select non_existent_func(1)"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}"""

new_func = """void TestInlineFunctions() {
    std::cout << "Running Inline Functions Tests..." << std::endl;
    
    std::ofstream tmp("tests/math_test.csv");
    tmp << "id,val\\n1,10.5\\n2,-5.2\\n3,inf\\n4,nan\\n5,1e1000\\n6,not_a_number\\n";
    tmp.close();

    std::string queries[] = {
        "from tests/math_test.csv select upper()",
        "from tests/math_test.csv select lower(\\"abc\\", \\"def\\")",
        "from tests/math_test.csv select length()",
        "from tests/math_test.csv select upper(\\"\\xFF\\xFF\\")",
        "from tests/math_test.csv select non_existent_func(1)",
        "from tests/math_test.csv select abs(val)",
        "from tests/math_test.csv select round(val)",
        "from tests/math_test.csv select abs(\\"extremely_long_string_that_is_not_a_number_at_all\\")"
    };
    for (const auto& q : queries) {
        try {
            lexer::Lexer lexer(q);
            parser::Parser parser(lexer.Tokenize());
            runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const errors::QleException&) {}
    }
}"""

if old_func in content:
    content = content.replace(old_func, new_func)
else:
    print("Could not find old TestInlineFunctions")

with open('tests/test_main.cpp', 'w') as f:
    f.write(content)
