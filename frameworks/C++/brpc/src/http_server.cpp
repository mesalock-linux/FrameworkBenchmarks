// Copyright (c) 2014 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A server to receive HttpRequest and send back HttpResponse.

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include <brpc/errno.pb.h>
#include <brpc/builtin/common.h>
#include <json2pb/pb_to_json.h>
#include "http.pb.h"

DEFINE_int32(port, 8080, "TCP Port of this server");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");

DEFINE_string(certificate, "cert.pem", "Certificate file path to enable SSL");
DEFINE_string(private_key, "key.pem", "Private key file path to enable SSL");
DEFINE_string(ciphers, "", "Cipher suite used for SSL connections");

namespace example {

// Service with static path.
class PlaintextServiceImpl : public PlaintextService {
public:
    PlaintextServiceImpl() {};
    virtual ~PlaintextServiceImpl() {};
    void plaintext(google::protobuf::RpcController* cntl_base,
              const HttpRequest*,
              HttpResponse*,
              google::protobuf::Closure* done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);
        
        brpc::Controller* cntl =
            static_cast<brpc::Controller*>(cntl_base);
        // Fill response.
        char buf[256];
        time_t now = time(0);
        brpc::Time2GMT(now, buf, sizeof(buf));
        cntl->http_response().SetHeader("Date", buf);
        cntl->http_response().SetHeader("Server", "brpc");
        cntl->http_response().set_content_type("text/plain");
        cntl->response_attachment().append("Hello, World!");
    }
};

// Service with static path.
class JsonServiceImpl : public JsonService {
public:
    JsonServiceImpl() {};
    virtual ~JsonServiceImpl() {};
    void json(google::protobuf::RpcController* cntl_base,
              const HttpRequest*,
              HttpResponse*,
              google::protobuf::Closure* done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);
        
        brpc::Controller* cntl =
            static_cast<brpc::Controller*>(cntl_base);
        // Fill response.
        Message msg;
        msg.set_message("Hello, World!");

        char buf[256];
        time_t now = time(0);
        brpc::Time2GMT(now, buf, sizeof(buf));
        cntl->http_response().SetHeader("Date", buf);
        cntl->http_response().SetHeader("Server", "brpc");
        cntl->http_response().set_content_type("application/json");

        butil::IOBufAsZeroCopyOutputStream wrapper(&cntl->response_attachment());
        std::string err;
        json2pb::Pb2JsonOptions opt;
        if (!json2pb::ProtoMessageToJson(msg, &wrapper, opt, &err)) {
            cntl->SetFailed(brpc::ERESPONSE, "Failed to parse content, %s", err.c_str());
        }
    }
};

}  // namespace example

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    // Generally you only need one Server.
    brpc::Server server;

    example::PlaintextServiceImpl plaintext_svc;
    example::JsonServiceImpl json_svc;

    // Add services into server. Notice the second parameter, because the
    // service is put on stack, we don't want server to delete it, otherwise
    // use brpc::SERVER_OWNS_SERVICE.
    if (server.AddService(&plaintext_svc,
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add plaintext_svc";
        return -1;
    }
    if (server.AddService(&json_svc,
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add json_svc";
        return -1;
    }
    
    // Start the server.
    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    //options.mutable_ssl_options()->default_cert.certificate = FLAGS_certificate;
    //options.mutable_ssl_options()->default_cert.private_key = FLAGS_private_key;
    //options.mutable_ssl_options()->ciphers = FLAGS_ciphers;
    if (server.Start(FLAGS_port, &options) != 0) {
        LOG(ERROR) << "Fail to start HttpServer";
        return -1;
    }

    // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
    server.RunUntilAskedToQuit();
    return 0;
}
