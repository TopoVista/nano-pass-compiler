#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

#include "service/compiler_service.h"
#include <llvm/Config/llvm-config.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
inline void closeSocket(Socket socket) { closesocket(socket); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Socket = int;
constexpr Socket kInvalidSocket = -1;
inline void closeSocket(Socket socket) { close(socket); }
#endif

namespace {
using namespace nanopass::service;

// This is a small, intentionally strict request decoder.  It supports only
// the JSON values the public API accepts, rejects malformed escape sequences,
// and never treats a client string as a file name or a shell command.
class JsonReader {
public:
  explicit JsonReader(std::string_view input) : input_(input) {}
  bool object(std::map<std::string, std::string> &values, std::string &error) {
    skip(); if (!take('{')) return fail(error, "expected JSON object");
    skip(); if (take('}')) return true;
    while (true) {
      auto key=string(error); if (!key) return false;
      skip(); if (!take(':')) return fail(error, "expected ':'");
      auto value=rawValue(error); if (!value) return false;
      if (!values.emplace(*key,*value).second) return fail(error,"duplicate JSON key");
      skip(); if (take('}')) return true; if (!take(',')) return fail(error,"expected ',' or '}'");
    }
  }
  static std::optional<std::string> decodeString(std::string_view raw, std::string &error) {
    JsonReader reader(raw); auto value=reader.string(error); reader.skip(); if (!value || reader.position_ != raw.size()) { if(error.empty())error="expected JSON string"; return std::nullopt; } return value;
  }
private:
  std::string_view input_; size_t position_=0;
  void skip(){while(position_<input_.size()&&std::isspace(static_cast<unsigned char>(input_[position_])))++position_;}
  bool take(char value){skip();if(position_>=input_.size()||input_[position_]!=value)return false;++position_;return true;}
  static bool fail(std::string&error,std::string message){error=std::move(message);return false;}
  std::optional<std::string> string(std::string &error) {
    skip(); if(position_>=input_.size()||input_[position_++]!='"'){error="expected JSON string";return std::nullopt;} std::string value;
    while(position_<input_.size()){char c=input_[position_++];if(c=='"')return value;if(static_cast<unsigned char>(c)<0x20){error="control character in JSON string";return std::nullopt;}if(c!='\\'){value+=c;continue;}if(position_>=input_.size()){error="truncated JSON escape";return std::nullopt;}switch(input_[position_++]){case '"':value+='"';break;case '\\':value+='\\';break;case '/':value+='/';break;case 'b':value+='\b';break;case 'f':value+='\f';break;case 'n':value+='\n';break;case 'r':value+='\r';break;case 't':value+='\t';break;default:error="unsupported JSON escape";return std::nullopt;}}
    error="unterminated JSON string";return std::nullopt;
  }
  std::optional<std::string> rawValue(std::string &error) {
    skip();const size_t start=position_;if(position_>=input_.size()){error="missing JSON value";return std::nullopt;}if(input_[position_]=='"'){auto ignored=string(error);if(!ignored)return std::nullopt;return std::string(input_.substr(start,position_-start));}
    if(input_[position_]=='['){int depth=0;bool quoted=false;while(position_<input_.size()){const char c=input_[position_++];if(quoted){if(c=='\\'&&position_<input_.size())++position_;else if(c=='"')quoted=false;continue;}if(c=='"')quoted=true;else if(c=='[')++depth;else if(c==']'&&--depth==0)return std::string(input_.substr(start,position_-start));}error="unterminated JSON array";return std::nullopt;}
    while(position_<input_.size()&&input_[position_]!=','&&input_[position_]!='}')++position_;auto value=input_.substr(start,position_-start);while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.back())))value.remove_suffix(1);if(value.empty()){error="missing JSON value";return std::nullopt;}return std::string(value);
  }
};

