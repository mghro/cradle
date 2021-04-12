#ifndef CRADLE_BACKGROUND_ENCODED_PROGRESS_H
#define CRADLE_BACKGROUND_ENCODED_PROGRESS_H

#include <cradle/core.h>

namespace cradle {

// This stores an optional progress value encoded as an integer so that it can
// be stored atomically.
struct encoded_optional_progress
{
    // Progress is encoded as an integer ranging from 0 to
    // `encoded_progress_max_value`.
    //
    // A negative value indicates that progress hasn't been reported.
    //
    int value = -1;
};
int constexpr encoded_progress_max_value = 1000;
inline encoded_optional_progress
encode_progress(float progress)
{
    return encoded_optional_progress{int(progress * 1000.f)};
}
inline void
reset(encoded_optional_progress& progress)
{
    progress.value = -1;
}
inline optional<float>
decode_progress(encoded_optional_progress progress)
{
    return progress.value < 0 ? none : some(float(progress.value) / 1000.f);
}

} // namespace cradle

#endif
