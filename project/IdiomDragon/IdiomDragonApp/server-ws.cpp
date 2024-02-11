#include "server.h"
#include <string>
#include "../../resource/tool.h"
#include <openssl/evp.h>
#include "../lib/Pinyin.h"
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
		auto& connections = ss->membersToConnections.at(uname);
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
		auto& vec = ss->membersToConnections.at(uname);
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
			members.append(i);
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
		if (!ss->losers.contains(i)) ss->membersOrder.push_back(i);
	}
}
static void StepMemberIndexForSess(shared_ptr<ServiceSession> ss) {
	long long new_index = ss->current_dragon_user_index + 1;
	if ((size_t)new_index >= (ss->membersOrder.size())) new_index = 0;
	ss->current_dragon_user_index = new_index;
}
static void RemoveMemberInSess(shared_ptr<ServiceSession> ss, string user) {
	auto cindex = ss->current_dragon_user_index;
	auto& myVector = ss->membersOrder;
	auto it = std::find(myVector.begin(), myVector.end(), user);
	if (it != myVector.end()) {
		auto uindex = std::distance(myVector.begin(), it);
		ss->membersOrder.erase(it);
		if (cindex >= uindex) {
			// 删除user会对后面的用户产生影响，需要处理
			ss->current_dragon_user_index -= 1;
			if (ss->current_dragon_user_index < 0) 
				ss->current_dragon_user_index = ss->membersOrder.size() - 1;
		}
	}
	ss->members.erase(std::find(ss->members.begin(), ss->members.end(), user));
	ss->membersToConnections.erase(user);
	userName2sesId.erase(user);
	UpdateMembersOrderForSess(ss);
}
static void MoveMemberToLosersInSess(shared_ptr<ServiceSession> ss, string user) {
	ss->losers.insert(user);
	//UpdateMembersOrderForSess(ss);
	auto cindex = ss->current_dragon_user_index;
	auto& myVector = ss->membersOrder;
	auto it = std::find(myVector.begin(), myVector.end(), user);
	if (it != myVector.end()) {
		auto uindex = std::distance(myVector.begin(), it);
		if (cindex >= uindex) {
			// 删除user会对后面的用户产生影响，需要处理
			ss->current_dragon_user_index -= 1;
			if (ss->current_dragon_user_index < 0) 
				ss->current_dragon_user_index = ss->membersOrder.size() - 2;
		}
	}
	UpdateMembersOrderForSess(ss);
}