std::optional<bool> jsonBool(std::string_view raw){if(raw=="true")return true;if(raw=="false")return false;return std::nullopt;}
std::optional<int> jsonInt(std::string_view raw){try{size_t offset=0;int value=std::stoi(std::string(raw),&offset);if(offset==raw.size())return value;}catch(...){}return std::nullopt;}
std::optional<std::vector<std::string>> jsonStrings(std::string_view raw,std::string &error){JsonReader reader(raw);std::map<std::string,std::string>ignored;(void)ignored;std::vector<std::string>result;size_t i=0;auto space=[&]{while(i<raw.size()&&std::isspace(static_cast<unsigned char>(raw[i])))++i;};space();if(i>=raw.size()||raw[i++]!='['){error="expected array";return std::nullopt;}space();if(i<raw.size()&&raw[i]==']'){++i;return result;}while(i<raw.size()){size_t start=i;if(raw[i]!='"'){error="array items must be strings";return std::nullopt;}++i;bool escape=false;while(i<raw.size()){char c=raw[i++];if(escape){escape=false;continue;}if(c=='\\'){escape=true;continue;}if(c=='"')break;}auto text=JsonReader::decodeString(raw.substr(start,i-start),error);if(!text)return std::nullopt;result.push_back(*text);space();if(i<raw.size()&&raw[i]==']'){++i;space();return i==raw.size()?std::optional<std::vector<std::string>>(result):std::nullopt;}if(i>=raw.size()||raw[i++]!=','){error="expected array separator";return std::nullopt;}space();}error="unterminated array";return std::nullopt;}

bool decodeCompileRequest(const std::string &body, CompileRequestV1 &request, std::string &error) {
  std::map<std::string,std::string> values; if(!JsonReader(body).object(values,error))return false;
  static const std::set<std::string> allowed{
      "schemaVersion", "source", "optimizationLevel", "policy", "passes", "mode",
      "includeSource", "includeAst", "includeDesugaredAst", "includeCore",
      "includeAnf", "includeCfg", "includeSsa", "includeOptimizedSsa",
      "includeLLVM", "includeGraphs", "includeRemarks", "includeTelemetry",
      "includeProvenance"};
  for (const auto &[key, value] : values) {
    (void)value;
    if (!allowed.contains(key)) { error = "unknown request field '" + key + "'"; return false; }
  }
  auto source=values.find("source");if(source==values.end()){error="missing required field 'source'";return false;}auto decoded=JsonReader::decodeString(source->second,error);if(!decoded)return false;request.source=*decoded;
  if(auto it=values.find("schemaVersion");it!=values.end()){auto value=jsonInt(it->second);if(!value||*value<0){error="schemaVersion must be an integer";return false;}request.schemaVersion=static_cast<uint32_t>(*value);}
  if(auto it=values.find("optimizationLevel");it!=values.end()){auto value=jsonInt(it->second);if(!value){error="optimizationLevel must be an integer";return false;}request.optimizationLevel=*value;}
  if(auto it=values.find("policy");it!=values.end()){auto value=JsonReader::decodeString(it->second,error);if(!value)return false;request.optimizationPolicy=*value;}
  if(auto it=values.find("passes");it!=values.end()){auto list=jsonStrings(it->second,error);if(!list)return false;request.passes=*list;}
  auto option=[&](std::string_view name,bool &field){if(auto it=values.find(std::string(name));it!=values.end()){auto value=jsonBool(it->second);if(!value){error=std::string(name)+" must be boolean";return false;}field=*value;}return true;};
  return option("includeSource",request.include.source)&&option("includeAst",request.include.ast)&&option("includeDesugaredAst",request.include.desugaredAst)&&option("includeCore",request.include.core)&&option("includeAnf",request.include.anf)&&option("includeCfg",request.include.cfg)&&option("includeSsa",request.include.ssa)&&option("includeOptimizedSsa",request.include.optimizedSsa)&&option("includeLLVM",request.include.llvm)&&option("includeGraphs",request.include.graphs)&&option("includeRemarks",request.include.remarks)&&option("includeTelemetry",request.include.telemetry)&&option("includeProvenance",request.include.provenance);
}

