// life_patterns.cpp

#include <content/life_patterns.hpp>

namespace hex::plugin::builtin::life {

namespace {

const char* patterns[] = {
    // Gosper's glider gun
    "24bo$22bobo$12b2o6b2o12b2o$11bo3bo4b2o12b2o$2o8bo5bo3b2o$2o8bo3bob2o4b"
    "obo$10bo5bo7bo$11bo3bo$12b2o!"
};

} // namespace {

size_t numPatterns() {
    return sizeof(patterns)/sizeof(const char*);
}

const char* getPattern(int idx) {
    return patterns[idx];
}

} // namespace hex::plugin::builtin::life {
