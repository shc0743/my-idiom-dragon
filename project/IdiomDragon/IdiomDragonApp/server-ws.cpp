#include "server.h"
#include <string>
#include "../../resource/tool.h"
#include <openssl/evp.h>
using namespace server;
using namespace drogon;

using namespace std;
extern vector<MyUserData> userdata;
extern unordered_map<string, string> loginSess2UserName;
extern unordered_set<string> idioms_database;


unordered_map<string, shared_ptr<ServiceSession>> sesId2details;
unordered_map<string, string> userName2sesId;

#define ccs8(x) (ConvertUTF16ToUTF8(L ## x))




static ULONGLONG GenerateMySessId() {
	// 静态局部变量：随机数生成器和分布  
	static std::mt19937 generator((UINT)std::chrono::system_clock::now().time_since_epoch().count());
	static std::uniform_int_distribution<int> distribution(10000000, 99999999);

	// 生成随机数  
	ULONGLONG randomNumber = distribution(generator);
	randomNumber += 200000000; // 以“2”开头
	return randomNumber;
}



static void wsProcessMessage(const WebSocketConnectionPtr& wsConnPtr, std::string message, const WebSocketMessageType& wstype);
void WebSocketService::handleNewMessage(const WebSocketConnectionPtr& wsConnPtr, std::string&& message, const WebSocketMessageType& wstype)
{
	//write your application logic here
	if (wstype == WebSocketMessageType::Text)
		wsProcessMessage(wsConnPtr, message, wstype);

}
void WebSocketService::handleNewConnection(const HttpRequestPtr& req, const WebSocketConnectionPtr& wsConnPtr)
{
	//write your application logic here

	// 存储session token
	std::string sess = req->getCookie(SessCookieName);
	shared_ptr<string> psess = make_shared<string>(sess);
	wsConnPtr->setContext(psess);
	try {
		string uname = loginSess2UserName.at(sess);
		string session_id = userName2sesId.at(uname);
		shared_ptr<ServiceSession> ss = sesId2details.at(session_id);
		auto& connections = ss->members.at(uname);
		connections.push_back(wsConnPtr);
	}
	catch (std::exception) {}

	LOG_DEBUG << "[ws] connected " << req->peerAddr().toIpPort() << ", session token=" << sess;
}
void WebSocketService::handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr)
{
	//write your application logic here
	string sess = wsConnPtr->getContextRef<string>();
	try {
		string uname = loginSess2UserName.at(sess);
		string session_id = userName2sesId.at(uname);
		shared_ptr<ServiceSession> ss = sesId2details.at(session_id);
		auto& vec = ss->members.at(uname);
		vec.erase(std::remove(vec.begin(), vec.end(), wsConnPtr), vec.end());
	}
	catch (std::exception) {}

	LOG_DEBUG << "[ws] disconnected, session token= " << sess;
}



static void WsSendUserState(const WebSocketConnectionPtr& wsConnPtr, const string& user) {
	Json::Value val;
	val["type"] = "receive-user-state";
	if (userName2sesId.contains(user)) {
		val["status"] = "running";
		val["sid"] = userName2sesId.at(user);
	}
	else {
		val["status"] = "unassigned";
	}
	Json::FastWriter fastWriter;
	wsConnPtr->send(fastWriter.write(val));
}
static void WsSendSessState(const WebSocketConnectionPtr& wsConnPtr, const string& ses) {
	Json::Value val;
	val["type"] = "receive-session-state";
	try {
		shared_ptr<ServiceSession> ss = sesId2details.at(ses);

		val["state"] = ss->state;
		val["host"] = ss->host;
		Json::Value members(Json::arrayValue);
		for (const auto& i : ss->members) {
			members.append(i.first);
		}
		val["members"] = members;
	}
	catch (std::exception exc) {
		val["success"] = false;
		val["reason"] = exc.what();
	}
	Json::FastWriter fastWriter;
	wsConnPtr->send(fastWriter.write(val));
}


