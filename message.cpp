#include "message.hpp"

//message's data structure
message::message(){}
message::message(std::string update_key, std::string update_value, version update_key_version, std::vector<std::pair<std::string, version> > received_dependencies):
key(update_key), value(update_value), key_version(update_key_version), dependencies(received_dependencies){}

bool operator < (message const &a, message const &b){
    return a.key_version.get_timestamp() < b.key_version.get_timestamp();
}

