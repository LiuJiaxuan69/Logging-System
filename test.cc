#include <iostream>
#include "util.hpp"
#include "level.hpp"
using namespace std;

int main()
{
    cout << log::toString(log::Level::DEBUG) << endl;
    cout << log::toString(log::Level::INFO) << endl;
    cout << log::toString(log::Level::WARNING) << endl;
    cout << log::toString(log::Level::ERROR) << endl;
    cout << log::toString(log::Level::FATAL) << endl;
    cout << log::toString(log::Level::OFF) << endl;
    cout << log::toString(log::Level::UNKNOW) << endl;
    return 0;
}