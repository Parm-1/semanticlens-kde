// Film-only successor. The immutable legacy embedding-probe remains separate.
#include "llama.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
using json=nlohmann::json;
namespace {
constexpr const char * reference="fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0";
struct ProducerFailure:std::runtime_error{using std::runtime_error::runtime_error;};
struct ModelFree {void operator()(llama_model *p){llama_model_free(p);}};
struct ContextFree {void operator()(llama_context *p){llama_free(p);}};
struct BatchFree {void operator()(llama_batch_ext *p){llama_batch_ext_free(p);}};
void quiet(ggml_log_level,const char*,void*){}
bool boundedLine(std::string &line,bool &over){line.clear();over=false;char c;bool any=false;while(std::cin.get(c)){any=true;if(c=='\n')return true;if(line.size()<65536)line.push_back(c);else over=true;}return any;}
void fields(const json &r,std::initializer_list<const char*> extra){
 if(!r.is_object())throw std::runtime_error("request must be object");
 for(auto it=r.begin();it!=r.end();++it){bool known=it.key()=="id"||it.key()=="op";for(auto k:extra)known|=it.key()==k;if(!known)throw std::runtime_error("unknown field "+it.key());}
}
std::string identifier(const json &r,const char *key){
 if(!r.contains(key)||!r[key].is_string())throw std::runtime_error(std::string("invalid ")+key);
 auto s=r[key].get<std::string>();if(s.empty()||s.size()>128)throw std::runtime_error(std::string("invalid ")+key);
 for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==':'||c=='.'||c=='_'||c=='-'))throw std::runtime_error(std::string("invalid ")+key);return s;
}
std::string text(const json &r){if(!r.contains("text")||!r["text"].is_string())throw std::runtime_error("invalid text");auto s=r["text"].get<std::string>();if(s.empty()||s.size()>65500)throw std::runtime_error("invalid text bytes");return s;}
std::string ticket(){std::random_device random;std::ostringstream s;s<<"film-"<<std::hex;for(int i=0;i<4;i++)s<<random()<<'-';return s.str();}
class Context {
 const llama_vocab *vocab;
 std::unique_ptr<llama_context,ContextFree> ctx;
 std::unique_ptr<llama_batch_ext,BatchFree> batch;
 std::atomic<bool> *cancellation;
 int dimension;
 static bool abort(void *p){return static_cast<std::atomic<bool>*>(p)->load();}
public:
 Context(llama_model *model,int threads,std::atomic<bool> *cancel=nullptr):vocab(llama_model_get_vocab(model)),cancellation(cancel),dimension(llama_model_n_embd_out(model)){
  auto cp=llama_context_default_params();cp.n_ctx=2048;cp.n_batch=2048;cp.n_ubatch=2048;cp.n_seq_max=16;cp.kv_unified=true;cp.embeddings=true;cp.pooling_type=LLAMA_POOLING_TYPE_MEAN;cp.n_threads=threads;cp.n_threads_batch=threads;
  if(cancel){cp.abort_callback=abort;cp.abort_callback_data=cancel;}
  ctx.reset(llama_init_from_model(model,cp));if(!ctx||dimension!=768)throw std::runtime_error("film context identity failure");batch.reset(llama_batch_ext_init(ctx.get()));if(!batch)throw std::runtime_error("film batch allocation failure");
 }
 void check(const std::string &input){
  if(input.find('\0')!=std::string::npos)throw std::runtime_error("NUL rejected");
  int n=llama_tokenize(vocab,input.data(),input.size(),nullptr,0,true,true);if(n<0)n=-n;if(n>2048)throw std::runtime_error("token_limit: formatted input exceeds2048");
 }
 std::vector<float> run(const std::string &input,bool legacy){
  llama_batch_ext_clear(batch.get());
  try{
   std::vector<llama_token> tokens(input.size()+8);int n=llama_tokenize(vocab,input.data(),input.size(),tokens.data(),tokens.size(),true,true);
   if(n<0){tokens.resize(-n);n=llama_tokenize(vocab,input.data(),input.size(),tokens.data(),tokens.size(),true,true);}if(n<0)throw std::runtime_error("tokenization failed");tokens.resize(n);
   if(tokens.size()>2048){if(!legacy)throw std::runtime_error("token_limit: formatted input exceeds2048");tokens.resize(2048);tokens.back()=llama_vocab_eos(vocab);}
   if(!legacy&&input.find('\0')!=std::string::npos)throw std::runtime_error("NUL rejected");
   int position=0;for(auto token:tokens){int idx=llama_batch_ext_add_token(batch.get(),0,token);llama_pos pos[4]={position,0,0,0};if(idx<0||!llama_batch_ext_set_pos(batch.get(),idx,pos)||!llama_batch_ext_set_output_embd(batch.get(),idx,true))throw std::runtime_error("batch placement failed");position++;}
   int status=llama_process(ctx.get(),LLAMA_PROCESS_TYPE_ENCODE,batch.get());if(status!=0)throw std::runtime_error(cancellation&&cancellation->load()?"canceled":"encode failed "+std::to_string(status));
   const float *p=llama_get_embeddings_seq(ctx.get(),0);if(!p)throw std::runtime_error("missing pooled embedding");double sum=0;for(int i=0;i<dimension;i++){if(!std::isfinite(p[i]))throw std::runtime_error("nonfinite embedding");sum+=double(p[i])*p[i];}if(sum<=0)throw std::runtime_error("zero embedding");
   float scale=float(1/std::sqrt(sum));std::vector<float> result(dimension);for(int i=0;i<dimension;i++)result[i]=p[i]*scale;llama_batch_ext_clear(batch.get());return result;
  }catch(...){llama_batch_ext_clear(batch.get());throw;}
 }
};
class Worker {
 std::unique_ptr<llama_model,ModelFree> model;
 std::atomic<bool> cancellation{false};
 std::unique_ptr<Context> foreground,background;
 std::mutex slotMutex,serialCompute;
 std::string currentTicket,error;
 std::vector<float> vector;
 bool running=false,complete=false,polled=false;
 int total;
 std::thread thread;
 // Keep the actual post-encode publication phase independently addressable for
 // owned phase controls. Both final disposition and cancel use this same mutex.
 [[gnu::noinline]] void publish(std::vector<float> output,std::string failure){
  std::lock_guard<std::mutex> lock(slotMutex);
  if(cancellation.load()){vector.clear();error="canceled";}
  else{vector=std::move(output);error=std::move(failure);}
  running=false;complete=true;
 }
public:
 Worker(const char *path,int threads):total(threads){
  if(threads<1||threads>16)throw std::runtime_error("total CPU budget must be1..16");
  // Global runtime setup and both context construction/warmup are sequential.
  llama_log_set(quiet,nullptr);llama_backend_init();auto mp=llama_model_default_params();mp.n_gpu_layers=0;model.reset(llama_model_load_from_file(path,mp));if(!model)throw std::runtime_error("text model load failed");
  foreground=std::make_unique<Context>(model.get(),1);background=std::make_unique<Context>(model.get(),std::max(1,total-1),&cancellation);
  for(int i=0;i<2;i++){foreground->run("task: search result | query: warm up",false);background->run("task: search result | query: warm up",false);}
 }
 ~Worker(){cancellation=true;if(thread.joinable())thread.join();}
 json info(){return {{"protocol","semanticlens-film-worker/1"},{"model","google/embeddinggemma-2"},{"model_revision","bfcd298762cc34d0357ece5ebdd31791a3a374d8"},{"runtime","llama.cpp-b11461+no-logits"},{"dimension",768},{"max_tokens",2048},{"context_tokens",2048},{"batch_tokens",2048},{"ubatch_tokens",2048},{"sequences",16},{"kv_unified",true},{"pooling","mean"},{"backend","CPU"},{"threads_total",total},{"foreground_threads",1},{"background_threads",std::max(1,total-1)},{"allocation",total==1?"serial/1":"reserved-foreground/1"},{"reference_producer_sha256",reference},{"legacy_preprocessing","legacy-query-clip2048-eos/1"},{"film_preprocessing","film-query-strict2048/1"}};}
 json handle(const json &r){
  identifier(r,"id");auto op=identifier(r,"op");
  if(op=="info"){fields(r,{});return {{"info",info()}};}
  if(op=="film_query"||op=="legacy_query"){
   fields(r,{"text"});auto input=text(r);bool legacy=op=="legacy_query";if(!legacy)foreground->check(input);if(legacy)input="task: search result | query: "+input;
   std::unique_lock<std::mutex> compute(serialCompute,std::defer_lock);if(total==1)compute.lock();try{return {{"vector",foreground->run(input,legacy)}};}catch(const std::exception &e){throw ProducerFailure(e.what());}
  }
  if(op=="film_begin"){
   fields(r,{"text"});auto input=text(r);if(input.find('\0')!=std::string::npos)throw std::runtime_error("NUL rejected");
   {std::lock_guard<std::mutex> lock(slotMutex);if(running||(complete&&!polled))throw std::runtime_error("background ticket active or completed-unread");}
   background->check(input);if(thread.joinable())thread.join();auto id=ticket();
   {std::lock_guard<std::mutex> lock(slotMutex);currentTicket=id;vector.clear();error.clear();complete=false;polled=false;running=true;cancellation=false;}
   thread=std::thread([this,input]{std::vector<float> output;std::string failure;try{std::unique_lock<std::mutex> compute(serialCompute,std::defer_lock);if(total==1)compute.lock();output=background->run(input,false);}catch(const std::exception &e){failure=e.what();}publish(std::move(output),std::move(failure));});return {{"accepted",true},{"ticket",id}};
  }
  if(op=="film_poll"||op=="film_cancel"){
   fields(r,{"ticket"});auto id=identifier(r,"ticket");std::lock_guard<std::mutex> lock(slotMutex);if(id!=currentTicket||id.empty())throw std::runtime_error("unknown ticket");
   if(op=="film_cancel"){
    // Completed results remain in the bounded slot until the next begin. A
    // first cancel also revokes their future polls; already delivered replies
    // cannot be revoked. Repeated cancellation reports accepted:false.
    bool accepted=!cancellation.load();
    if(accepted){cancellation=true;vector.clear();error="canceled";}
    return {{"accepted",accepted}};
   }
   if(!complete)return {{"ready",false}};
   polled=true;
   if(!error.empty())return {{"ready",true},{"error",error},{"error_code",error=="canceled"?"canceled":"producer_failure"}};
   return {{"ready",true},{"vector",vector}};
  }
  throw std::runtime_error("unknown operation");
 }
};
}
int main(int argc,char **argv){
 if(argc!=3){std::cerr<<"usage: semanticlens-film-worker TEXT.gguf TOTAL_CPU_THREADS\n";return 2;}
 try{size_t consumed=0;int total=std::stoi(argv[2],&consumed);if(consumed!=std::string(argv[2]).size())throw std::runtime_error("invalid CPU budget");Worker worker(argv[1],total);std::string line;bool over=false;while(boundedLine(line,over)){json result={{"id",nullptr}};try{if(over)throw std::runtime_error("request byte limit");auto r=json::parse(line);if(r.is_object()&&r.contains("id"))result["id"]=r["id"];auto payload=worker.handle(r);result.update(payload);result["ok"]=true;}catch(const ProducerFailure &e){result["ok"]=false;result["error"]=e.what();result["error_code"]="producer_failure";}catch(const std::exception &e){result["ok"]=false;result["error"]=e.what();result["error_code"]=std::string(e.what()).rfind("token_limit:",0)==0?"token_limit":"invalid_request";}std::cout<<result.dump()<<std::endl;}return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
