#include <cradle/core/diff.hpp>
#include <cradle/core/type_interfaces.hpp>

namespace cradle {

static value_diff_item
make_insert_item(value_diff_path const& path, dynamic const& new_value)
{
    value_diff_item item;
    item.path = path;
    item.op = value_diff_op::INSERT;
    item.a = none;
    item.b = some(new_value);
    return item;
}

static value_diff_item
make_update_item(
    value_diff_path const& path,
    dynamic const& old_value,
    dynamic const& new_value)
{
    value_diff_item item;
    item.path = path;
    item.op = value_diff_op::UPDATE;
    item.a = some(old_value);
    item.b = some(new_value);
    return item;
}

static value_diff_item
make_delete_item(value_diff_path const& path, dynamic const& old_value)
{
    value_diff_item item;
    item.path = path;
    item.op = value_diff_op::DELETE;
    item.a = some(old_value);
    item.b = none;
    return item;
}

static value_diff_path
extend_path(value_diff_path const& path, dynamic const& addition)
{
    value_diff_path extended = path;
    extended.push_back(addition);
    return extended;
}

static void
compute_value_diff(
    value_diff& diff,
    value_diff_path const& path,
    dynamic const& a,
    dynamic const& b);

static void
compute_map_diff(
    value_diff& diff,
    value_diff_path const& path,
    dynamic_map const& a,
    dynamic_map const& b)
{
    // The simplest possible diff is to just treat the whole map as being
    // updated.
    value_diff simple_diff;
    simple_diff.push_back(make_update_item(path, dynamic(a), dynamic(b)));

    // Try to generated a more compact diff by diffing individual fields.
    value_diff compressed_diff;
    auto a_i = a.begin(), a_end = a.end();
    auto b_i = b.begin(), b_end = b.end();
    while (1)
    {
        if (a_i != a_end)
        {
            if (b_i != b_end)
            {
                if (a_i->first == b_i->first)
                {
                    compute_value_diff(
                        compressed_diff,
                        extend_path(path, a_i->first),
                        a_i->second,
                        b_i->second);
                    ++a_i;
                    ++b_i;
                }
                else if (a_i->first < b_i->first)
                {
                    compressed_diff.push_back(make_delete_item(
                        extend_path(path, a_i->first), a_i->second));
                    ++a_i;
                }
                else
                {
                    compressed_diff.push_back(make_insert_item(
                        extend_path(path, b_i->first), b_i->second));
                    ++b_i;
                }
            }
            else
            {
                compressed_diff.push_back(make_delete_item(
                    extend_path(path, a_i->first), a_i->second));
                ++a_i;
            }
        }
        else
        {
            if (b_i != b_end)
            {
                compressed_diff.push_back(make_insert_item(
                    extend_path(path, b_i->first), b_i->second));
                ++b_i;
            }
            else
                break;
        }
    }

    // Use whichever diff is smaller.
    value_diff* diff_to_use
        = deep_sizeof(compressed_diff) < deep_sizeof(simple_diff)
              ? &compressed_diff
              : &simple_diff;
    std::move(
        diff_to_use->begin(), diff_to_use->end(), std::back_inserter(diff));
}

struct insertion_description
{
    // index at which items were inserted
    size_t index;
    // number of items inserted
    size_t count;

