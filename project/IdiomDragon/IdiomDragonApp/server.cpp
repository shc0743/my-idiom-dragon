#include "server.h"
#include <string>
#include "../../resource/tool.h"
#include <openssl/evp.h>
using namespace server;
using namespace drogon;

using namespace std;


std::string ossl_sha256(const std::string str) {
	unsigned char hash[EVP_MAX_MD_SIZE];
	unsigned int hash_len;
	EVP_MD_CTX* mdctx = EVP_MD_CTX_new();

	if (!mdctx) {
		// �������  
		return "";
	}

	// ��ʼ����ϢժҪ������  
	if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
		// �������  
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	// �ṩ����  
	if (1 != EVP_DigestUpdate(mdctx, str.data(), str.size())) {
		// �������  
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	// �����ϣֵ  
	if (1 != EVP_DigestFinal_ex(mdctx, hash, &hash_len)) {
		// �������  
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	// �ͷ���ϢժҪ������  
	EVP_MD_CTX_free(mdctx);

	// ����ϣֵת��Ϊʮ�������ַ���  
	std::stringstream ss;
	for (unsigned int i = 0; i < hash_len; i++) {
		ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	}

	return ss.str();
}




vector<MyUserData> userdata;


unordered_map<string, string> loginSess2UserName;

unordered_set<string> idioms_database;


extern unordered_map<string, ServiceSession> sesId2details;
extern unordered_map<string, string> userName2sesId;


void InitAuthTokenVerify(CmdLineW& cl) {
	wstring tok;
	if (!cl.getopt(L"token", tok)) return;
	if (tok.starts_with(L"File;")) {
		// File;C:\Path\To\File.txt
		tok.erase(0, 7);
		fstream fp(L"users.data", ios::in);
		if (!fp) {
			fstream fp("server.log", ios::app);
			fp << "Error: Cannot open users.data" << endl;
			return;
		}
		char buffer[2048]{};
		while (fp.getline(buffer, 2048)) {
			wstring i = ConvertUTF8ToUTF16(buffer);
			if (i.npos == i.find(L"=")) continue;
			wstring wname = i.substr(0, i.find(L"="));
			wstring wpswd = i.substr(i.find(L"=") + 1);
			userdata.push_back(MyUserData{
				.name = ConvertUTF16ToUTF8(wname),
				.pswd = ConvertUTF16ToUTF8(wpswd),
			});
		}
		fp.close();
	}
	else {
		//// token ��������һ���ַ���token
		//MySessList.normal.push_back(ws2s(tok));
	}

}

bool __stdcall VerifyAuthSession(std::string sessId) {
	if (sessId.length() > 2048) return false;
	try {
		return loginSess2UserName.at(sessId).empty() == false;
	}
	catch (...) { return false; }
}

bool __stdcall LoginAndCreateSessionData(string username, string password, HttpResponsePtr& resp, bool remember) {
	for (auto& i : userdata) {
		if (i.name == username && i.pswd == password) {
			string sess = GenerateUUID();
			loginSess2UserName.insert(make_pair(sess, i.name));
			Cookie cookie;
			cookie.setHttpOnly(true);
			cookie.setPath("/");
			cookie.setKey(SessCookieName);
			cookie.setValue(sess);
			resp->addCookie(cookie);
			if (remember) {
				Cookie cookie2;
				cookie2.setHttpOnly(true);
				cookie2.setPath("/");
				cookie2.setKey(CreditCookieName);
				string linfo = username + "@" + password;
				cookie2.setValue(linfo);
				ULONGLONG t = ((time(0) * 1000) + (86400ull * 30 * 1000)) * 1000;
				trantor::Date date(t);
				LOG_DEBUG << "cookie time number: " << t;
				LOG_DEBUG << "cookie date: " << date.toFormattedString(false);
				cookie2.setExpiresDate(date);
				resp->addCookie(cookie2);
			}
			return true;
		}
	}
	return false;
}

bool __stdcall LogOutAndRemoveSessionData(string sess, HttpResponsePtr& resp) {
	try {
		loginSess2UserName.erase(sess);
		Cookie cookie;
		cookie.setHttpOnly(true);
		cookie.setPath("/");
		cookie.setKey(SessCookieName);
		cookie.setValue("deleted");
		cookie.setExpiresDate(trantor::Date(0));
		resp->addCookie(Cookie(cookie));
		cookie.setKey(CreditCookieName);
		resp->addCookie(cookie);
		return true;
	}
	catch (...) { return false; }
}

bool IsUserAccountInfoValid(const std::string& userInfo) {
	// ���������ַ����е�ÿ���ַ�  
	for (char ch : userInfo) {
		if (ch == '\r' || ch == '\n') {
			return false; // ����ASCII�����ַ�������false  
		}
		// ����Ƿ��ǿհ׷�  
		else if (std::isspace(ch)) {
			return false; // �����հ׷�������false  
		}
		// ����Ƿ��ǵȺ�  
		else if (ch == '=') {
			return false; // �����Ⱥţ�����false  
		}
	}
	// ��������������ַ���û�з�����Ч�ַ����򷵻�true  
	return true;
}

bool WsCreateUser(string username, string password) {
	if (!IsUserAccountInfoValid(username) || !IsUserAccountInfoValid(password)) return false;
	// ȷ������û���û�б�ע��
	for (const auto& i : userdata) {
		if (username == i.name) return false;
	}
	string sz = username + "=" + password + "\n";
	// ���������ļ�
	fstream fp(L"users.data", ios::app);
	if (!fp) return false;
	fp.write(sz.c_str(), sz.length());
	fp.close();

	// �����ڴ��е�����
	MyUserData ud{};
	ud.name = username; ud.pswd = password;
	userdata.push_back(ud);

	return true;
}




void server::AuthFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& fcb, FilterChainCallback&& fccb)
{
	string sess = [&] () -> string {
		try {
			return req->getCookie(SessCookieName);
		} 
		catch (...) { return ""; }
	}();
	if (req->method() == Options || VerifyAuthSession(sess)) {
		return fccb();
	}
	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	resp->addHeader("access-control-allow-origin", "*");
	resp->addHeader("access-control-allow-methods", req->getHeader("access-control-request-method") + ",OPTIONS");
	resp->addHeader("access-control-allow-headers", req->getHeader("access-control-request-headers"));
	resp->setStatusCode(k401Unauthorized);
	resp->setBody("");
	fcb(resp);
}




static std::string generate_error_string_from_last_error(std::string description = "") {
	std::wstring errStr = LastErrorStrW();
	std::string sErrStr;
	std::string suffix = description + " (error code: " + std::to_string(GetLastError()) + ")";
	if (!ConvertUTF16ToUTF8(errStr, sErrStr)) {
		return suffix;
	}
	return sErrStr + " - " + suffix;
}


void server::MainServer::auth(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	string sess = [&]() -> string {
		try { return req->getCookie(SessCookieName); }
		catch (...) { return ""; }
	}();
	if (req->method() == Options || VerifyAuthSession(sess)) {
		resp->setContentTypeCode(CT_APPLICATION_JSON);
		resp->setBody("{\"success\":true}");
		return callback(resp);
	}

	string cred = [&]() -> string {
		try { return req->getCookie(CreditCookieName); }
		catch (...) { return ""; }
	}();
	if (cred.empty() || cred.npos == cred.find("@")) {
		resp->setStatusCode(k401Unauthorized);
		return callback(resp);
	}

	string u = cred.substr(0, cred.find("@"));
	string p = cred.substr(cred.find("@") + 1);

	if (LoginAndCreateSessionData(u, p, resp, true)) {
		resp->setContentTypeCode(CT_APPLICATION_JSON);
		resp->setBody("{\"success\":true}");
		return callback(resp);
	}
	else {
		resp->setStatusCode(k401Unauthorized);
		return callback(resp);
	}
}


void server::MainServer::login(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string sess = [&]() -> string {
		try {
			return req->getCookie(SessCookieName);
		}
		catch (...) { return ""; }
	}();
	if (req->method() == Options || VerifyAuthSession(sess)) {
		HttpResponsePtr resp = HttpResponse::newHttpResponse();
		CORSadd(req, resp);
		resp->setStatusCode(k200OK);
		return callback(resp);
	}
	string u = req->getParameter("user");
	string p = req->getParameter("password");
	string r = req->getParameter("remember");

	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	if (LoginAndCreateSessionData(u, p, resp, r == "true")) {
		resp->setContentTypeCode(CT_APPLICATION_JSON);
		resp->setBody("{\"success\":true}");
	}
	else {
		resp->setStatusCode(k401Unauthorized);
	}
	callback(resp);
}


void server::MainServer::reg(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string u = req->getParameter("user");
	string p = req->getParameter("password");

	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	if (WsCreateUser(u, p)) {
		resp->setContentTypeCode(CT_APPLICATION_JSON);
		resp->setBody("{\"success\":true}");
	}
	else {
		resp->setStatusCode(k500InternalServerError);
	}
	callback(resp);
}


void server::MainServer::logout(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string sess = [&]() -> string {
		try {
			return req->getCookie(SessCookieName);
		}
		catch (...) { return ""; }
	}();
	
	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	if (!LogOutAndRemoveSessionData(sess, resp)) {
		resp->setStatusCode(k500InternalServerError);
	}
	callback(resp);
}


void server::MainServer::editPassword(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string sess = req->getCookie(SessCookieName);
	string uname = loginSess2UserName.at(sess);
	string oldPswd = req->getParameter("o");
	string newPswd = req->getParameter("n");

	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);

	if (!LoginAndCreateSessionData(uname, oldPswd, resp, false)) {
		resp->setStatusCode(k403Forbidden);
		return callback(resp);
	}
	if (!IsUserAccountInfoValid(newPswd)) {
		resp->setStatusCode(k400BadRequest);
		return callback(resp);
	}

	static mutex mx;
	mx.lock();
	// �����ļ��е���Ϣ
	{
		string uuname = uname + "=";
		fstream fp(L"users.data", ios::in | ios::out);
		fstream fp2(L"users.data.swap", ios::out);
		if (!fp || !fp2) {
			if (fp) fp.close();
			if (fp2) fp2.close();
			resp->setStatusCode(k500InternalServerError);
			callback(resp);
			mx.unlock();
			return;
		}
		char buffer[2048]{};
		while (fp.getline(buffer, 2048)) {
			string i = (buffer);
			if (i.starts_with(uuname)) {
				i = uuname + newPswd + "\n";
			}
			else {
				i += "\n";
			}
			fp2.write(i.c_str(), i.size());
		}
		fp2.close();
		fp.close();
		DeleteFileW(L"users.data");
		MoveFileW(L"users.data.swap", L"users.data");
	}
	// �����ڴ��е���Ϣ
	for (auto& i : userdata) {
		if (i.name == uname) {
			i.pswd = newPswd; break;
		}
	}
	mx.unlock();

	resp->setContentTypeCode(CT_APPLICATION_JSON);
	resp->setBody("{\"success\":true}");

	callback(resp);
}


