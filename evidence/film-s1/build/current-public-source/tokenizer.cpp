#include "llama.h"
#include "nlohmann/json.hpp"
#include <climits>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <QCryptographicHash>
using json=nlohmann::json;
static void log(ggml_log_level l,const char*s,void*){if(l>=GGML_LOG_LEVEL_WARN)std::cerr<<s;}
int main(int argc,char**argv){
 if(argc!=2){std::cerr<<"usage: semanticlens-film-tokenizer TEXT.gguf\n";return 2;}
 try{llama_log_set(log,nullptr);llama_backend_init();auto mp=llama_model_default_params();mp.n_gpu_layers=0;mp.vocab_only=true;
 std::unique_ptr<llama_model,decltype(&llama_model_free)> model(llama_model_load_from_file(argv[1],mp),llama_model_free);if(!model)throw std::runtime_error("vocabulary load failed");auto vocab=llama_model_get_vocab(model.get());std::string line;
 while(std::getline(std::cin,line)){json r;try{if(line.size()>65536)throw std::runtime_error("request byte limit exceeded");auto q=json::parse(line);if(!q.is_object()||!q.contains("id")||!q["id"].is_string()||q["id"].get<std::string>().empty()||q["id"].get<std::string>().size()>128||!q.contains("op")||!q["op"].is_string())throw std::runtime_error("invalid request");r["id"]=q["id"];auto op=q["op"].get<std::string>();for(auto i=q.begin();i!=q.end();++i)if(i.key()!="id"&&i.key()!="op"&&(op!="count"||i.key()!="text"))throw std::runtime_error("unknown field");
 if(op=="info")r["info"]={{"runtime","llama.cpp-b11461+no-logits"},{"tokenizer","semanticlens-film-tokenizer/1"},{"add_special",true},{"parse_special",true},{"vocab_only",true}};
 else if(op=="count"){auto s=q.at("text").get<std::string>();if(s.find('\0')!=std::string::npos)throw std::runtime_error("NUL rejected");std::vector<llama_token> ts(s.size()+8);int n=llama_tokenize(vocab,s.data(),int(s.size()),ts.data(),int(ts.size()),true,true);if(n==INT_MIN)throw std::runtime_error("token overflow");if(n<0){ts.resize(-n);n=llama_tokenize(vocab,s.data(),int(s.size()),ts.data(),int(ts.size()),true,true);}if(n<0)throw std::runtime_error("tokenization failed");r["token_count"]=n;r["utf8_sha256"]=QCryptographicHash::hash(QByteArray::fromStdString(s),QCryptographicHash::Sha256).toHex().toStdString();}
 else throw std::runtime_error("unknown operation");r["ok"]=true;
 }catch(const std::exception&e){r["ok"]=false;r["error"]=e.what();}std::cout<<r.dump()<<std::endl;}
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
