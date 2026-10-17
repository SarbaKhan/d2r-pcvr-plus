#include "../vr/pcvr_mod_layout.h"
#include <cassert>
#include <cstdio>
using namespace pcvrlayout;
int main(){
    const std::wstring game=L"D:\\Games\\Diablo II Resurrected\\";
    const std::wstring mod=game+L"mods\\D2R-PCVR\\";
    const std::wstring modPlugins=mod+L"d2rloader\\plugins\\";
    const std::wstring globalPlugins=game+L"d2rloader\\plugins\\";
    // The mod's plugin and the global one find the same game folder.
    assert(GameOfPluginDir(modPlugins)==game);
    assert(GameOfPluginDir(globalPlugins)==game);
    assert(GameOfPluginDir(game+L"MODS\\D2R-PCVR\\D2RLoader\\Plugins\\")==game);
    assert(GameOfPluginDir(game+L"bin\\")==L"");
    assert(ModRootOfPluginDir(modPlugins)==mod);
    assert(ModRootOfPluginDir(globalPlugins)==L"");
    // A folder merely ending in "mods" is not the mods folder.
    assert(ModRootOfPluginDir(L"D:\\oldmods\\x\\d2rloader\\plugins\\")==L"");
    // ReShade: the mod's own when it is there, else the game folder (an older install).
    assert(ReShadeDir(game,mod,true)==mod+L"reshade\\");
    assert(ReShadeDir(game,mod,false)==mod+L"reshade\\");
    assert(ReShadeDir(game,L"",true)==game);
    const auto candidates = SettingsCandidates(game);
    assert(candidates.front() == modPlugins + L"d2r_pcvr.ini");
    assert(SettingsCandidates(modPlugins)[1] == modPlugins + L"d2r_pcvr.ini");
    assert(SettingsCandidates(mod)[2] == modPlugins + L"d2r_pcvr.ini");
    // The other VR mod is looked for where its plugin would load into this game: this
    // plugin's folder and the global plugins folder, never another mod's folder.
    auto dirs=SameGamePluginDirs(modPlugins);
    assert(dirs.size()==2&&dirs[0]==modPlugins&&dirs[1]==globalPlugins);
    for(const auto& d:dirs) assert(d.find(L"D2R-VR")==std::wstring::npos);
    dirs=SameGamePluginDirs(globalPlugins);
    assert(dirs.size()==1&&dirs[0]==globalPlugins);
    std::puts("mod layout ok");
}
