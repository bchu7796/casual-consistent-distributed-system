#include "version.hpp"

// version's data structure
version::version():timestamp(-1), datacenter_id(-1){}
version::version(int server_local_time, int server_id):timestamp(server_local_time), datacenter_id(server_id){}
bool version::operator==(version &a){
    if(this->timestamp == a.timestamp && this->datacenter_id == a.datacenter_id){
        return true;
    }
    else{
        return false;
    }
}
int version::get_timestamp() const{
    return timestamp;
}
void version::write_timestamp(int time){
    timestamp = time;
}

int version::get_datacenterid() const{
    return this->datacenter_id;
}

void version::write_datacenterid(int id){
    this->datacenter_id = id;
}