static void UpdateMembersOrderForSess(shared_ptr<ServiceSession> ss) {
	ss->membersOrder.clear();
	for (auto& i : ss->members) {
		if (!ss->losers.contains(i.first)) ss->membersOrder.push_back(i.first);
	}
}


static void UpdateInfluencedUserWebUI(string session_id) {
	try {
		shared_ptr<ServiceSession> ss = sesId2details.at(session_id);
		for (auto& i : ss->members) {
			for (auto& j : i.second) {
				WsSendUserState(j, i.first);
				WsSendSessState(j, session_id);

			}
		}
	}
	catch (...) {}
}


static void wsReplyDragon(string ses, string targetUser = "") {
	try {
		shared_ptr<ServiceSession> ss = sesId2details.at(ses);
		Json::Value val;
		val["type"] = "dragon-event";
		val["state"] = ss->state;
		val["host"] = ss->host;
		val["round"] = ss->phrases.size();
		val["dragon_phrase"] = ss->phrases.empty() ? 
			Json::Value(Json::nullValue) : ss->phrases[0];
		try {
			val["dragon_user"] = ss->membersOrder.at(ss->current_dragon_user_index);
		}
		catch (std::out_of_range) {
			val["dragon_user"] = Json::Value(Json::nullValue);
		}
		for (auto& i : ss->members) {
			if (!targetUser.empty() && i.first != targetUser) continue;
			try {
				val["skipCount"] = ss->membersSkipChance.at(i.first);
			}
			catch (std::out_of_range) {
				val["skipCount"] = 0;
			}
			for (auto& j : i.second) {
				Json::FastWriter fastWriter;
				j->send(fastWriter.write(val));
			}
		}
	}
	catch (...) {}
}


