#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
namespace framevr::placement {
constexpr int NoticeWidth=1024,NoticeRow=192,NoticeRows=4;
// One immutable atlas per OpenXR session, not a GDI/upload operation per frame.
inline std::vector<uint8_t> PaintNotices() {
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=NoticeWidth;info.bmiHeader.biHeight=-NoticeRow*NoticeRows;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dc||!bitmap||!bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return {};}
    auto old=SelectObject(dc,bitmap);SetBkMode(dc,TRANSPARENT);
    HFONT font=CreateFontW(34,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Arial");
    auto oldFont=SelectObject(dc,font);
    RECT all{0,0,NoticeWidth,NoticeRow*NoticeRows};
    auto brush=CreateSolidBrush(RGB(16,22,28));FillRect(dc,&all,brush);DeleteObject(brush);
    SetTextColor(dc,RGB(240,220,170));
    const wchar_t* titles[]={L"Diorama edit mode",L"Editing - press Start to finish",
        L"Diorama placement saved",L"Diorama editing ended"};
    for(int row=0;row<NoticeRows;++row){
        RECT title{16,row*NoticeRow+18,NoticeWidth-16,row*NoticeRow+76};
        DrawTextW(dc,titles[row],-1,&title,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        if(row==0){
            RECT hint{16,row*NoticeRow+82,NoticeWidth-16,(row+1)*NoticeRow-8};
            DrawTextW(dc,L"RT: move   RB: recenter   Start: finish\nD-pad: height / size   LT: 1 cm height steps",-1,&hint,DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
        }
    }
    GdiFlush();const auto* b=static_cast<const uint8_t*>(bits);
    std::vector<uint8_t> rgba(NoticeWidth*NoticeRow*NoticeRows*4);
    for(size_t i=0;i<rgba.size();i+=4){rgba[i]=b[i+2];rgba[i+1]=b[i+1];rgba[i+2]=b[i];rgba[i+3]=235;}
    SelectObject(dc,oldFont);if(font)DeleteObject(font);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    return rgba;
}
}