void server::MainServer::isSessionExpired(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string sess = [&]() -> string {
		try {
			return req->getCookie(SessCookieName);
		}
		catch (...) { return ""; }
	}();
	if (req->method() == Options) {
		HttpResponsePtr resp = HttpResponse::newHttpResponse();
		CORSadd(req, resp); return callback(resp);
	}

	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	if (VerifyAuthSession(sess)) resp->setBody("valid");
	else if (sess.empty()) resp->setBody("no");
	else resp->setBody("yes");
	callback(resp);
}



void server::MainServer::meinfo(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string sess = req->getCookie(SessCookieName);
	string uname = loginSess2UserName.at(sess);

	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);

	Json::Value data;

	data["name"] = uname;

	Json::FastWriter fastWriter;
	std::string jsonString = fastWriter.write(data);

	resp->setContentTypeCode(CT_APPLICATION_JSON);
	resp->setBody(jsonString);

	callback(resp);
}



void server::MainServer::accountpunishinfo(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) const
{
	string sess = req->getCookie(SessCookieName);
	wstring uname = ConvertUTF8ToUTF16(loginSess2UserName.at(sess));
	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	resp->setContentTypeCode(CT_TEXT_PLAIN);

	// ˽��
	if (uname == L"����" || uname == L"�ְ�") resp->setBody(ConvertUTF16ToUTF8(L"����"));
	else resp->setBody(ConvertUTF16ToUTF8(L"<a target=_blank href=\"https://genshin.hoyoverse.com/en/character/Fontaine?char=6\">ܽ����</a>"));

	callback(resp);
}




