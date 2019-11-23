#include <iostream>
#include <stdio.h>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <string>
#include <vector>
#include "server.hpp"

int main(void){
    server datacenter(1000, "127.0.0.1", 8080);
    datacenter.listen_client();
}
