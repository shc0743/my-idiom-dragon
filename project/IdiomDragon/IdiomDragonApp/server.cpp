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
		// 处理错误  
		return "";
	}

	// 初始化消息摘要上下文  
	if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL)) {
		// 处理错误  
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	// 提供数据  
	if (1 != EVP_DigestUpdate(mdctx, str.data(), str.size())) {
		// 处理错误  
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	// 计算哈希值  
	if (1 != EVP_DigestFinal_ex(mdctx, hash, &hash_len)) {
		// 处理错误  
		EVP_MD_CTX_free(mdctx);
		return "";
	}

	// 释放消息摘要上下文  
	EVP_MD_CTX_free(mdctx);

	// 将哈希值转换为十六进制字符串  
	std::stringstream ss;
	for (unsigned int i = 0; i < hash_len; i++) {
		ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
	}

	return ss.str();
}




vector<MyUserData> userdata;


unordered_map<string, string> sess2UserName;


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
		//// token 参数就是一条字符串token
		//MySessList.normal.push_back(ws2s(tok));
	}

}

bool __stdcall VerifyAuthSession(std::string sessId) {
	if (sessId.length() > 2048) return false;
	try {
		return sess2UserName.at(sessId).empty() == false;
	}
	catch (...) { return false; }
}

bool __stdcall LoginAndCreateSessionData(string username, string password, HttpResponsePtr& resp) {
	for (auto& i : userdata) {
		if (i.name == username && i.pswd == password) {
			string sess = GenerateUUID();
			sess2UserName.insert(make_pair(sess, i.name));
			Cookie cookie;
			cookie.setHttpOnly(true);
			cookie.setPath("/");
			cookie.setKey(SessCookieName);
			cookie.setValue(sess);
			resp->addCookie(cookie);
			return true;
		}
	}
	return false;
}

bool __stdcall LogOutAndRemoveSessionData(string sess, HttpResponsePtr& resp) {
	try {
		sess2UserName.erase(sess);
		Cookie cookie;
		cookie.setHttpOnly(true);
		cookie.setPath("/");
		cookie.setKey(SessCookieName);
		cookie.setValue("deleted");
		cookie.setExpiresDate(trantor::Date(0));
		resp->addCookie(cookie);
		return true;
	}
	catch (...) { return false; }
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
	resp->setContentTypeCode(CT_APPLICATION_JSON);
	resp->setBody("{\"success\":true}");
	callback(resp);
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

	HttpResponsePtr resp = HttpResponse::newHttpResponse();
	CORSadd(req, resp);
	if (LoginAndCreateSessionData(u, p, resp)) {
		resp->setContentTypeCode(CT_APPLICATION_JSON);
		resp->setBody("{\"success\":true}");
	}
	else {
		resp->setStatusCode(k401Unauthorized);
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
	string uname = sess2UserName.at(sess);

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




