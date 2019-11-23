#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include "version.hpp"
#include "message.hpp"

#define DATACENTER_NUM 3

class server{
    public:
        server();
        server(int datacenter_id, std::string server_ip, int port);
        int listen_client();
        void test_delay(int datacenter_port, std::string key, std::string value, int client_id);

    private:
        int server_id;
        int local_time;
        std::string server_ip;
        int server_port;
        std::map<int, std::map<std::string, version> > dependency;
        std::map<std::string, int> variable_mapping;
        std::vector<std::string> variables;
        std::vector<std::pair<std::string, version> > commited_messages;
        std::vector<message> pending_messages;

        //Create a variable when write to a non-exist key.
        int create_variables(std::string key, std::string value, std::vector<std::string> &vars, std::map<std::string, int> &var_map);

        //Update dependency for client_read and client_write
        int update_dependency(int client_id, std::string key, int operation);

        int replicated_write(const char * datacenter_ip, int datacenter_port, std::string key, std::string value, int client_id);

        int client_read(int new_socket, int client_id);

        int client_write(int new_socket, int client_id);

        bool dependency_check(std::vector<std::pair<std::string, version> > received_dependencies);

        int commit(message received_message);

        int server_write(int new_socket, int server_id);

        int receive_connection(int new_socket, int client_id, int request_type);
};
#endif