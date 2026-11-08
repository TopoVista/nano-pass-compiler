#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
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

struct RequestContext {
  uint64_t id = 0;
  std::string path = "unknown";
  std::chrono::steady_clock::time_point started;
};

std::atomic<uint64_t> nextRequestId{1};
thread_local std::optional<RequestContext> activeRequest;

void logResponse(int status) {
  if (!activeRequest) return;
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - activeRequest->started);
  std::cerr << "request_id=req-" << activeRequest->id
            << " endpoint=" << activeRequest->path
            << " status=" << status
            << " duration_ms=" << elapsed.count() << '\n';
}

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

std::string errorJson(int status,std::string_view code,std::string_view message){std::ostringstream out;out<<"{\"schemaVersion\":1,\"success\":false,\"status\":"<<status;if(activeRequest)out<<",\"requestId\":\"req-"<<activeRequest->id<<"\"";out<<",\"error\":{\"code\":\""<<jsonEscape(code)<<"\",\"message\":\""<<jsonEscape(message)<<"\"}}";return out.str();}
std::string healthJson(const CompilerService &service) {
  std::ostringstream out;
  out << "{\"schemaVersion\":1,\"status\":\"ok\",\"ready\":true"
      << ",\"compilerRevision\":\"" << nanopass::build::kCompilerRevision << "\""
      << ",\"llvmVersion\":\"" LLVM_VERSION_STRING "\""
      << ",\"executionPolicy\":\"" << executionPolicyName(service.executionPolicy()) << "\"}";
  return out.str();
}
std::optional<std::string> requestHeader(std::string_view request, std::string_view name) {
  const auto lower = [](std::string_view value) {
    std::string result(value);
    for (char &character : result)
      character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    return result;
  };
  const auto wanted = lower(name);
  size_t lineStart = request.find("\r\n");
  if (lineStart == std::string_view::npos) return std::nullopt;
  lineStart += 2;
  while (lineStart < request.size()) {
    const auto lineEnd = request.find("\r\n", lineStart);
    if (lineEnd == lineStart || lineEnd == std::string_view::npos) break;
    const auto colon = request.find(':', lineStart);
    if (colon != std::string_view::npos && colon < lineEnd && lower(request.substr(lineStart, colon - lineStart)) == wanted) {
      std::string value(request.substr(colon + 1, lineEnd - colon - 1));
      const auto first = value.find_first_not_of(" \t");
      const auto last = value.find_last_not_of(" \t");
      return first == std::string::npos ? std::string{} : value.substr(first, last - first + 1);
    }
    lineStart = lineEnd + 2;
  }
  return std::nullopt;
}

std::set<std::string> allowedOrigins() {
  const char *raw = std::getenv("NANOPASS_ALLOWED_ORIGINS");
  if (!raw || std::string_view(raw).empty()) raw = std::getenv("NANO_ALLOWED_ORIGINS");
  std::set<std::string> origins{"http://127.0.0.1:5173", "http://localhost:5173"};
  if (!raw || std::string_view(raw).empty()) return origins;
  origins.clear();
  std::istringstream values(raw);
  std::string origin;
  while (std::getline(values, origin, ',')) if (!origin.empty()) origins.insert(origin);
  return origins;
}

bool originAllowed(const std::optional<std::string> &origin, const std::set<std::string> &origins) {
  return !origin || origins.contains(*origin);
}

void sendResponse(Socket socket,int status,std::string_view body,const std::optional<std::string> &origin={}){const char*text=status==200?"OK":status==400?"Bad Request":status==403?"Forbidden":status==404?"Not Found":status==413?"Payload Too Large":"Internal Server Error";std::ostringstream response;response<<"HTTP/1.1 "<<status<<' '<<text<<"\r\nContent-Type: application/json; charset=utf-8\r\n";if(origin)response<<"Access-Control-Allow-Origin: "<<*origin<<"\r\nVary: Origin\r\nAccess-Control-Allow-Headers: Content-Type\r\nAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\n";response<<"Content-Length: "<<body.size()<<"\r\nConnection: close\r\n\r\n"<<body;const auto data=response.str();size_t sent=0;while(sent<data.size()){const int result=send(socket,data.data()+sent,static_cast<int>(data.size()-sent),0);if(result<=0)break;sent+=static_cast<size_t>(result);}logResponse(status);}