std::optional<ExecutionMode> decodeRunMode(const std::string &body, std::string &error) {
  std::map<std::string,std::string> values; if(!JsonReader(body).object(values,error)) return std::nullopt;
  const auto found=values.find("mode"); if(found==values.end()) return ExecutionMode::SSAInterpreter;
  const auto mode=JsonReader::decodeString(found->second,error); if(!mode) return std::nullopt;
  if(*mode=="core"||*mode=="interpreter") return ExecutionMode::CoreInterpreter;
  if(*mode=="ssa") return ExecutionMode::SSAInterpreter;
  if(*mode=="jit") return ExecutionMode::JIT;
  if(*mode=="adaptive-jit"||*mode=="adaptive") return ExecutionMode::AdaptiveJIT;
  error="unsupported execution mode"; return std::nullopt;
}

std::string errorJson(int status,std::string_view code,std::string_view message){std::ostringstream out;out<<"{\"schemaVersion\":1,\"success\":false,\"status\":"<<status<<",\"error\":{\"code\":\""<<jsonEscape(code)<<"\",\"message\":\""<<jsonEscape(message)<<"\"}}";return out.str();}
std::string healthJson(const CompilerService &service) {
  std::ostringstream out;
  out << "{\"schemaVersion\":1,\"status\":\"ok\",\"ready\":true"
      << ",\"compilerRevision\":\"phase11-service-v1\""
      << ",\"llvmVersion\":\"" LLVM_VERSION_STRING "\""
      << ",\"executionPolicy\":\"" << executionPolicyName(service.executionPolicy()) << "\"}";
  return out.str();
}
void sendResponse(Socket socket,int status,std::string_view body){const char*text=status==200?"OK":status==400?"Bad Request":status==404?"Not Found":status==413?"Payload Too Large":"Internal Server Error";std::ostringstream response;response<<"HTTP/1.1 "<<status<<' '<<text<<"\r\nContent-Type: application/json; charset=utf-8\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Headers: Content-Type\r\nContent-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body;const auto data=response.str();size_t sent=0;while(sent<data.size()){const int result=send(socket,data.data()+sent,static_cast<int>(data.size()-sent),0);if(result<=0)break;sent+=static_cast<size_t>(result);}}

std::string readRequest(Socket socket){std::string request;std::array<char,8192>chunk{};while(request.size()<512*1024){const int n=recv(socket,chunk.data(),static_cast<int>(chunk.size()),0);if(n<=0)break;request.append(chunk.data(),static_cast<size_t>(n));if(request.find("\r\n\r\n")!=std::string::npos){const auto end=request.find("\r\n\r\n");const auto marker=request.find("Content-Length:");if(marker==std::string::npos||marker>end)break;const auto lineEnd=request.find("\r\n",marker);const size_t length=static_cast<size_t>(std::strtoull(request.substr(marker+15,lineEnd-marker-15).c_str(),nullptr,10));if(request.size()>=end+4+length)break;}}return request;}
void setClientReceiveTimeout(Socket socket) {
#ifdef _WIN32
  DWORD timeoutMs = 5000;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char *>(&timeoutMs), sizeof(timeoutMs));
#else
  timeval timeout{}; timeout.tv_sec = 5;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}
