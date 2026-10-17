#pragma once
#include <cstdint>
namespace nativebelt {
// Timer thread; retries until the belt's mercenary query is decrypted.
void Install();
// UI thread only, after the caller revalidates the exact captured belt item.
bool Drink(int column,uint64_t session);
// Same contract; the belt's native mercenary query answers yes for this press only.
bool FeedMerc(int column,uint64_t session);
}