    insertion_description()
    {
    }
    insertion_description(size_t index, size_t count)
        : index(index), count(count)
    {
    }
};

static optional<insertion_description>
detect_insertion(dynamic_array const& a, dynamic_array const& b)
{
    size_t a_size = a.size();
    size_t b_size = b.size();
    assert(b_size > a_size);

    // Look for a mistmatch between the items in a and b.
    optional<size_t> removal_point;
    for (size_t i = 0; i != a_size; ++i)
    {
        if (a[i] != b[i])
        {
            // Scan through the remaining items in b looking for one that
            // matches a[i].
            size_t offset = 1;
            for (; i + offset != b_size; ++offset)
            {
                if (a[i] == b[i + offset])
                    goto found_match;
            }
            // None was found, so this wasn't an insertion.
            return none;
        found_match:
            // Now check that the remaining items in b match the remaining
            // items in a.
            for (size_t j = i; j != a_size; ++j)
            {
                // If we find another mismatch, give up.
                if (a[j] != b[j + offset])
                    return none;
            }
            return insertion_description(i, offset);
        }
    }

    // We didn't find a mismatch, so the last items were the ones inserted.
    return insertion_description(a_size, b_size - a_size);
}

static void
compute_array_diff(
    value_diff& diff,
    value_diff_path const& path,
    dynamic_array const& a,
    dynamic_array const& b)
{
    // The simplest possible diff is to just treat the whole array as being
    // updated.
    value_diff simple_diff;
    simple_diff.push_back(make_update_item(path, dynamic(a), dynamic(b)));

    // We also detect three common cases for compression:
    // * one or more items were inserted somewhere in the array
    // * one or more items were removed from the array
    // * the array didn't change size but items may have been updated
    // If any of these cases are found, we compute the corresponding diff.

    value_diff compressed_diff;

    size_t a_size = a.size();
    size_t b_size = b.size();

    // Check if items were inserted.
    if (a_size < b_size)
    {
        auto insertion = detect_insertion(a, b);
        if (insertion)
        {
            for (size_t i = 0; i != insertion->count; ++i)
            {
                compressed_diff.push_back(make_insert_item(
                    extend_path(path, to_dynamic(insertion->index + i)),
                    b[insertion->index + i]));
            }
        }
    }
    // Check if an item was removed.
    else if (a_size > b_size)
    {
        auto removal = detect_insertion(b, a);
        if (removal)
        {
            for (size_t i = removal->count; i != 0; --i)
            {
                compressed_diff.push_back(make_delete_item(
                    extend_path(path, to_dynamic(removal->index + i - 1)),
                    a[removal->index + i - 1]));
            }
        }
    }
    // If the arrays are the same size, just diff each item.
    else
    {
        assert(a_size == b_size);
        for (size_t i = 0; i != a_size; ++i)
        {
            compute_value_diff(
                compressed_diff, extend_path(path, to_dynamic(i)), a[i], b[i]);
        }
    }

    // Use whichever diff is smaller.
    value_diff* diff_to_use = &simple_diff;
    if (!compressed_diff.empty()
        && deep_sizeof(compressed_diff) < deep_sizeof(simple_diff))
    {
        diff_to_use = &compressed_diff;
    }

    std::move(
        diff_to_use->begin(), diff_to_use->end(), std::back_inserter(diff));
}

static void
compute_value_diff(
    value_diff& diff,
    value_diff_path const& path,
    dynamic const& a,
    dynamic const& b)
{
    if (a != b)
    {
        // If a and b are both records, do a field-by-field diff.
        if (a.type() == value_type::MAP && b.type() == value_type::MAP)
        {
            compute_map_diff(
                diff, path, cast<dynamic_map>(a), cast<dynamic_map>(b));
        }
        // If a and b are both arrays, do an item-by-item diff.
        else if (
            a.type() == value_type::ARRAY && b.type() == value_type::ARRAY)
        {
            compute_array_diff(
                diff, path, cast<dynamic_array>(a), cast<dynamic_array>(b));
        }
        // Otherwise, there's no way to compress the change, so just add an
        // update to the new value.
        else
        {
            diff.push_back(make_update_item(path, a, b));
        }
    }
}

value_diff
compute_value_diff(dynamic const& a, dynamic const& b)
{
    value_diff diff;
    compute_value_diff(diff, value_diff_path(), a, b);
    return diff;
}

static dynamic
apply_value_diff_item(
    dynamic const& initial,
    value_diff_path const& path,
    size_t path_index,
    value_diff_op op,
    dynamic const& new_value)
{
    size_t path_size = path.size();

    // If the path is empty, then we must be replacing the whole value.
    if (path_index == path_size)
        return new_value;

    // Otherwise, check out the next path element.
    auto const& path_element = path[path_index];
    switch (path_element.type())
    {
        case value_type::STRING: {
            if (initial.type() != value_type::MAP)
                throw invalid_diff_path();
            dynamic_map map = cast<dynamic_map>(initial);
            // If this is the last element, we need to actually act on it.
            if (path_index + 1 == path_size)
            {
                switch (op)
                {
                    case value_diff_op::INSERT:
                    case value_diff_op::UPDATE:
                        map[path_element] = new_value;
                        break;
                    case value_diff_op::DELETE:
                        map.erase(path_element);
                        break;
                }
            }
            // Otherwise, just continue on down the path.
            else
            {
                auto field = map.find(path_element);
                if (field == map.end())
                    throw invalid_diff_path();
                field->second = apply_value_diff_item(
                    field->second, path, path_index + 1, op, new_value);
            }
            return dynamic(map);
        }
        case value_type::INTEGER: {
            if (initial.type() != value_type::ARRAY)
                throw invalid_diff_path();
            dynamic_array array = cast<dynamic_array>(initial);
            size_t index;
            from_dynamic(&index, path_element);
            // If this is the last element, we need to actually act on it.
            if (path_index + 1 == path_size)
            {
                switch (op)
                {
                    case value_diff_op::INSERT:
                        if (index > array.size())
                            throw invalid_diff_path();
                        array.insert(array.begin() + index, new_value);
                        break;
                    case value_diff_op::UPDATE:
                        if (index >= array.size())
                            throw invalid_diff_path();
                        array[index] = new_value;
                        break;
                    case value_diff_op::DELETE:
                        array.erase(array.begin() + index);
                        break;
                }
            }
            // Otherwise, just continue on down the path.
            else
            {
                if (index >= array.size())
                    throw invalid_diff_path();
                array[index] = apply_value_diff_item(
                    array[index], path, path_index + 1, op, new_value);
            }
            return dynamic(array);
        }
        default:
            throw invalid_diff_path();
    }
}

dynamic
apply_value_diff(dynamic const& v, value_diff const& diff)
{
    // TODO: Check that the original value is consistent with the 'a'-side of
    // the diff.
    dynamic patched = v;
    for (auto const& item : diff)
    {
        patched = apply_value_diff_item(
            patched, item.path, 0, item.op, item.b ? *item.b : dynamic());
    }
    return patched;
}

} // namespace cradle
