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
#include <thread>
#include <algorithm>
#include "server.hpp"

server::server():server_id(-1), local_time(0){}

server::server(int id, std::string ip, int port):server_id(id), local_time(0), server_ip(ip), server_port(port){}

//Create a variable when write to a non-exist key.
int server::create_variables(std::string key, std::string value, std::vector<std::string> &vars, std::map<std::string, int> &var_map){
    int index;
    std::pair<std::map<std::string,int>::iterator, bool> ret;
    
    //Add variable's value to the vector
    vars.push_back(value);
    index = vars.size() - 1;
    
    //Map the key to the index of the vector
    ret = var_map.insert (std::pair<std::string, int>(key, index));
    if (ret.second==false) {
        //key already in the map
        std::cout << "variable with key " << key << " already existed";
        std::cout << " with a value of " << ret.first->second << '\n';
    }
    else{
        //create a variable
        std::cout << "Create Variable: " << key << "\n";
    }
    return 0;
}

//Update dependency for client_read and client_write
int server::update_dependency(int client_id, std::string key, int operation){
    std::map<int, std::map<std::string, version> >::iterator dependency_it;
    std::pair<std::map<std::string, version>::iterator, bool> ret;
    
    dependency_it = dependency.find(client_id);
    if(dependency_it == dependency.end()){
        //client dependency not exist
        perror("dependency error");
        return -1;
    }
    else{
        //client dependency exist
        version latest_version(local_time, server_id);
        if(operation == 1){
            //write operation
            dependency_it->second.clear();
        }
        else{
            // read operation. Find the latest write to the key
            std::vector<std::pair<std::string, version> >::iterator it;
            for(it = commited_messages.end() - 1; it >= commited_messages.begin(); it--){
                if(key == it->first){
                    latest_version.write_timestamp(it->second.get_timestamp());
                    latest_version.write_datacenterid(it->second.get_datacenterid());
                    break;
                }
            }
        }
        ret = dependency_it->second.insert(std::pair<std::string, version>(key, latest_version));
        if (ret.second == false) {
            //the key's dependency already exist
            //update version
            ret.first->second = latest_version;
        }
        std::cout << "update client " << client_id <<" dependency: ";
        std::cout << "<" << key << ", ";
        std::cout << latest_version.get_timestamp() << ", " << latest_version.get_datacenterid() <<">" << "\n";
    }
    return 0;
}

