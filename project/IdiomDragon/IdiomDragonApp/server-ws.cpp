#include "server.h"
#include <string>
#include "../../resource/tool.h"
#include <openssl/evp.h>
using namespace server;
using namespace drogon;

using namespace std;
extern vector<MyUserData> userdata;
extern unordered_map<string, string> sess2UserName;


unordered_map<string, ServiceSession> sesId2details;
unordered_map<string, string> userName2sesId;


void WebSocketService::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,std::string &&message, const WebSocketMessageType & wstype)
{
    //write your application logic here
    try {
        Json::Value json;
        Json::CharReaderBuilder rbuilder;
        Json::CharReader* reader = rbuilder.newCharReader();
        string err;
        if (!reader->parse(message.c_str(), message.c_str() + message.length(), &json, &err)) {
            throw err;
        }

        string type = json["type"].asString();

        if (type == "echo") {
			Json::Value val = json["data"];
			Json::FastWriter fastWriter;
            wsConnPtr->send(fastWriter.write(val));
            return;
        }

    }
    catch (std::string errText) {
        Json::Value json;
        json["success"] = false;
        json["type"] = "error";
        json["error"] = errText;
		Json::FastWriter fastWriter;
        wsConnPtr->send(fastWriter.write(json));
    }
    catch (...) {
        // 无法处理
    }
}
void WebSocketService::handleNewConnection(const HttpRequestPtr &req,const WebSocketConnectionPtr &wsConnPtr)
{
    //write your application logic here
}
void WebSocketService::handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr)
{
    //write your application logic here
}



