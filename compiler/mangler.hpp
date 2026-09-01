#pragma once 

#include <map>
#include <string>
class Mangler {
  private:
    std::map<std::string, int> occurenceCount;

  public:
    std::string newMangledName(const std::string& str);
};
