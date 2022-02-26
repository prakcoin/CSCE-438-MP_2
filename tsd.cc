#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using namespace std;

typedef struct ClientStruct{
  string username;
  vector<ClientStruct> followers;
  vector<ClientStruct> following;
  ServerReaderWriter<Message, Message>* stream = 0;
} ClientStruct;

vector<ClientStruct> client_db;

class SNSServiceImpl final : public SNSService::Service {
  public:
  string serveraddr = "";
  
  private:
  int user_in_db(string username){ //returns user index in the database
    for (int i = 0; i < client_db.size(); i++){
      if (client_db[i].username == username){
        return i;
      }
    }
    return -1;
  }
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    int index = user_in_db(request->username());
    ClientStruct cli = client_db[index];
    for (int i = 0; i < client_db.size(); i++){
      reply->add_all_users(client_db[i].username);
    }
    for (int i = 0; i < cli.followers.size(); i++){
      reply->add_following_users(cli.followers[i].username);
    }
    reply->add_following_users(request->username() + " (self)");//adding youself to the list since that's required
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    string username = request->username();
    string otheruser = request->arguments(0);
    
    int otheruserindex = user_in_db(otheruser);
    int userindex = user_in_db(username);
    if (otheruserindex < 0){ //not in the database
      reply->set_msg("Follow failed. Invalid user.");
    } else {
      ClientStruct* othercli = &client_db[otheruserindex];
      ClientStruct* client = &client_db[userindex];
      
      for (int i = 0; i < client_db[userindex].following.size(); i++){
        if (othercli->username == client_db[userindex].following[i].username){//already following them
          reply->set_msg("Follow failed. Already following.");
          return Status::OK;
        }
      }
      client->following.push_back(*othercli);//add eachother to lists
      othercli->followers.push_back(*client);
      reply->set_msg("Followed user.");
    }
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    string username = request->username();
    string otheruser = request->arguments(0);
    
    int otheruserindex = user_in_db(otheruser);
    int userindex = user_in_db(username);
    if (otheruserindex < 0){
      reply->set_msg("UnFollow failed. Invalid user.");
    } else {
      int indexinothersfollowers = 0;
      for (int i = 0; i < client_db[otheruserindex].followers.size(); i++){
        if (client_db[otheruserindex].followers[i].username == username){ //remove the user from the unfollowed users following by tracking where they are in their followers list
          indexinothersfollowers = i;
          break;
        }
      }
      for (int i = 0; i < client_db[userindex].following.size(); i++){
        if (otheruser == client_db[userindex].following[i].username){ //erase eachother from followers and following
          client_db[userindex].following.erase(client_db[userindex].following.begin() + i);
          client_db[otheruserindex].followers.erase(client_db[otheruserindex].followers.begin() + indexinothersfollowers);
          reply->set_msg("UnFollowed user.");
          return Status::OK;
        }
      }
      reply->set_msg("UnFollow failed. User not found.");
    }
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    ClientStruct cli;
    string username = request->username();
    int index = user_in_db(username);
    if (index == -1){
      cli.username = username;
      client_db.push_back(cli);
      cout << "Username available! Creating user '" << username << "' and logging in..." << endl;
      reply->set_msg("Username available! Creating user and logging in...");
    } else { //EXISTS
      cout << "Username taken." << endl; 
      reply->set_msg("Username taken.");
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    Message m;
    int index = 0;
    string username = "";
    string filename = "";
    bool init = true;
    while (stream->Read(&m)){
      username = m.username();
      index = user_in_db(username);
      google::protobuf::Timestamp timestamp = m.timestamp();
      string timestr = google::protobuf::util::TimeUtil::ToString(timestamp);
      string fullmsg = m.username() + "@" + timestr + ":" + m.msg();
      if (client_db[index].stream == 0){ //set the stream in order for messages to display 
        client_db[index].stream = stream;
      }
      if (m.msg() == "Initial Login"){ //prompts sending the last 20 messages upon initial login
        if (init){
          string line = "";
          ifstream read_file(username + "_following.txt");
          int count = 0;
          Message sent_msg;
          while (getline(read_file, line) && count < 20){//reading the users following stream
            sent_msg.set_msg(line);
            stream->Write(sent_msg);
            count++;
          }
        }
      }
      for (int i = 0; i < client_db[index].followers.size(); i++){ //creating file to track the users followings messages 
        int otherindex = user_in_db(client_db[index].followers[i].username);
        string otherusername = client_db[otherindex].username;
        string otherfilename = otherusername + "_following.txt";
        if (client_db[otherindex].stream != 0){
          client_db[otherindex].stream->Write(m);
        }
        ofstream other_file(otherfilename, ofstream::out | ofstream::in | ofstream::app);
        other_file << fullmsg;
      }
    }
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  string server_addr = "localhost:" + port_no;
  SNSServiceImpl service;
  service.serveraddr = server_addr;
  
  ServerBuilder builder;
  builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_addr << std::endl;
  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  RunServer(port);
  return 0;
}
