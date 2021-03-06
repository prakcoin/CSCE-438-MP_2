#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"
#include "sns.grpc.pb.h"

using namespace std;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using csce438::SNSService;
using csce438::Message;
using csce438::Request;
using csce438::Reply;

/*
export MY_INSTALL_DIR=$HOME/.grpc
export PATH="$MY_INSTALL_DIR/bin:$PATH"
export PKG_CONFIG_PATH=$MY_INSTALL_DIR/lib/pkgconfig/
export PKG_CONFIG_PATH=$MY_INSTALL_DIR/lib64/pkgconfig:$PKG_CONFIG_PATH
*/

class Client : public IClient
{
    private:
        std::string hostname;
        std::string username;
        std::string port;

        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
        string getUsername(){return username;}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
    string login = hostname + ":" + port;
    if (login == ":"){
        return -1;
    }
    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(login, grpc::InsecureChannelCredentials())));
    
    Request req;
    Reply reply;
    ClientContext context;
    req.set_username(username);
    
    Status status = stub_->Login(&context, req, &reply);
    
    IReply ire;
    ire.grpc_status = status;
    
    if (reply.msg() == "Username taken."){
        return -1;
    }
    
    return 1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------

    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------

	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    //
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //
    //      return ire;
    //
    // IMPORTANT:
    // For the command "LIST", you should set both "all_users" and
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------

    
    IReply ire;
    string command = "";
    string otheruser = "";
    
    ClientContext context;
    Request request;
    Reply reply;
    int index = 0;
    if ((index = input.find_first_of(" ")) != string::npos){ // handle follow and unfollow
        command = input.substr(0, index);
        otheruser = input.substr(index + 1, input.length() - index);
        if (command == "FOLLOW"){
            request.set_username(username);
            request.add_arguments(otheruser);
            Status status = stub_->Follow(&context, request, &reply);
            ire.grpc_status = status;
            if (reply.msg() == "Follow failed. Invalid user.") {
                ire.comm_status = FAILURE_INVALID_USERNAME;
            } else if (reply.msg() == "Follow failed. Already following.") {
                ire.comm_status = FAILURE_ALREADY_EXISTS;
            } else if (reply.msg() == "Followed user.") {
                ire.comm_status = SUCCESS;
            } else {
                ire.comm_status = FAILURE_UNKNOWN;
            }
        } else if (command == "UNFOLLOW"){
            request.set_username(username);
            request.add_arguments(otheruser);
            Status status = stub_->UnFollow(&context, request, &reply);
            ire.grpc_status = status;
            if (reply.msg() == "UnFollow failed. Invalid user." || reply.msg() == "UnFollow Failed. User not found.") {
                ire.comm_status = FAILURE_INVALID_USERNAME;
            } else if (reply.msg() == "UnFollowed user.") {
                ire.comm_status = SUCCESS;
            } else {
                ire.comm_status = FAILURE_UNKNOWN;
            }
        }
    } else { //handle list and timeline
        command = input.substr(0, index);
        if (command == "LIST"){
            request.set_username(username);
            Status status = stub_->List(&context, request, &reply);
            ire.grpc_status = status;
            if (status.ok()){
                ire.comm_status = SUCCESS;
                for (string user : reply.all_users()){
                    ire.all_users.push_back(user);
                }
                for (string user : reply.following_users()){
                    ire.following_users.push_back(user);
                }
            } else {
                ire.comm_status = FAILURE_UNKNOWN;
            }
        } else if (command == "TIMELINE"){
            ire.comm_status = SUCCESS;
        }
    }
    return ire;
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
	ClientContext context;
	shared_ptr<ClientReaderWriter<Message, Message>> stream(stub_->Timeline(&context));
	string username = getUsername();
	
	thread writethread([username, stream](){
	    string sendmsg = "";
	    string init = "Initial Login";
	    Message m;
        m.set_username(username);
        m.set_msg(init);
        google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
        timestamp->set_seconds(time(NULL));
        timestamp->set_nanos(0);
        m.set_allocated_timestamp(timestamp);
	    stream->Write(m);
	    while(true){
	        sendmsg = getPostMessage();
	        Message m;
            m.set_username(username);
            m.set_msg(sendmsg);
            google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
            timestamp->set_seconds(time(NULL));
            timestamp->set_nanos(0);
            m.set_allocated_timestamp(timestamp);
	        stream->Write(m);
	    }
	    stream->WritesDone();
	});
	
	
	thread readthread([username, stream](){
	    Message m;
	    while(stream->Read(&m)){
	        google::protobuf::Timestamp timestamp = m.timestamp();
	        time_t timeinsec = timestamp.seconds();
	        displayPostMessage(m.username(), m.msg(), timeinsec);
	    }
	});
	
	if (writethread.joinable()){
	    writethread.join();
	}
	if (readthread.joinable()){
	    readthread.join();    
	}
	
}
