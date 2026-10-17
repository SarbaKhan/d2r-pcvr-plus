#include "flat_controls.h"
#include <cassert>
int main() {
    using namespace pcvrplus;
    constexpr auto wasd=PackKeys('W','A','S','D'), zqsd=PackKeys('Z','Q','S','D');
    static_assert(Key(wasd,0)=='W' && Key(wasd,1)=='A' && Key(wasd,2)=='S' && Key(wasd,3)=='D');
    static_assert(Key(zqsd,0)=='Z' && Key(zqsd,1)=='Q');
    assert(UniqueKeys(wasd) && UniqueKeys(zqsd));
    assert(!UniqueKeys(PackKeys('W','W','S','D')));
    assert(LetterKey(L'z',0,'W')=='Z');
    assert(LetterKey(L'Z',L'Q','W')=='W');
    assert(LetterKey(L'1',0,'W')=='W');
    assert(LetterKey(0,0,'W')=='W');
}
