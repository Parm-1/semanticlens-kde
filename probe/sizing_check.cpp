#include "image_sizing.h"
#include "nlohmann/json.hpp"
#include <iostream>
using json=nlohmann::json;
int main(){std::string line;while(std::getline(std::cin,line)){try{auto r=json::parse(line);auto s=r.at("source").get<std::vector<uint8_t>>();int w=r.at("width"),h=r.at("height"),ow=r.at("out_width"),oh=r.at("out_height");if(w<=0||h<=0||ow<=0||oh<=0||s.size()!=size_t(w)*h*3)throw std::runtime_error("invalid size");std::cout<<json(sizing::resize(s.data(),w,h,ow,oh)).dump()<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}}
