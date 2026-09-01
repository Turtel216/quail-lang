#include "mangler.hpp"


std::string Mangler::newMangledName(const std::string& n) {
    auto occurenceIt = occurenceCount.find(n);
    int occurence = 0;
    if(occurenceIt != occurenceCount.end()) {
        occurence = occurenceIt->second + 1;
    }
    occurenceCount[n] = occurence;

    std::string finalName = n;
    if (occurence != 0) {
        finalName += "_";
        finalName += std::to_string(occurence);
    }
    return finalName;
}
