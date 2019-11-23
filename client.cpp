#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <strings.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define CLIENT_ID 0

std::mutex mtx;

struct sockaddr_in server_address;

void split(const std::string& s, std::vector<std::string>& parameters, const char delim = ' '){
    parameters.clear();
    std::istringstream iss(s);
    std::string temp;

    while (std::getline(iss, temp, delim)) {
        parameters.emplace_back(std::move(temp));
    }
    return;
}

int write_request(std::string key, std::string value){
    /*
    send:
        request type
        client_id
        the written key
        the written value
    receive:
        OK
    */
    int client_socket;
    std::string response_message;
    const uint32_t request_type = 1;
    std::string reponse_message;

    //create socket
    if((client_socket = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket");
        exit(0);
    }

    //connect to server
    if(connect(client_socket,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to connect");
        exit(0);
    }

    //write (key,value)
    int client_id = CLIENT_ID;
    send(client_socket, &client_id, sizeof(client_id), 0);
    send(client_socket, &request_type, sizeof(request_type), 0);
    send(client_socket, &key, sizeof(key), 0);
    send(client_socket, &value, sizeof(value), 0);
    if(recv(client_socket, &response_message, sizeof(response_message), 0) < 0){
        perror("server response failed");
        return -1;
    }
    std::cout << "server response: " << response_message << std::endl;
    close(client_socket);
    return 0;
}


int read_request(std::string key){
    /*
    send:
        request type
        client_id
        key
    receive:
        'found' if server found (key,value)
        'no record' else        
    */
    int client_socket;
    const uint32_t request_type = 0;
    std::string response_message;

    //create socket
    if((client_socket = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket");
        exit(0);
    }

    //connect to server
    if(connect(client_socket,(struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to connect");
        exit(0);
    }

    //read request
    int client_id = CLIENT_ID;
    send(client_socket, &client_id, sizeof(client_id), 0);
    send(client_socket, &request_type, sizeof(request_type), 0);
    send(client_socket, &key, sizeof(key), 0);

    //wait for response
    if(recv(client_socket, &response_message, sizeof(response_message), 0) < 0){
        perror("server response failed");
        return -1;
    }
    std::cout << "server response: " << response_message << std::endl;
    close(client_socket);
    return 0;
}

int parse_command(std::string command){
    std::vector<std::string> parameters;

    split(command, parameters);
    if(parameters[0] == "read"){
        read_request(parameters[1]);
    }
    else if(parameters[0] == "write"){
	    write_request(parameters[1], parameters[2]);
    }
    return 0;
}

void user_interface(){
    char buffer[1024];
    while(1){
        std::string command;
        std::cout<<">";
        std::cin.getline(buffer, 1024);
        command = buffer;
        parse_command(command);
    }
    return;
}

int main(void){
    //initailize address structure
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = PF_INET; 
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP); 
    server_address.sin_port = htons(PORT);

    user_interface();
    return 0;
}
