// Port of DigUp ImageSizing.swift at 6faa29e4 (MIT; see NOTICE and DigUp-LICENSE).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>
namespace sizing {
inline std::pair<int,int> target(int h,int w,int budget) {
    if(h<=0||w<=0) throw std::runtime_error("invalid dimensions");
    const double factor=std::sqrt(double(budget)*9*16*16/(double(h)*w));
    int th=int(std::floor(factor*h/48))*48, tw=int(std::floor(factor*w/48))*48;
    if(!th&&!tw) throw std::runtime_error("image too thin to embed");
    if(!th) { th=48; tw=std::min(w/h*48,budget*48); }
    else if(!tw) { tw=48; th=std::min(h/w*48,budget*48); }
    return {th,tw};
}
struct Coeff { int size; std::vector<int> bounds; std::vector<int32_t> weights; };
inline Coeff coefficients(int in,int out) {
    const double scale=double(in)/out, fs=std::max(scale,1.0), support=2*fs;
    Coeff c{int(std::ceil(support))*2+1,std::vector<int>(out*2),{}};
    c.weights.resize(out*c.size); std::vector<double> row(c.size);
    for(int o=0;o<out;o++) {
        double center=(o+.5)*scale; int first=std::max(int(center-support+.5),0);
        int count=std::min(int(center+support+.5),in)-first; double total=0;
        for(int x=0;x<c.size;x++) {
            double a=std::abs((x+first-center+.5)/fs);
            row[x]=x>=count?0:a<1?((1.5*a-2.5)*a*a+1):a<2?((a-5)*a+8)*a*(-.5)+2:0;
            total+=row[x];
        }
        for(int x=0;x<c.size;x++) {
            double v=x<count&&total!=0?row[x]/total:row[x];
            c.weights[o*c.size+x]=int32_t(v*(1<<22)+(v<0?-.5:.5));
        }
        c.bounds[o*2]=first; c.bounds[o*2+1]=count;
    }
    return c;
}
inline uint8_t clip(int32_t v) { return uint8_t(std::clamp(v>>22,0,255)); }
inline std::vector<uint8_t> resize(const uint8_t* src,int w,int h,int ow,int oh) {
    std::vector<uint8_t> tmp;
    if(ow!=w) {
        auto c=coefficients(w,ow); tmp.resize(ow*h*3);
        for(int y=0;y<h;y++) for(int x=0;x<ow;x++) for(int ch=0;ch<3;ch++) {
            uint32_t sum=1<<21;
            for(int k=0;k<c.bounds[x*2+1];k++) sum+=uint32_t(src[(y*w+c.bounds[x*2]+k)*3+ch])*uint32_t(c.weights[x*c.size+k]);
            tmp[(y*ow+x)*3+ch]=clip(int32_t(sum));
        }
        src=tmp.data();
    }
    if(oh==h) return std::vector<uint8_t>(src,src+ow*h*3);
    auto c=coefficients(h,oh); std::vector<uint8_t> out(ow*oh*3);
    for(int y=0;y<oh;y++) for(int i=0;i<ow*3;i++) {
        uint32_t sum=1<<21;
        for(int k=0;k<c.bounds[y*2+1];k++) sum+=uint32_t(src[(c.bounds[y*2]+k)*ow*3+i])*uint32_t(c.weights[y*c.size+k]);
        out[y*ow*3+i]=clip(int32_t(sum));
    }
    return out;
}
}
