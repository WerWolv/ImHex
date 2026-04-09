// life_patterns.cpp

#include <content/life_patterns.hpp>

namespace hex::plugin::builtin::life {

namespace {

const char* patterns[] = {
    // Gosper's glider gun
    "24bo$22bobo$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o$2o8bo3bob2o4b"
    "obo$10bo5bo7bo$11bo3bo$12b2o!"
    ,
    // Space ships
    "bo16b3o$o5bo10b5o$o5bo9b2ob3o$5obo10b2o2$4b2o8bo$2bo4bo5bo$bo11bo4bo$b"
    "o5bo5b5o$b6o7$3bo$bo3bo$o$o4bo9b6o$5o10bo5b2o$15bo5bob2o$5b2o9bo3bo$5b"
    "2o11bo$17b2o$6b2o8b4o$4bo4bo5b2ob2o$3bo12b2o$3bo5bo$3b6o!"
};

} // namespace {

size_t numPatterns() {
    return sizeof(patterns)/sizeof(const char*);
}

const char* getPattern(int idx) {
    return patterns[idx];
}

} // namespace hex::plugin::builtin::life {
