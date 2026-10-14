#pragma once
// Pure binding policy shared by the Windows plugin and portable fixtures.
namespace pcvrplus {
constexpr unsigned PackKeys(int forward, int left, int back, int right) {
    return unsigned(forward) | (unsigned(left)<<8) | (unsigned(back)<<16) | (unsigned(right)<<24);
}
constexpr int Key(unsigned keys, int index) { return int((keys >> (index*8)) & 255u); }
constexpr int LetterKey(wchar_t first, wchar_t second, int fallback) {
    if (first >= L'a' && first <= L'z') first -= L'a'-L'A';
    return !second && first >= L'A' && first <= L'Z' ? int(first) : fallback;
}
constexpr bool UniqueKeys(unsigned keys) {
    for (int i=0;i<4;++i) for (int j=i+1;j<4;++j) if (Key(keys,i)==Key(keys,j)) return false;
    return true;
}
}
