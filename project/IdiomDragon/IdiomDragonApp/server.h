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
constexpr auto CreditCookieName = "IDIOMDRAGONAPP_CREDITS";




namespace server {
	using namespace drogon;
	using namespace std;

	struct MyUserData
	{
		string name;
		string pswd;
	};

	constexpr size_t MEMBER_CAN_SKIP_TIME = 5;

	//class ServiceSessionMemberData {
	//public:
	//
	//};
	class IdiomAppealPhraseAndUser
	{
	public:
		IdiomAppealPhraseAndUser() :isValid(false) {

		}
		bool isValid;
		string user, phrase;

	};

	class ServiceSession
	{
	public:
		ServiceSession() : state(0), current_dragon_user_index(-1), l_aiMode(false) {};
		~ServiceSession() = default;
	public:
		string host;
		//vector<ServiceSessionMember> members;
		unordered_map<string, vector<WebSocketConnectionPtr>> members;
		unordered_map<string, size_t> membersSkipChance;
		vector<string> membersOrder;
		unordered_set<string> losers;
		IdiomAppealPhraseAndUser l_appealingPhrase;

		long long state;
		deque<string> phrases;
		long long current_dragon_user_index;

		bool l_aiMode;
		string winnerStr;

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
			ADD_METHOD_TO(server::MainServer::auth, "/api/auth", Post, Options);
			ADD_METHOD_TO(server::MainServer::login, "/api/auth/login", Post, Options);
			ADD_METHOD_TO(server::MainServer::reg, "/api/auth/register", Post, Options);
			ADD_METHOD_TO(server::MainServer::logout, "/api/auth/logout", Post, Options, "server::AuthFilter");
			ADD_METHOD_TO(server::MainServer::editPassword, "/api/auth/type/password/edit", Post, Options, "server::AuthFilter");
			ADD_METHOD_TO(server::MainServer::isSessionExpired, "/api/auth/isExpired", Get, Post, Options);

			ADD_METHOD_TO(server::MainServer::meinfo, "/api/me", Get, "server::AuthFilter");
			ADD_METHOD_TO(server::MainServer::accountpunishinfo, "/api/account-punish/query", Get, "server::AuthFilter");

			ADD_METHOD_TO(server::MainServer::test20240210_getpy, "/api/test/20240210/pinyin?char={}", Get, "server::AuthFilter");

		METHOD_LIST_END



		void auth(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void login(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void reg(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void logout(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void editPassword(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;
		void isSessionExpired(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;

		void meinfo(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;

		void accountpunishinfo(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const;

		void test20240210_getpy(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, std::string&& cch) const;

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



