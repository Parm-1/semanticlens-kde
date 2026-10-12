// Linux port of DigUp's embedding pipeline; original MIT notice in NOTICE.
#include "llama.h"
#include "mtmd.h"
#include "mtmd-helper.h"
#include "nlohmann/json.hpp"
#include "image_sizing.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
using json=nlohmann::json;
using Clock=std::chrono::steady_clock;
static double seconds(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
static void logs(ggml_log_level level,const char* text,void*){if(level>=GGML_LOG_LEVEL_WARN) std::cerr<<text;}
class Embedder {
    struct ModelFree{void operator()(llama_model*p){llama_model_free(p);}};
    struct CtxFree{void operator()(llama_context*p){llama_free(p);}};
    struct MediaFree{void operator()(mtmd_context*p){mtmd_free(p);}};
    struct BatchFree{void operator()(llama_batch_ext*p){llama_batch_ext_free(p);}};
    std::unique_ptr<llama_model,ModelFree> model;
    std::unique_ptr<llama_context,CtxFree> ctx;
    std::unique_ptr<mtmd_context,MediaFree> media;
    std::unique_ptr<llama_batch_ext,BatchFree> batch;
    const llama_vocab* vocab=nullptr;
    int dim=0,rowSize=0,maxTokens=2048,position=0,seq=0,total=0;
    void reset(){llama_batch_ext_clear(batch.get());seq=0;total=0;position=0;}
    void place(int idx){llama_pos pos[4]={position,0,0,0};if(idx<0||!llama_batch_ext_set_pos(batch.get(),idx,pos)||!llama_batch_ext_set_output_embd(batch.get(),idx,true)) throw std::runtime_error("batch placement failed");position++;total++;}
    void add(llama_token t){place(llama_batch_ext_add_token(batch.get(),seq-1,t));}
    void begin(){seq++;position=0;}
    std::vector<std::vector<float>> run(){
        int status=llama_process(ctx.get(),LLAMA_PROCESS_TYPE_ENCODE,batch.get());
        if(status!=0){reset();throw std::runtime_error("llama_process failed "+std::to_string(status));}
        std::vector<std::vector<float>> out;
        for(int s=0;s<seq;s++) {
            const float* p=llama_get_embeddings_seq(ctx.get(),s);if(!p) throw std::runtime_error("missing pooled embedding");
            double sum=0;for(int i=0;i<dim;i++) {if(!std::isfinite(p[i]))throw std::runtime_error("nonfinite embedding");sum+=double(p[i])*p[i];}
            if(sum<=0) throw std::runtime_error("zero embedding");
            float scale=float(1/std::sqrt(sum));std::vector<float> v(dim);for(int i=0;i<dim;i++)v[i]=p[i]*scale;out.push_back(std::move(v));
        }
        reset();return out;
    }
public:
    double loadSeconds;
    Embedder(const char* text,const char* projection,int threads,bool textOnly) {
        auto start=Clock::now();llama_log_set(logs,nullptr);mtmd_helper_log_set(logs,nullptr);llama_backend_init();
        auto mp=llama_model_default_params();mp.n_gpu_layers=0;
        model.reset(llama_model_load_from_file(text,mp));if(!model) throw std::runtime_error("text model load failed");
        maxTokens=textOnly?512:2048;
        auto cp=llama_context_default_params();cp.n_ctx=maxTokens;cp.n_batch=maxTokens;cp.n_ubatch=maxTokens;cp.n_seq_max=16;cp.kv_unified=true;cp.embeddings=true;cp.pooling_type=LLAMA_POOLING_TYPE_MEAN;cp.n_threads=threads;cp.n_threads_batch=threads;
        ctx.reset(llama_init_from_model(model.get(),cp));if(!ctx)throw std::runtime_error("context init failed");
        if(!textOnly){auto pp=mtmd_context_params_default();pp.use_gpu=false;pp.n_threads=threads;pp.print_timings=false;pp.warmup=true;pp.image_min_tokens=1;pp.image_max_tokens=1120;media.reset(mtmd_init_from_file(projection,model.get(),pp));if(!media)throw std::runtime_error("media model load failed");}
        batch.reset(llama_batch_ext_init(ctx.get()));if(!batch)throw std::runtime_error("batch init failed");vocab=llama_model_get_vocab(model.get());dim=llama_model_n_embd_out(model.get());rowSize=llama_model_n_embd_inp(model.get());
        for(int i=0;i<2;i++)texts({"task: search result | query: warm up"});
        loadSeconds=seconds(start);
    }
    json info(){return {{"model","google/embeddinggemma-2"},{"model_revision","bfcd298762cc34d0357ece5ebdd31791a3a374d8"},{"runtime","llama.cpp-b11461+no-logits"},{"pipeline","digup-vectors1-linux-cpu-probe1"},{"dimension",dim},{"max_tokens",maxTokens},{"backend","CPU"},{"load_seconds",loadSeconds}};}
    std::vector<std::vector<float>> texts(const std::vector<std::string>& inputs){
        reset();std::vector<std::vector<float>> result;
        for(const auto& text:inputs){
            std::vector<llama_token> t(text.size()+8);int n=llama_tokenize(vocab,text.data(),text.size(),t.data(),t.size(),true,true);
            if(n<0){t.resize(-n);n=llama_tokenize(vocab,text.data(),text.size(),t.data(),t.size(),true,true);}if(n<0)throw std::runtime_error("tokenization failed");t.resize(n);
            if(t.size()>size_t(maxTokens)){t.resize(maxTokens);t.back()=llama_vocab_eos(vocab);}
            if(seq==16||total+int(t.size())>maxTokens){auto r=run();result.insert(result.end(),r.begin(),r.end());}
            begin();for(auto token:t)add(token);
        }
        if(seq){auto r=run();result.insert(result.end(),r.begin(),r.end());}return result;
    }
    json image(const std::string& path,int budget){
        if(!media)throw std::runtime_error("text-only context");
        if(budget!=70&&budget!=140&&budget!=280&&budget!=560&&budget!=1120)throw std::runtime_error("unsupported image budget");
        using Bitmap=std::unique_ptr<mtmd_bitmap,decltype(&mtmd_bitmap_free)>;
        auto wrapper=mtmd_helper_bitmap_init_from_file(media.get(),path.c_str(),false,mtmd_helper_init_opt_default());
        Bitmap bitmap(wrapper.bitmap,mtmd_bitmap_free);
        if(wrapper.video_ctx){mtmd_helper_video_free(wrapper.video_ctx);throw std::runtime_error("video not accepted");}
        if(!bitmap||mtmd_bitmap_is_audio(bitmap.get()))throw std::runtime_error("unreadable image or wrong media type");
        int w=mtmd_bitmap_get_nx(bitmap.get()),h=mtmd_bitmap_get_ny(bitmap.get());auto [th,tw]=sizing::target(h,w,budget);
        auto pixels=sizing::resize(mtmd_bitmap_get_data(bitmap.get()),w,h,tw,th);bitmap.reset(mtmd_bitmap_init(tw,th,pixels.data()));if(!bitmap)throw std::runtime_error("bitmap resize failed");
        using Chunks=std::unique_ptr<mtmd_input_chunks,decltype(&mtmd_input_chunks_free)>;
        Chunks chunks(mtmd_input_chunks_init(),mtmd_input_chunks_free);const mtmd_bitmap* b=bitmap.get();const char* marker=mtmd_default_marker();mtmd_input_text input{marker,strlen(marker),true,true};
        if(mtmd_tokenize(media.get(),chunks.get(),&input,&b,1)!=0)throw std::runtime_error("media tokenization failed");
        reset();begin();size_t rowsCount=0;
        for(size_t i=0;i<mtmd_input_chunks_size(chunks.get());i++){
            auto chunk=mtmd_input_chunks_get(chunks.get(),i);
            if(mtmd_input_chunk_get_type(chunk)==MTMD_INPUT_CHUNK_TYPE_TEXT){size_t n=0;auto ts=mtmd_input_chunk_get_tokens_text(chunk,&n);for(size_t j=0;j<n;j++)add(ts[j]);}
            else{if(mtmd_encode_chunk(media.get(),chunk)!=0)throw std::runtime_error("image encoder failed");auto rows=mtmd_get_output_embd(media.get());if(!rows)throw std::runtime_error("no image rows");size_t n=mtmd_input_chunk_get_n_tokens(chunk);rowsCount+=n;for(size_t j=0;j<n;j++){llama_embd e{rows+j*rowSize,1,size_t(rowSize)};place(llama_batch_ext_add_embd(batch.get(),0,e));}}
        }
        auto vector=run().at(0);return {{"vector",vector},{"source_width",w},{"source_height",h},{"width",tw},{"height",th},{"encoder_rows",rowsCount}};
    }
};
int main(int argc,char**argv){
    if(argc<3){std::cerr<<"usage: embedding-probe TEXT.gguf MMPROJ.gguf [threads=6] [--text-only]\nJSON lines on stdin: op info/texts/query/document/image\n";return 2;}
    try{
        int threads=argc>3?std::stoi(argv[3]):6;bool textOnly=argc>4&&std::string(argv[4])=="--text-only";if(threads<1)throw std::runtime_error("threads must be positive");
        Embedder e(argv[1],argv[2],threads,textOnly);std::string line;
        while(std::getline(std::cin,line)){json result;auto start=Clock::now();try{auto request=json::parse(line);result["id"]=request.value("id",json(nullptr));auto op=request.at("op").get<std::string>();
            if(op=="info")result["info"]=e.info();
            else if(op=="texts")result["vectors"]=e.texts(request.at("texts").get<std::vector<std::string>>());
            else if(op=="query")result["vector"]=e.texts({"task: search result | query: "+request.at("text").get<std::string>()}).at(0);
            else if(op=="document")result["vector"]=e.texts({"title: "+(request.value("title",std::string()).empty()?"none":request.at("title").get<std::string>())+" | text: "+request.at("text").get<std::string>()}).at(0);
            else if(op=="image"){auto im=e.image(request.at("path").get<std::string>(),request.at("budget").get<int>());result.update(im);}
            else throw std::runtime_error("unknown operation");result["ok"]=true;
        }catch(const std::exception& ex){result["ok"]=false;result["error"]=ex.what();}result["seconds"]=seconds(start);std::cout<<result.dump()<<std::endl;}
        return 0;
    }catch(const std::exception& ex){std::cerr<<ex.what()<<"\n";return 1;}
}
