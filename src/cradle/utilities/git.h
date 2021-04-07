#ifndef CRADLE_UTILITIES_GIT_H
#define CRADLE_UTILITIES_GIT_H

namespace cradle {

// This captures information about the state of the Git repository for the
// source code itself.
struct repository_info
{
    // the abbreviated object name of the current commit
    string commit_object_name;

    // Does the repository have local modifications?
    bool dirty;

    // the closest tag in the history of the repository
    string tag;

    // how many commits there have been since the tag
    unsigned commits_since_tag;
};

// Does the given repository correspond to a tagged version of the code?
inline bool
is_tagged_version(repository_info const& info)
{
    return info.commits_since_tag == 0 && !info.dirty;
}

} // namespace cradle

#endif
