#include <iostream>
#include "format.hpp"
using namespace std;

int main()
{
    log::Format fmt("test%%%[test1][%d%{][%t][%p][%c][%f:%l] %m%n");
    log::LogMsg msg("ljx", "test.cc", 8, "This is just a test", log::Level::INFO);
    cout << fmt.format(msg) << endl;
    return 0;
}