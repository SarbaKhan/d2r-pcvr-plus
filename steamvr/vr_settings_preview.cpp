// Synthetic UI preview; does not load the game or its configuration.
#include "vr_settings_ui.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int main(int argc,char** argv){
    using namespace framevr::settings;
    Snapshot s;s.ready=true;s.values[Motion]=1;s.values[Snap]=30;s.values[Foveation]=2;
    Model m;m.Open(s);const int page=argc>2?std::atoi(argv[2]):0;
    m.Click({Kind::Tab,page});
    if(argc>3&&std::strcmp(argv[3],"--dropdown")==0)m.Click({Kind::ControllerMenu});
    framevr::calibration::Flow body;body.Begin();body.phase=framevr::calibration::Phase::Hold;body.hint=framevr::calibration::Hint::Pose;body.progress=.55f;
    const auto pixels=Paint(m,{},page==int(Page::Video)?"VR source: 2560 x 2560 per eye":"Foveated shading available. Eye-tracking samples are active.",page==TabCount?&body:nullptr,
        framevr::controllers::Labels{{framevr::controllers::Layout::Touch,framevr::controllers::Layout::Touch}});
    if(pixels.size()!=Width*Height*4)return 1;
    FILE* file=std::fopen(argc>1?argv[1]:"vr-settings-preview.ppm","wb");if(!file)return 2;
    const char* name=argc>1?argv[1]:"";
    if(std::strstr(name,".bmp")){
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof header+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+uint32_t(pixels.size());
        BITMAPINFOHEADER info{};info.biSize=sizeof info;info.biWidth=Width;info.biHeight=-Height;info.biPlanes=1;info.biBitCount=32;
        if(std::fwrite(&header,1,sizeof header,file)!=sizeof header||std::fwrite(&info,1,sizeof info,file)!=sizeof info){std::fclose(file);return 3;}
        auto bgra=pixels;for(size_t i=0;i<bgra.size();i+=4)std::swap(bgra[i],bgra[i+2]);
        if(std::fwrite(bgra.data(),1,bgra.size(),file)!=bgra.size()){std::fclose(file);return 3;}
    }else{
        std::fprintf(file,"P6\n%d %d\n255\n",Width,Height);
        for(size_t i=0;i<pixels.size();i+=4)if(std::fwrite(pixels.data()+i,1,3,file)!=3){std::fclose(file);return 3;}
    }
    return std::fclose(file)==0?0:4;
}
