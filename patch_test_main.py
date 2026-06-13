import os

test_main_cpp = "tests/test_main.cpp"
with open(test_main_cpp, "r") as f:
    content = f.read()

join_test = """
void TestJoin() {
    std::cout << "Running Join Tests..." << std::endl;
    // create users.csv
    FILE* fu = fopen("../tests/users.csv", "w");
    if (!fu) fu = fopen("tests/users.csv", "w");
    if (fu) {
        fprintf(fu, "id,name\\n1,Alice\\n2,Bob\\n3,Charlie\\n");
        fclose(fu);
    }
    
    // create orders.csv
    FILE* fo = fopen("../tests/orders.csv", "w");
    if (!fo) fo = fopen("tests/orders.csv", "w");
    if (fo) {
        fprintf(fo, "order_id,user_id,item\\n101,1,Apple\\n102,1,Banana\\n103,2,Carrot\\n104,4,Date\\n");
        fclose(fo);
    }
    
    try {
        std::string query = "from ../tests/users.csv join ../tests/orders.csv on id == user_id select name, item";
        qle::lexer::Lexer lexer(query);
        qle::parser::Parser parser(lexer.Tokenize());
        qle::runtime::Runtime rt;
        rt.Execute(parser.Parse().get());
    } catch (const std::exception& e) {
        std::cerr << "Join test failed (../tests/): " << e.what() << std::endl;
        try {
            std::string query = "from tests/users.csv join tests/orders.csv on id == user_id select name, item";
            qle::lexer::Lexer lexer(query);
            qle::parser::Parser parser(lexer.Tokenize());
            qle::runtime::Runtime rt;
            rt.Execute(parser.Parse().get());
        } catch (const std::exception& e) {
            std::cerr << "Join test failed (tests/): " << e.what() << std::endl;
        }
    }
}
"""

if "void TestJoin()" not in content:
    content = content.replace("void TestSqliteAdapter();", "void TestSqliteAdapter();\nvoid TestJoin();")
    content = content.replace("TestReplEdgeCases();", "TestReplEdgeCases();\n    TestJoin();")
    content += join_test

with open(test_main_cpp, "w") as f:
    f.write(content)