//Send replica of write to other datacenters
int server::replicated_write(const char * datacenter_ip, int datacenter_port, std::string key, std::string value, int client_id){
    /*
    send:
        request type
        datacenter_id
        local_time
        key
        value
        number of dependencies
        dependencies
    receive:
        OK
    */
    //initailize address structure
    struct sockaddr_in datacenter_address;
    bzero(&datacenter_address, sizeof(datacenter_address));
    datacenter_address.sin_family = PF_INET;
    //inet_pton(AF_INET, datacenter_ip, &(datacenter_address.sin_addr));
    datacenter_address.sin_addr.s_addr = inet_addr(datacenter_ip);
    datacenter_address.sin_port = htons(datacenter_port);

    int client_socket;
    std::string response_message;
    const uint32_t request_type = 2;
    std::string reponse_message;
    std::map<int, std::map<std::string, version> >::iterator dependency_it;
    std::map<std::string, version>::iterator version_it;

    //create socket
    if((client_socket = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket");
        exit(0);
    }

    //connect to server
    if(connect(client_socket,(struct sockaddr*) &datacenter_address, sizeof(datacenter_address)) < 0){
        perror("Failed to connect");
        exit(0);
    }

    //send data
    send(client_socket, &server_id, sizeof(server_id), 0);
    send(client_socket, &request_type, sizeof(request_type), 0);
    send(client_socket, &local_time, sizeof(local_time), 0);
    send(client_socket, &key, sizeof(key), 0);
    send(client_socket, &value, sizeof(value), 0);
    //send dependency
    dependency_it = dependency.find(client_id);
    int map_size = dependency_it->second.size();
    send(client_socket, &map_size, sizeof(map_size), 0);
    for(version_it = dependency_it->second.begin(); version_it != dependency_it->second.end(); version_it++){
        std::string key_temp = version_it->first;
        int datacenter_id_temp = version_it->second.get_datacenterid();
        int timestamp_temp = version_it->second.get_timestamp();

        send(client_socket, &key_temp, sizeof(key_temp), 0);
        send(client_socket, &datacenter_id_temp, sizeof(datacenter_id_temp), 0);
        send(client_socket, &timestamp_temp, sizeof(timestamp_temp), 0);
    }
    close(client_socket);
    return 0;
}

int server::client_read(int new_socket, int client_id){
    std::string key;
    std::map<std::string,int>::iterator variable_it;
    std::map<int, std::map<std::string, version> >::iterator dependency_it;
    std::pair<std::map<std::string, version>::iterator, bool> version_ret;
    std::string response_message;
    recv(new_socket, &key, sizeof(key), 0);

    //check if the variable exist
    variable_it = variable_mapping.find(key);
    if(variable_it == variable_mapping.end()){
        //key does not exist
        std::cout << "client " << client_id << " reads undefined variable" << "\n";
        response_message = "key " + key + " not found";
        send(new_socket, &response_message, sizeof(response_message), 0);
        close(new_socket);
        return -1;
    }
    else{
        //key found
        //send value to client
        response_message = "key " + key + ", value is: " + variables[variable_it->second];
        std::cout << "send message: " + response_message << "\n";
        send(new_socket, &response_message, sizeof(response_message), 0);
    }

    //Update dependency
    update_dependency(client_id, key, 0);

    close(new_socket);
    return 0;
}

void server::test_delay(int datacenter_port, std::string key, std::string value, int client_id){
    struct sockaddr_in datacenter_address;
    bzero(&datacenter_address, sizeof(datacenter_address));
    datacenter_address.sin_family = PF_INET;
    datacenter_address.sin_addr.s_addr = inet_addr(server_ip.c_str());
    datacenter_address.sin_port = htons(datacenter_port);

    sleep(30);

    std::cout<< "delayed message sent" << "\n";
    
    int client_socket;
    std::string response_message;
    const uint32_t request_type = 2;
    std::string reponse_message;
    std::map<std::string, version>::iterator version_it;

    //create socket
    if((client_socket = socket(AF_INET, SOCK_STREAM , 0)) < 0){
        perror("Failed to create socket");
        exit(0);
    }

    //connect to server
    if(connect(client_socket,(struct sockaddr*) &datacenter_address, sizeof(datacenter_address)) < 0){
        perror("Failed to connect");
        exit(0);
    }

    //example1:2 example2:1
    int test_time = 2;
    //send data
    send(client_socket, &server_id, sizeof(server_id), 0);
    send(client_socket, &request_type, sizeof(request_type), 0);
    send(client_socket, &test_time, sizeof(test_time), 0);
    send(client_socket, &key, sizeof(key), 0);
    send(client_socket, &value, sizeof(value), 0);
    //send dependency
    std::map<std::string, version> test_versions;
    //example1:uncommented example2:commented
    version test_version(1, 1000);
    test_versions.insert(std::pair<std::string, version>("x", test_version));
    //
    int map_size = test_versions.size();
    send(client_socket, &map_size, sizeof(map_size), 0);
    for(version_it = test_versions.begin(); version_it != test_versions.end(); version_it++){
        std::string key_temp = version_it->first;
        int datacenter_id_temp = version_it->second.get_datacenterid();
        int timestamp_temp = version_it->second.get_timestamp();

        send(client_socket, &key_temp, sizeof(key_temp), 0);
        send(client_socket, &datacenter_id_temp, sizeof(datacenter_id_temp), 0);
        send(client_socket, &timestamp_temp, sizeof(timestamp_temp), 0);
    }
    close(client_socket);
}

int server::client_write(int new_socket, int client_id){
    std::string key;
    std::string value;
    std::map<std::string,int>::iterator variable_it;
    std::map<int, std::map<std::string, version> >::iterator dependency_it;
    std::pair<std::map<std::string, version>::iterator, bool> version_ret;
    std::string response_message;
    
    recv(new_socket, &key, sizeof(key), 0);
    recv(new_socket, &value, sizeof(value), 0);

    //check if the variable exist
    variable_it = variable_mapping.find(key);
    if(variable_it == variable_mapping.end()){
        //key does not exist
        //create new variable
        create_variables(key, value, variables, variable_mapping);
    }
    else{
        //key found
        //update variable with key
        variables[variable_it->second] = value;
    }

    //local time increase by 1
    local_time += 1;
    std::cout << "update local time to " << local_time << "\n";

    //commit
    version temp_version(local_time, server_id);
    std::pair<std::string, version> temp_pair(key, temp_version);
    commited_messages.push_back(temp_pair);

    //response write success
    response_message = "Write successfully";
    send(new_socket, &response_message, sizeof(response_message), 0);

    //send write content to other datacenter
    for(int i=0; i < DATACENTER_NUM;i++){
        if(8080 + i == server_port){
            continue;
        }
        //example1 key == "y" example2 ley == "x"
        if(key == "y" && i == 2){
            //std::thread tid(test_delay, 8082, key, value, client_id);
            //tid.detach();
            test_delay(8082, key, value, client_id);
        }
        else{
            replicated_write(server_ip.c_str(), 8080 + i, key, value, client_id);
        }
        
    }

    //Update dependency
    std::cout << "update dependency..." << "\n";
    update_dependency(client_id, key, 1);

    std::cout << "client_write completed..." << "\n";
    close(new_socket);
    return 0;
}

bool server::dependency_check(std::vector<std::pair<std::string, version> > received_dependencies){
    bool found = 0;
    if(received_dependencies.size() == 0){
        std::cout << "No dependecy, satisfied." << "\n";
        return true;
    }
    for(int i=0; i < received_dependencies.size(); i++){
        for(int j=0; j < commited_messages.size(); j++){
            if((received_dependencies[i].first == commited_messages[j].first) && (received_dependencies[i].second == commited_messages[j].second)){
                found = 1;
                break;
            }
        }
        if(found == 1){
            found = 0;
        }
        else{
            std::cout << "at least one dependency not found, unsatisfied" << "\n";
            return false;
        }
    }
    std::cout << "all dependencies found, satisfied." << "\n";
    return true;
}

int server::commit(message received_message){
    std::map<std::string,int>::iterator variable_it;
    //all the dependencies has received
    //ready to commit

    //check if the variable exist
    variable_it = variable_mapping.find(received_message.key);
    if(variable_it == variable_mapping.end()){
        //key does not exist
        //create new variable
        create_variables(received_message.key, received_message.value, variables, variable_mapping);
    }
    else{
        //key found
        //update variable with key
        variables[variable_it->second] = received_message.value;
    }

    //update local time
    if(received_message.key_version.get_timestamp() > local_time + 1){
        local_time = received_message.key_version.get_timestamp();
    }
    else{
        local_time += 1;
    }
    std::cout << "update local time to " << local_time << "\n";

    //commit
    version temp_version(received_message.key_version.get_timestamp(), received_message.key_version.get_datacenterid());
    std::pair<std::string, version> temp_pair(received_message.key, temp_version);
    commited_messages.push_back(temp_pair);
    std::cout << "commit message..." << "\n";
    std::cout << "all the commited massages: "<< "\n";

    for(int i = 0; i < commited_messages.size(); i++){
        std::cout << "<"<< commited_messages[i].first << ", ";
        std::cout << commited_messages[i].second.get_timestamp() << ", ";
        std::cout << commited_messages[i].second.get_datacenterid() << ">\n";
    }
    std::cout << "commit completed." << "\n";

    return 0;
}

int server::server_write(int new_socket, int server_id){
    /*
    receive:
        key
        value
        number of dependencies
        dependencies
    send:
        OK
    */
    std::string key;
    std::string value;
    version message_version(-1, server_id);
    int dependency_num;
    std::vector<std::pair<std::string, version> > received_dependencies;
    std::vector<message>::iterator it;
    std::string response_message;
    int temp_timestamp;
    
    //receiving messages from servers and put them into message data structure
    recv(new_socket, &temp_timestamp, sizeof(temp_timestamp), 0);
    message_version.write_timestamp(temp_timestamp);
    recv(new_socket, &key, sizeof(key), 0);
    recv(new_socket, &value, sizeof(value), 0);
    recv(new_socket, &dependency_num, sizeof(dependency_num), 0);
    for(int i = 0; i < dependency_num; i++){
        std::string key_temp;
        version key_version_temp(-1, -1);
        int temp_datacenter_id;

        recv(new_socket, &key_temp, sizeof(key_temp), 0);
        recv(new_socket, &temp_datacenter_id, sizeof(temp_datacenter_id), 0);
        key_version_temp.write_datacenterid(temp_datacenter_id);
        recv(new_socket, &temp_timestamp, sizeof(temp_timestamp), 0);
        key_version_temp.write_timestamp(temp_timestamp);
        
        received_dependencies.push_back(std::pair<std::string, version>(key_temp, key_version_temp));
    }
    message received_message(key, value, message_version, received_dependencies);

    for(int i = 0; i < received_message.dependencies.size(); i++){
        std::cout << "dependency: <"<< received_message.dependencies[i].first << ", ";
        std::cout << received_message.dependencies[i].second.get_timestamp() << ", ";
        std::cout << received_message.dependencies[i].second.get_datacenterid() << ">\n";
    }

    //dependency check
    //if satisfied -> commit
    //else -> pending list
    if(dependency_check(received_message.dependencies)){
        //all the dependencies has received
        //ready to commit
        commit(received_message);
        //reissue pending_list
        for(it = pending_messages.begin(); it != pending_messages.end(); it++){
            if(dependency_check(it->dependencies)){
                commit(*it);
                it = pending_messages.erase(it);
                if(it == pending_messages.end()){
                    break;
                }
                else{
                    it --;
                }
            }
        }
    }
    else{
        std::cout << "delay message..." << "\n";
        pending_messages.push_back(received_message);
        std::sort(pending_messages.begin(),pending_messages.end());

    }
    close(new_socket);
    return 0;
}

int server::receive_connection(int new_socket, int client_id, int request_type){
     //create dependency for client if not exist
     if(client_id < 1000){
        std::pair<std::map<int, std::map<std::string, version> >::iterator, bool> ret;
        std::map<std::string, version> temp;
        ret = dependency.insert(std::pair<int, std::map<std::string, version> >(client_id, temp));
        if(ret.second == false){
            std::cout << "client " << client_id << " dependency exist" << "\n";
        }
        else{
            std::cout << "client " << client_id << " dependency created" << "\n";
        }
     }
    //0: client read, 1: client write, 2: server write
    switch(request_type){
        case 0:
            std::cout << "client " << client_id << " read..." << "\n";
            client_read(new_socket, client_id);
            break;
        case 1:
            std::cout << "client " << client_id << " write..." << "\n";
            client_write(new_socket, client_id);
            break;
        case 2:
            std::cout << "server " << client_id << " replicated write..." << "\n";
            server_write(new_socket, client_id);
            break;
        default:
            perror("Wrong request number");
            break;
    }
    return 0;
}

int server::listen_client(){
    int server_socket, new_socket;
    int client_sockets[30];
    struct sockaddr_in server_address, client_address;
    fd_set read_fds;
    int max_sd;
    int activity;
    int client_addrlen;
    
    //create socket
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Failed to create socket");
        exit(0);
    }

    //initailize address structure
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = PF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    //bind socket
    if(bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        perror("Failed to bind");
        exit(0);
    }

    //listen socket
    if(listen(server_socket, 5) < 0){
        perror("Failed to listen");
        exit(0);
    }

    //wait for incoming connections
    client_addrlen = sizeof(client_address);
    puts("Waiting for connections ...");

    //accept connections
    while(true){
        if((new_socket = accept(server_socket,(struct sockaddr*) &client_address, (socklen_t *) &client_addrlen)) < 0){
            perror("Failed to accept");
            exit(0);
        }
        printf("New connection , socket fd is %d , ip is : %s , port : %d \n" , new_socket , inet_ntoa(client_address.sin_addr) , ntohs(client_address.sin_port));

        int request_type;
        int client_id;

        recv(new_socket, &client_id, sizeof(client_id), 0);
        recv(new_socket, &request_type, sizeof(request_type), 0);

        //use multi-thread to handle requests
        //std::thread handle_thread(receive_connection, new_socket, client_id, request_type);
        //handle_thread.detach();
        receive_connection(new_socket, client_id, request_type);
    }
}