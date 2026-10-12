// Same installed GIO recommendation/default APIs as the portal's primary source.
#include <gio/gdesktopappinfo.h>
#include <cstdio>
#include <string>
static std::string esc(const char*s){std::string r="\"";if(s)for(auto p=s;*p;++p){if(*p=='"'||*p=='\\')r+='\\';r+=*p;}return r+'"';}
int main(){auto d=g_app_info_get_default_for_type("text/plain",false);auto recommended=g_app_info_get_recommended_for_type("text/plain");bool fallback=!recommended;auto used=recommended?recommended:g_app_info_get_all_for_type("text/plain");printf("{\"default\":%s,\"fallback_to_all\":%s,\"choices\":[",d?esc(g_app_info_get_id(d)).c_str():"null",fallback?"true":"false");bool first=true;for(auto l=used;l;l=l->next){auto a=G_APP_INFO(l->data);printf("%s{\"id\":%s,\"executable\":%s,\"commandline\":%s,\"should_show\":%s}",first?"":",",esc(g_app_info_get_id(a)).c_str(),esc(g_app_info_get_executable(a)).c_str(),esc(g_app_info_get_commandline(a)).c_str(),g_app_info_should_show(a)?"true":"false");first=false;}puts("]}");if(d)g_object_unref(d);g_list_free_full(used,g_object_unref);}