void handle(Socket socket,const CompilerService &service){const auto request=readRequest(socket);if(request.size()>=512*1024){sendResponse(socket,413,errorJson(413,"REQUEST_LIMIT_EXCEEDED","request exceeds server byte limit"));return;}const auto headerEnd=request.find("\r\n\r\n");if(headerEnd==std::string::npos){sendResponse(socket,400,errorJson(400,"MALFORMED_HTTP","missing HTTP header terminator"));return;}const auto lineEnd=request.find("\r\n");const auto line=request.substr(0,lineEnd);std::istringstream words(line);std::string method,path,version;words>>method>>path>>version;if(method=="OPTIONS"){sendResponse(socket,200,"{\"schemaVersion\":1,\"success\":true}");return;}if(method=="GET"&&(path=="/api/v1/health"||path=="/api/v1/readiness")){sendResponse(socket,200,healthJson(service));return;}if(method!="POST"){sendResponse(socket,404,errorJson(404,"ROUTE_NOT_FOUND","unsupported endpoint"));return;}CompileRequestV1 compile;std::string error;const auto body=request.substr(headerEnd+4);if(!decodeCompileRequest(body,compile,error)){sendResponse(socket,400,errorJson(400,"MALFORMED_REQUEST",error));return;}if(path=="/api/v1/correctness"){auto result=service.checkCorrectness(compile);sendResponse(socket,result.compilation.success?200:400,correctnessResponseJson(result));return;}if(path=="/api/v1/compile"||path=="/api/v1/analyze"||path=="/api/v1/optimize"){if(path=="/api/v1/analyze"){compile.include.graphs=true;compile.include.provenance=true;compile.include.ssa=true;}if(path=="/api/v1/optimize"){compile.include.optimizedSsa=true;compile.include.remarks=true;compile.include.telemetry=true;}auto result=service.compile(compile);sendResponse(socket,result.success?200:400,compileResponseJson(result));return;}if(path=="/api/v1/run"||path=="/api/v1/profile"){RunRequestV1 run;run.compile=compile;if(path=="/api/v1/profile")run.mode=ExecutionMode::SSAInterpreter;else {auto mode=decodeRunMode(body,error);if(!mode){sendResponse(socket,400,errorJson(400,"MALFORMED_REQUEST",error));return;}run.mode=*mode;}auto result=service.run(run);sendResponse(socket,result.compilation.success?200:400,runResponseJson(result));return;}sendResponse(socket,404,errorJson(404,"ROUTE_NOT_FOUND","unsupported endpoint"));}
}

int main(int argc,char**argv){int port=8080;bool bindAll=false;ExecutionPolicy policy=ExecutionPolicy::LocalTrusted;for(int i=1;i<argc;++i){std::string_view arg=argv[i];if(arg.starts_with("--port=")){try{port=std::stoi(std::string(arg.substr(7)));}catch(...){std::cerr<<"invalid port\n";return 2;}}else if(arg.starts_with("--execution-policy=")){const auto value=arg.substr(19);if(value=="disabled")policy=ExecutionPolicy::Disabled;else if(value=="interpreter-only")policy=ExecutionPolicy::InterpreterOnly;else if(value=="local-trusted")policy=ExecutionPolicy::LocalTrusted;else{std::cerr<<"invalid execution policy\n";return 2;}}else if(arg=="--bind=loopback")bindAll=false;else if(arg=="--bind=all")bindAll=true;else if(arg=="--help"){std::cout<<"nano-server --port=8080 --bind=loopback|all --execution-policy=disabled|interpreter-only|local-trusted\n";return 0;}else{std::cerr<<"unknown argument: "<<arg<<'\n';return 2;}}if(port<1024||port>65535){std::cerr<<"port must be in 1024..65535\n";return 2;}
#ifdef _WIN32
  WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0){std::cerr<<"WSAStartup failed\n";return 1;}
#endif
  Socket server=socket(AF_INET,SOCK_STREAM,0);if(server==kInvalidSocket){std::cerr<<"could not create server socket\n";return 1;}int reuse=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(bindAll?INADDR_ANY:INADDR_LOOPBACK);address.sin_port=htons(static_cast<uint16_t>(port));if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||listen(server,16)<0){std::cerr<<"could not bind nano-server\n";closeSocket(server);return 1;}std::cout<<"nano-server listening on "<<(bindAll?"all interfaces":"127.0.0.1")<<":"<<port<<" (schema v1, "<<executionPolicyName(policy)<<")\n";CompilerService service({},policy);while(true){Socket client=accept(server,nullptr,nullptr);if(client==kInvalidSocket)continue;setClientReceiveTimeout(client);handle(client,service);closeSocket(client);} }
