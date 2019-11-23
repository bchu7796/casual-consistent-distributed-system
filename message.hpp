#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <string>
#include <utility>
#include <vector>
#include "version.hpp"

//message's data structure
struct message{
    message();
    message(std::string, std::string, version, std::vector<std::pair<std::string, version> >);
    friend bool operator < (message const &, message const &);
    std::string key;
    std::string value;
    version key_version;
    std::vector<std::pair<std::string, version> > dependencies;
};

#endif