static void wsProcessMessage(const WebSocketConnectionPtr& wsConnPtr, std::string message, const WebSocketMessageType& wstype) {
	if (message[0] != '{') {
		if (message == "Here's some text that the server is urgently awaiting!") {
			wsConnPtr->send("Thank you! I need them! By the way, do you know Genshin Impact? "
				"It is a popular video game which is developed by miHoYo. It's really "
				"interesting and fun to play! If you want to play it, just go to "
				"https://genshin.hoyoverse.com/en/ to download it! Have a good day!");
			return;
		}
	}
	try {
		Json::Value json;
		Json::CharReaderBuilder rbuilder;
		Json::CharReader* reader = rbuilder.newCharReader();
		string err;
		if (!reader->parse(message.c_str(), message.c_str() + message.length(), &json, &err)) {
			throw err;
		}

		string type = json["type"].asString();
		string sess_tok = wsConnPtr->getContextRef<string>();
		string user = loginSess2UserName.at(sess_tok);

		if (type == "echo") {
			Json::Value val = json["data"];
			Json::FastWriter fastWriter;
			wsConnPtr->send(fastWriter.write(val));
			return;
		}

		// 下面是需要session的功能
		if (sess_tok.empty()) throw "no session found";

		if (type == "getUserState") {
			WsSendUserState(wsConnPtr, user);
			return;
		}

		if (type == "create-or-join-session") {
			Json::Value val;
			val["type"] = "receive-initsession";
			string action = json["action"].asString();
			if (userName2sesId.contains(user)) {
				val["success"] = false; val["reason"] = ccs8("用户已处于另一对局中。");
			}
			else if (action == "create") do {
				string session_id = to_string(GenerateMySessId());
				if (sesId2details.contains(session_id)) {
					val["success"] = false; val["reason"] = ccs8("生成的对局ID出现重复。"); break;
				}
				shared_ptr<ServiceSession> ss = make_shared<ServiceSession>();
				ss->host = user;
				vector<WebSocketConnectionPtr> conn;
				conn.push_back(wsConnPtr);
				ss->members.insert(make_pair(user, conn));
				ss->membersSkipChance.insert(make_pair(user, MEMBER_CAN_SKIP_TIME));
				ss->state = 1;
				sesId2details.insert(make_pair(session_id, ss));
				userName2sesId.insert(make_pair(user, session_id));
				val["success"] = true;
				val["sid"] = session_id;
			} while (0);
			else if (action == "join") do {
				const auto& target = json["target"];
				if (!target.isString()) {
					val["success"] = false; val["reason"] = ccs8("无效的对局ID类型。"); break;
				}
				string session_id = target.asString();
				if (!sesId2details.contains(session_id)) {
					val["success"] = false; val["reason"] = ccs8("找不到指定的对局。"); break;
				}
				shared_ptr<ServiceSession> ss = sesId2details.at(session_id);
				if (ss->state != 1) {
					val["success"] = false; val["reason"] = ccs8("指定的对局已开始，无法加入。"); break;
				}
				vector<WebSocketConnectionPtr> conn;
				conn.push_back(wsConnPtr);
				ss->members.insert(make_pair(user, conn));
				ss->membersSkipChance.insert(make_pair(user, MEMBER_CAN_SKIP_TIME));
				val["success"] = true;
				val["sid"] = session_id;
				userName2sesId.insert(make_pair(user, session_id));
				UpdateInfluencedUserWebUI(session_id);
			} while (0);
			else {
				val["success"] = false; val["reason"] = ccs8("无效的action参数值");
			}
			Json::FastWriter fastWriter;
			wsConnPtr->send(fastWriter.write(val));
			return;
		}

		if (type == "query-session-state") {
			WsSendSessState(wsConnPtr, userName2sesId.at(user));
			return;
		}

		if (type == "leave-session") {
			Json::Value val;
			string sesId = userName2sesId.at(user);

			shared_ptr<ServiceSession> ss = sesId2details.at(sesId);

			if (ss->host == user) {
				for (const auto& i : ss->members) {
					auto& un = i.first;
					userName2sesId.erase(un);
					for (auto& j : i.second)
						WsSendUserState(j, un);
				}
				ss->members.clear();
				sesId2details.erase(sesId);
			}
			else {
				ss->members.erase(user);
				userName2sesId.erase(user);
				UpdateInfluencedUserWebUI(sesId);
				UpdateMembersOrderForSess(ss);
			}

			val["type"] = "refresh-page";
			Json::FastWriter fastWriter;
			wsConnPtr->send(fastWriter.write(val));
			return;
		}

		if (type == "kick-user-in-session") {
			Json::Value val;
			val["type"] = "error-ui";
			val["modal"] = true;
			try {
				string action = json["action"].asString();
				string tuser = json["user"].asString();
				string session_id = json["sid"].asString();
				if (!sesId2details.contains(session_id)) {
					throw std::exception(ccs8("找不到指定的对局。").c_str());
				}
				shared_ptr<ServiceSession> ss = sesId2details.at(session_id);
				if (ss->host != user) {
					throw std::exception(ccs8("你没有访问此对象的权限。").c_str());
				}

				try {
					auto& vec = ss->members.at(tuser);
					Json::Value v2;
					v2["type"] = "error-ui";
					v2["modal"] = true;
					v2["refresh"] = true;
					v2["error"] = ccs8("您被请出对局");
					for (auto& i : vec) {
						Json::FastWriter fastWriter;
						i->send(fastWriter.write(v2));
					}
				}
				catch (std::out_of_range) {
					throw std::exception(ccs8("指定的用户不在对局中。").c_str());
				}
				catch (std::exception exc) {
					throw std::exception((ccs8("请离用户时发生C++异常: ") + exc.what()).c_str());
				}
				ss->members.erase(tuser);
				userName2sesId.erase(tuser);

				UpdateInfluencedUserWebUI(session_id);

			}
			catch (std::exception exc) {
				val["error"] = exc.what();
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
			return;
		}

		if (type == "start-game") {
			Json::Value val;
			val["type"] = "error-ui";
			val["modal"] = true;
			try {
				string ses = userName2sesId.at(user);
				shared_ptr<ServiceSession> ss = sesId2details.at(ses);
				if (ss->host != user) {
					throw (ccs8("你没有访问此对象的权限。"));
				}
				if (ss->state != 1 && ss->state != 2) {
					throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
				}
				if (ss->state == 2) {
					if (ss->members.size() < 2) {
						//throw ccs8("成员数量不足。请重新开始此会话。");
					}
					string param = json["param"].asString();
					if (!idioms_database.contains(param)) {
						throw ccs8("抱歉，但“") + param + ccs8("”似乎不是成语。");
					}
					ss->phrases.push_front(param);
					ss->state = 16;
					UpdateInfluencedUserWebUI(ses);
					ss->current_dragon_user_index = (std::min)(size_t(1), ss->members.size() - 1);
					UpdateMembersOrderForSess(ss);
					wsReplyDragon(ses);
				}
				else {
					ss->state = 2;
					UpdateInfluencedUserWebUI(ses);
				}
			}
			catch (string s) {
				val["error"] = s;
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
			return;
		}

		if (type == "get-dragon-data") {
			Json::Value val;
			val["type"] = "error-ui";
			try {
				string ses = userName2sesId.at(user);
				shared_ptr<ServiceSession> ss = sesId2details.at(ses);
				if (ss->state != 16) {
					throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
				}
				wsReplyDragon(ses, user);
			}
			catch (string s) {
				val["error"] = s;
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
			return;
		}

		if (type == "submit-dragon") {
			Json::Value val;
			val["type"] = "error-ui";
			try {
				string ses = userName2sesId.at(user);
				shared_ptr<ServiceSession> ss = sesId2details.at(ses);
				if (ss->membersOrder[ss->current_dragon_user_index] != user) {
					throw ccs8("当前不是你的回合。");
				}
				string param = json["userinput"].asString();
				if (json["useSkipChance"].asBool()) {
					size_t chances = ss->membersSkipChance.at(user);
					if (chances < 1) throw ccs8("你的跳过机会已用完。"
						"Tips: 看到旁边的“认输”按钮了吗？:)");
					ss->membersSkipChance[user] = chances - 1;
					param = ss->phrases.at(0);
				}
				else if (!idioms_database.contains(param)) {
					val["type"] = "dragon-unacceptable-phrase";
					val["phrase"] = param;
					throw ""s;
				}
				long long new_index = ss->current_dragon_user_index + 1;
				if ((size_t)new_index >= ss->members.size()) new_index = 0;
				ss->current_dragon_user_index = new_index;
				ss->phrases.push_front(param);
				wsReplyDragon(ses);
			}
			catch (string s) {
				val["error"] = s;
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
			return;
		}

		if (type == "im-loser") {
			Json::Value val;
			val["type"] = "error-ui";
			try {
				string ses = userName2sesId.at(user);
				shared_ptr<ServiceSession> ss = sesId2details.at(ses);
				if (ss->state != 16) {
					throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
				}

				ss->losers.insert(user);
				UpdateMembersOrderForSess(ss);
				wsReplyDragon(ses, user);

				Json::FastWriter fastWriter;
				val["type"] = "dragon-loser-ack";
				auto& i = ss->members.at(user);
				for (auto& j : i) {
					j->send(fastWriter.write(val));
				}
				val["type"] = "dragon-s2c-message";
				val["msgtype"] = "success";
				val["message"] = user + " 认输了！";
				for (auto& i : ss->members) {
					for (auto& j : i.second) {
						j->send(fastWriter.write(val));
					}
				}
			}
			catch (string s) {
				val["error"] = s;
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
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
	catch (std::exception exc) {
		Json::Value json;
		json["success"] = false;
		json["type"] = "error";
		json["level"] = "exception";
		json["exception"] = exc.what();
		Json::FastWriter fastWriter;
		wsConnPtr->send(fastWriter.write(json));
	}
	catch (...) {
		// 无法处理
	}
}