std::string readRequest(Socket socket) {
  activeRequest = RequestContext{
      nextRequestId.fetch_add(1), "unknown", std::chrono::steady_clock::now()};
  std::string request;
  std::array<char, 8192> chunk{};
  while (request.size() < 512 * 1024) {
    const int received = recv(socket, chunk.data(), static_cast<int>(chunk.size()), 0);
    if (received <= 0) break;
    request.append(chunk.data(), static_cast<size_t>(received));
    const auto headerEnd = request.find("\r\n\r\n");
    if (headerEnd == std::string::npos) continue;
    const auto requestLineEnd = request.find("\r\n");
    if (requestLineEnd != std::string::npos && activeRequest) {
      std::istringstream requestLine(request.substr(0, requestLineEnd));
      std::string method;
      std::string path;
      requestLine >> method >> path;
      if (!path.empty()) activeRequest->path = path;
    }
    const auto contentLength = requestHeader(request, "Content-Length");
    if (!contentLength) break;
    size_t parsed = 0;
    size_t length = 0;
    try {
      length = std::stoull(*contentLength, &parsed);
    } catch (...) {
      break;
    }
    if (parsed != contentLength->size()) break;
    if (request.size() >= headerEnd + 4 + length) break;
  }
  return request;
}
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
void handle(Socket socket,const CompilerService &service,const std::set<std::string> &origins){const auto request=readRequest(socket);const auto origin=requestHeader(request,"Origin");if(!originAllowed(origin,origins)){sendResponse(socket,403,errorJson(403,"ORIGIN_NOT_ALLOWED","request origin is not allowed"));return;}if(request.size()>=512*1024){sendResponse(socket,413,errorJson(413,"REQUEST_LIMIT_EXCEEDED","request exceeds server byte limit"),origin);return;}const auto headerEnd=request.find("\r\n\r\n");if(headerEnd==std::string::npos){sendResponse(socket,400,errorJson(400,"MALFORMED_HTTP","missing HTTP header terminator"),origin);return;}const auto lineEnd=request.find("\r\n");const auto line=request.substr(0,lineEnd);std::istringstream words(line);std::string method,path,version;words>>method>>path>>version;if(method=="OPTIONS"){sendResponse(socket,200,"{\"schemaVersion\":1,\"success\":true}",origin);return;}if(method=="GET"&&(path=="/api/v1/health"||path=="/api/v1/readiness")){sendResponse(socket,200,healthJson(service),origin);return;}if(method!="POST"){sendResponse(socket,404,errorJson(404,"ROUTE_NOT_FOUND","unsupported endpoint"),origin);return;}CompileRequestV1 compile;std::string error;const auto body=request.substr(headerEnd+4);if(!decodeCompileRequest(body,compile,error)){sendResponse(socket,400,errorJson(400,"MALFORMED_REQUEST",error),origin);return;}if(path=="/api/v1/correctness"){auto result=service.checkCorrectness(compile);sendResponse(socket,result.compilation.success?200:400,correctnessResponseJson(result),origin);return;}if(path=="/api/v1/compile"||path=="/api/v1/analyze"||path=="/api/v1/optimize"){if(path=="/api/v1/analyze"){compile.include.graphs=true;compile.include.provenance=true;compile.include.ssa=true;}if(path=="/api/v1/optimize"){compile.include.optimizedSsa=true;compile.include.remarks=true;compile.include.telemetry=true;}auto result=service.compile(compile);sendResponse(socket,result.success?200:400,compileResponseJson(result),origin);return;}if(path=="/api/v1/run"||path=="/api/v1/profile"){RunRequestV1 run;run.compile=compile;if(path=="/api/v1/profile")run.mode=ExecutionMode::SSAInterpreter;else {auto mode=decodeRunMode(body,error);if(!mode){sendResponse(socket,400,errorJson(400,"MALFORMED_REQUEST",error),origin);return;}run.mode=*mode;}auto result=service.run(run);sendResponse(socket,result.compilation.success?200:400,runResponseJson(result),origin);return;}sendResponse(socket,404,errorJson(404,"ROUTE_NOT_FOUND","unsupported endpoint"),origin);}
std::optional<int> environmentPort(const char *name) {
  const char *raw = std::getenv(name);
  if (!raw || std::string_view(raw).empty()) return std::nullopt;
  try {
    size_t parsed = 0;
    const int value = std::stoi(raw, &parsed);
    if (parsed != std::strlen(raw)) return std::nullopt;
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<ExecutionPolicy> parseExecutionPolicy(std::string_view value) {
  if (value == "disabled") return ExecutionPolicy::Disabled;
  if (value == "interpreter-only") return ExecutionPolicy::InterpreterOnly;
  if (value == "local-trusted") return ExecutionPolicy::LocalTrusted;
  return std::nullopt;
}

bool productionEnvironment() {
  const char *environment = std::getenv("NANOPASS_ENV");
  return environment && std::string_view(environment) == "production";
}

}

int main(int argc, char **argv) {
  int port = 8080;
  bool bindAll = false;
  ExecutionPolicy policy = ExecutionPolicy::LocalTrusted;
  ServiceLimits limits;

  if (const auto configuredPort = environmentPort("PORT")) port = *configuredPort;
  else if (std::getenv("PORT")) { std::cerr << "invalid PORT\n"; return 2; }
  if (const char *configuredPolicy = std::getenv("NANOPASS_EXECUTION_POLICY")) {
    const auto parsed = parseExecutionPolicy(configuredPolicy);
    if (!parsed) { std::cerr << "invalid NANOPASS_EXECUTION_POLICY\n"; return 2; }
    policy = *parsed;
  }
  if (const char *configuredBind = std::getenv("NANOPASS_BIND")) {
    if (std::string_view(configuredBind) == "all") bindAll = true;
    else if (std::string_view(configuredBind) == "loopback") bindAll = false;
    else { std::cerr << "invalid NANOPASS_BIND\n"; return 2; }
  }
  if (const auto configuredLimit = environmentPort("NANOPASS_MAX_SOURCE_BYTES")) {
    if (*configuredLimit <= 0 || *configuredLimit > 512 * 1024) {
      std::cerr << "NANOPASS_MAX_SOURCE_BYTES must be in 1..524288\n";
      return 2;
    }
    limits.maxSourceBytes = static_cast<size_t>(*configuredLimit);
  } else if (std::getenv("NANOPASS_MAX_SOURCE_BYTES")) {
    std::cerr << "invalid NANOPASS_MAX_SOURCE_BYTES\n";
    return 2;
  }

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument.starts_with("--port=")) {
      try { port = std::stoi(std::string(argument.substr(7))); }
      catch (...) { std::cerr << "invalid port\n"; return 2; }
    } else if (argument.starts_with("--execution-policy=")) {
      const auto parsed = parseExecutionPolicy(argument.substr(19));
      if (!parsed) { std::cerr << "invalid execution policy\n"; return 2; }
      policy = *parsed;
    } else if (argument == "--bind=loopback") {
      bindAll = false;
    } else if (argument == "--bind=all") {
      bindAll = true;
    } else if (argument == "--help") {
      std::cout << "nano-server [--port=8080] [--bind=loopback|all] "
                   "[--execution-policy=disabled|interpreter-only|local-trusted]\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << argument << '\n';
      return 2;
    }
  }
  if (productionEnvironment() && !std::getenv("NANOPASS_ALLOWED_ORIGINS")) {
    std::cerr << "NANOPASS_ALLOWED_ORIGINS is required when NANOPASS_ENV=production\n";
    return 2;
  }
  if (port < 1024 || port > 65535) { std::cerr << "port must be in 1024..65535\n"; return 2; }
#ifdef _WIN32
  WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0){std::cerr<<"WSAStartup failed\n";return 1;}
#endif
  Socket server=socket(AF_INET,SOCK_STREAM,0);if(server==kInvalidSocket){std::cerr<<"could not create server socket\n";return 1;}int reuse=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,reinterpret_cast<const char*>(&reuse),sizeof(reuse));sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(bindAll?INADDR_ANY:INADDR_LOOPBACK);address.sin_port=htons(static_cast<uint16_t>(port));if(bind(server,reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||listen(server,16)<0){std::cerr<<"could not bind nano-server\n";closeSocket(server);return 1;}const auto origins=allowedOrigins();std::cout<<"nano-server listening on "<<(bindAll?"all interfaces":"127.0.0.1")<<":"<<port<<" (schema v1, "<<executionPolicyName(policy)<<", "<<origins.size()<<" allowed origins)\n";CompilerService service(limits,policy);while(true){Socket client=accept(server,nullptr,nullptr);if(client==kInvalidSocket)continue;setClientReceiveTimeout(client);handle(client,service,origins);closeSocket(client);} }
