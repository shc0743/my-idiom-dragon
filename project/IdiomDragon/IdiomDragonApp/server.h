#pragma once
#include <drogon/drogon.h>
#include <drogon/WebSocketController.h>
#include <Windows.h>
#include <string>
#include <vector>


#define CORSadd(req, resp) {\
	resp->addHeader("access-control-allow-origin", req->getHeader("origin"));\
	resp->addHeader("access-control-allow-methods", "GET,HEAD,POST,PUT,PATCH,DELETE,OPTIONS");\
	resp->addHeader("access-control-allow-headers", req->getHeader("access-control-request-headers"));\
	resp->addHeader("access-control-allow-credentials", "true");\
	resp->addHeader("access-control-max-age", "300");\
}

constexpr auto SessCookieName = "IDIOMDRAGONAPP_SESSION";




namespace server {
	using namespace drogon;
	using namespace std;

	struct MyUserData
	{
		string name;
		string pswd;
	};

	class ServiceSession
	{
	public:
		ServiceSession() = default;
		~ServiceSession() = default;
	public:
		string host;
		vector<string> members;

		long long state;

	};

	class AuthFilter :public drogon::HttpFilter<AuthFilter>
	{
	public:
		virtual void doFilter(const HttpRequestPtr& req,
			FilterCallback&& fcb,
			FilterChainCallback&& fccb) override;
	};

	class MainServer : public drogon::HttpController<MainServer>
	{
	public:
		METHOD_LIST_BEGIN
			ADD_METHOD_TO(server::MainServer::auth, "/api/auth", Post, Options, "server::AuthFilter");
			ADD_METHOD_TO(server::MainServer::login, "/api/auth/login", Post, Options);
			ADD_METHOD_TO(server::MainServer::logout, "/api/auth/logout", Post, Options, "server::AuthFilter");
			ADD_METHOD_TO(server::MainServer::isSessionExpired, "/api/auth/isExpired", Get, Post, Options);

			ADD_METHOD_TO(server::MainServer::meinfo, "/api/me", Get, "server::AuthFilter");

		METHOD_LIST_END



		void auth(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void login(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void logout(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void isSessionExpired(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;

		void meinfo(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;

	public:


		MainServer() {

		}

		static const bool isAutoCreation = false;
	};

	class WebSocketService : public drogon::WebSocketController<WebSocketService>
	{
	  public:
		void handleNewMessage(const WebSocketConnectionPtr&,
							  std::string &&,
							  const WebSocketMessageType &) override;
		void handleNewConnection(const HttpRequestPtr &,
								 const WebSocketConnectionPtr&) override;
		void handleConnectionClosed(const WebSocketConnectionPtr&) override;
		WS_PATH_LIST_BEGIN
		//list path definitions here;
			WS_PATH_ADD("/api/v4.4/web", "server::AuthFilter");
		WS_PATH_LIST_END
	};


}