static void UpdateInfluencedUserWebUI(string session_id) {
	try {
		shared_ptr<ServiceSession> ss = sesId2details.at(session_id);
		for (auto& i : ss->membersToConnections) {
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
		if ((ss->membersOrder.size() == 1 && ss->state >= 2) || !ss->winnerStr.empty()) {
			val["completed"] = true;
			if (ss->winnerStr.empty()) {
				val["winner"] = ss->winnerStr = ss->membersOrder.at(0);
				if (ss->membersCountWhenEnded < 1) ss->membersCountWhenEnded = ss->members.size();
				if (ss->state != 100) ss->state = 100;
			}
			else {
				val["winner"] = ss->winnerStr;
			}
		}
		if (ss->l_appealingPhrase.isValid) {
			val["appealingPhrase"] = ss->l_appealingPhrase.phrase;
		}
		Json::Value losers(Json::arrayValue);
		for (auto& i : ss->losers) {
			losers.append(i);
		}
		val["losers"] = losers;
		if (ss->challengeAgainRequestTime && (time(0) - ss->challengeAgainRequestTime < 15)) {
			val["challengeAgain_requestTime"] = ss->challengeAgainRequestTime;
			Json::Value members(Json::arrayValue);
			for (auto& i : ss->challengeAgain_agreedMembers) {
				members.append(i);
			}
			val["challengeAgain_agreedMembers"] = members;
		}
		val["state"] = ss->state;
		for (auto& i : ss->membersToConnections) {
			if (!targetUser.empty() && i.first != targetUser) continue;
			try {
				val["skipCount"] = ss->membersSkipChance.at(i.first);
			}
			catch (std::out_of_range) {
				val["skipCount"] = 0;
			}
			val["isLoser"] = ss->losers.contains(i.first);
			for (auto& j : i.second) {
				Json::FastWriter fastWriter;
				j->send(fastWriter.write(val));
			}
		}
	}
	catch (...) {}
}


static bool CheckIdiomPinyin(wstring wcsUserPhrase, wstring wcsLastPhrase) {
	if (wcsUserPhrase[0] == wcsLastPhrase[wcsLastPhrase.length() - 1]) return true;
	// 允许同音
	WzhePinYin::Pinyin py;
	wchar_t cch = wcsUserPhrase[0];
	wchar_t cch2 = wcsLastPhrase[wcsLastPhrase.length() - 1];
	if (!(py.IsChinese(cch) && py.IsChinese(cch2))) {
		return false;
	}
	vector<string>
		py1 = py.GetPinyins(cch),
		py2 = py.GetPinyins(cch2);
	bool r = false;
	for (auto& i : py1) {
		for (auto& j : py2) {
			if (i == j) { r = true; break; }
		}
	}
	return r;
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
				ss->members.push_back(user);
				ss->membersToConnections.insert(make_pair(user, conn));
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
				ss->members.push_back(user);
				ss->membersToConnections.insert(make_pair(user, conn));
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

			if (ss->host == user && ss->state != 100) {
				for (const auto& i : ss->membersToConnections) {
					auto& un = i.first;
					userName2sesId.erase(un);
					for (auto& j : i.second)
						WsSendUserState(j, un);
				}
				ss->members.clear();
				sesId2details.erase(sesId);
			}
			else {
				RemoveMemberInSess(ss, user);
				UpdateInfluencedUserWebUI(sesId);
				UpdateMembersOrderForSess(ss);

			}
			if (ss->members.size() < 1) {
				sesId2details.erase(sesId); // 在所有成员退出后，清除session数据
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
				if (tuser == user) {
					throw std::exception(ccs8("不可以移出自己。").c_str());
				}

				try {
					auto& vec = ss->membersToConnections.at(tuser);
					Json::Value v2;
					v2["type"] = "dragon-removed-from-session";
					v2["error"] = ccs8("您被移出对局");
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
				RemoveMemberInSess(ss, tuser);
				wsReplyDragon(session_id);
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
				if (ss->members.size() < 2) {
					throw ccs8("成员数量不足。");
				}
				if (ss->state == 2) {
					string param = json["param"].asString();
					if (!idioms_database.contains(param)) {
						throw ccs8("抱歉，但“") + param + ccs8("”似乎不是成语。");
					}
					ss->phrases.push_front(param);
					ss->state = 16;
					if (json["noRepeat"].isBool() && json["noRepeat"].asBool()) {
						ss->noRepeatPhrase = true;
					}
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
				if (ss->state != 16 && ss->state != 100) {
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
				if (ss->state != 16) {
					throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
				}
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
				else {
					if (ss->noRepeatPhrase)
						for (size_t i = 0, l = ss->phrases.size(); i < l; ++i) {
							if (param == ss->phrases[i]) { // 发现重复
								throw ccs8("该对局不允许给出重复的成语，因此「") +
									param + ccs8("」不符合规则。(在第" +
										to_wstring(l - i) + L"回合出现)");
							}
						}
					wstring wcsUserPhrase = ConvertUTF8ToUTF16(param);
					wstring wcsLastPhrase = ConvertUTF8ToUTF16(ss->phrases[0]);
					if (!CheckIdiomPinyin(wcsUserPhrase, wcsLastPhrase)) {
						throw ccs8("提供的成语不符合规则。");
					}
					if (!idioms_database.contains(param)) {
						val["type"] = "dragon-unacceptable-phrase";
						val["phrase"] = param;
						throw ""s;
					}
				}
				ss->l_appealingPhrase.isValid = false;
				StepMemberIndexForSess(ss);
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

				MoveMemberToLosersInSess(ss, user);
				wsReplyDragon(ses, user);

				Json::FastWriter fastWriter;
				val["type"] = "dragon-loser-ack";
				auto& i = ss->membersToConnections.at(user);
				for (auto& j : i) {
					j->send(fastWriter.write(val));
				}
				val["type"] = "dragon-s2c-message";
				val["msgtype"] = "success";
				val["message"] = user + ccs8(" 认输了！");
				for (auto& i : ss->membersToConnections) {
					for (auto& j : i.second) {
						j->send(fastWriter.write(val));
					}
				}
				wsReplyDragon(ses);
			}
			catch (string s) {
				val["error"] = s;
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
			return;
		}

		if (type == "get-dragon-record") {
			Json::Value val;
			val["type"] = "receive-dragon-record";

			string ses = userName2sesId.at(user);
			shared_ptr<ServiceSession> ss = sesId2details.at(ses);
			if (ss->state != 16 && ss->state != 100) {
				throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
			}
			Json::Value records(Json::arrayValue);
			for (size_t i = ss->phrases.size(); i > 0; --i) {
				records.append(ss->phrases[i - 1]);
			}
			val["records"] = records;
			
			Json::FastWriter fastWriter;
			wsConnPtr->send(fastWriter.write(val));
			return;
		}

		if (type == "appeal-unacceptable-phrase") {
			Json::Value val;
			string ses = userName2sesId.at(user);
			shared_ptr<ServiceSession> ss = sesId2details.at(ses);
			if (ss->state != 16) {
				throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
			}
			if (ss->membersOrder.at(ss->current_dragon_user_index) != user) {
				throw ccs8("你没有权限执行此操作。");
			}
			string phrase = json["phrase"].asString();
			ss->l_appealingPhrase.isValid = true;
			ss->l_appealingPhrase.user = user;
			ss->l_appealingPhrase.phrase = phrase;
			wsReplyDragon(ses);
			return;
		}

		if (type == "judge-unacceptable-phrase") {
			Json::Value val;
			string ses = userName2sesId.at(user);
			shared_ptr<ServiceSession> ss = sesId2details.at(ses);
			if (ss->state != 16 || ss->l_appealingPhrase.isValid == false) {
				throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
			}
			if (ss->l_appealingPhrase.user == user) {
				throw ccs8("你没有权限执行此操作。");
			}

			string phrase = ss->l_appealingPhrase.phrase;
			wstring wcsUser = ConvertUTF8ToUTF16(phrase),
				wcsLast = ConvertUTF8ToUTF16(ss->phrases[0]);
			if (!CheckIdiomPinyin(wcsUser, wcsLast)) {
				throw ccs8("请求无效。");
			}
			bool result = json["result"].asBool();
			if (!result) {
				ss->l_appealingPhrase.isValid = false;
				wsReplyDragon(ses);
				return;
			}
			idioms_database.insert(phrase);
			// 将更改保存到文件系统
			{
				fstream fp("userphrase.txt", ios::app);
				fp.write(phrase.c_str(), phrase.length());
				fp.write("\r\n", 2);
			}
			val["type"] = "dragon-s2c-message";
			val["msgtype"] = "success";
			val["message"] = ccs8("申诉成功！");
			Json::FastWriter fastWriter;
			try {
				for (auto& i : ss->membersToConnections.at(ss->l_appealingPhrase.user)) {
					i->send(fastWriter.write(val));
				}
			}
			catch (std::out_of_range) {}
			ss->phrases.push_front(phrase);
			StepMemberIndexForSess(ss);
			ss->l_appealingPhrase.isValid = false;
			wsReplyDragon(ses);
			return;
		}

		if (type == "start-with-same-members") {
			Json::Value val;
			val["type"] = "error-ui";
			try {
				string ses = userName2sesId.at(user);
				shared_ptr<ServiceSession> ss = sesId2details.at(ses);
				if (ss->state != 100) {
					throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
				}
				if (ss->host != user) {
					throw ccs8("你没有权限执行此操作。");
				}
				if (ss->members.size() < ss->membersCountWhenEnded) {
					throw ccs8("有成员离开了对局，无法继续。");
				}
				if (ss->members.size() < 2) {
					throw ccs8("成员数量不足以发起一次新对局。");
				}
				if (time(0) - ss->challengeAgainRequestTime < 10) {
					throw ccs8("请求过于频繁。10秒内只能发起一次请求。");
				}
				ss->challengeAgainRequestTime = time(0);
				ss->challengeAgain_agreedMembers.clear();
				ss->challengeAgain_agreedMembers.insert(user);

				Json::FastWriter fastWriter;
				val["type"] = "dragon-start-again-request";
				for (auto& i : ss->membersToConnections) {
					if (i.first == user) continue;
					for (auto& j : i.second) {
						j->send(fastWriter.write(val));
					}
				}
				wsReplyDragon(ses);
			}
			catch (string s) {
				val["error"] = s;
				Json::FastWriter fastWriter;
				wsConnPtr->send(fastWriter.write(val));
			}
			return;
		}

		if (type == "start-with-same-members-resp") {
			Json::Value val;
			val["type"] = "error-ui";
			try {
				string ses = userName2sesId.at(user);
				shared_ptr<ServiceSession> ss = sesId2details.at(ses);
				if (ss->state != 100) {
					throw ccs8("对处于此阶段的对象，无法在其上执行此操作。");
				}
				if (ss->host == user) {
					throw ccs8("你没有权限执行此操作。");
				}
				if (ss->members.size() < ss->membersCountWhenEnded) {
					throw ccs8("有成员离开了对局，无法继续。");
				}
				if (ss->challengeAgain_agreedMembers.contains(user)) {
					throw ccs8("你已经完成过此操作，请勿重复提交。");
				}
				if (time(0) - ss->challengeAgainRequestTime > 15) {
					throw ccs8("请求已超时。");
				}
				
				bool bAgree = json["agree"].isBool() && json["agree"].asBool();
				bool bSucceed = false;
				if (!bAgree) {
					ss->challengeAgainRequestTime = 0;
					ss->challengeAgain_agreedMembers.clear();

					Json::FastWriter fastWriter;
					val["error"] = user + ccs8(" 拒绝了请求。");
					for (auto& i : ss->membersToConnections) {
						//if (i.first != ss->host) continue;
						for (auto& j : i.second) {
							j->send(fastWriter.write(val));
						}
					}
				}
				else {
					ss->challengeAgain_agreedMembers.insert(user);
					if (ss->challengeAgain_agreedMembers.size() >= ss->membersCountWhenEnded) {
						// 所有成员均已同意

						// 先清空以前的资料
						auto& users = ss->members;
						for (auto& i : users) {
							// 清除用户的关联
							userName2sesId.erase(i);
						}
						// 清除session的关联
						sesId2details.erase(ses);

						// 建立新的session
						Json::Value val;
						val["type"] = "receive-initsession";
						do {
							string session_id = to_string(GenerateMySessId());
							if (sesId2details.contains(session_id)) {
								throw ccs8("生成的对局ID出现重复。");
							}
							shared_ptr<ServiceSession> ss2 = make_shared<ServiceSession>();
							ss2->host = ss->host;
							ss2->state = 2;
							for (auto& i : users) {
								ss2->members.push_back(i);
								ss2->membersToConnections.insert(
									make_pair(i, ss->membersToConnections.at(i))
								);
								ss2->membersSkipChance.insert(make_pair(i, MEMBER_CAN_SKIP_TIME));
								userName2sesId.insert(make_pair(i, session_id));
							}
							if (ss2->members.size() < 1) throw ccs8("成员数量异常");
							if (ss2->members.size() < 2) ss2->state = 1;
							sesId2details.insert(make_pair(session_id, ss2));
							val["success"] = true;
							val["sid"] = session_id;
							Json::FastWriter fastWriter;
							for (auto& i : ss->membersToConnections) {
								for (auto& j : i.second) {
									j->send(fastWriter.write(val));
								}
							}
							bSucceed = true;
						} while (0);
					}
				}

				if (!bSucceed) wsReplyDragon(ses);
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


