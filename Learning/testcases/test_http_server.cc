#include <google/protobuf/service.h>
#include <atomic>
#include <future>
#include "test_tinypb_server.pb.h"
#include "../tinyrpc/comm/start.h"

const char* html = "<html><body><h1>Welcome to TinyRPC, just enjoy it!</h1><p>%s</p></body></html>";

tinyrpc::IPAddress:: ptr addr = std::make_shared<tinyrpc::IPAddress>("127.0.0.1", 20000);

class BlockCallHttpServlet : public tinyrpc::HttpServlet{

};

class BlockCallHttpServlet : public tinyrpc::HttpServlet{
    
};

class BlockCallHttpServlet : public tinyrpc::HttpServlet{
    
};

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Start TinyRPC server error, input argc is not 2!");
        printf("Start TinyRPC server like this: \n");
        printf("./server a.xml\n");
        return 0;
    }

    tinyrpc::InitConfig(argv[1]);
}