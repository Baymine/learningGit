#ifndef TINYRPC_COMM_START_H
#define TINYRPC_COMM_START_H

#include <google/protobuf/service.h>
#include <memory>

namespace tinyrpc{
    #define REGISTER_HTTP_SERVLET(path, servlet) \
    do{\
        if(!tinyrpc::GetServer()->registerHttpServlet(path, std::make_shared<servlet>())){ \
            printf("Start TinyRPC server error, because register http servelt error, please look up rpc log get more details!\n"); \
            tinyrpc::Exit(0);
        }
    }
}


#endif