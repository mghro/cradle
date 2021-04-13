#ifndef CRADLE_UTILITIES_FUNCTIONAL_H
#define CRADLE_UTILITIES_FUNCTIONAL_H

#include <map>
#include <vector>

namespace cradle {

// functional map over a vector
template<class Item, class Fn>
auto
map(Fn const& fn, std::vector<Item> const& items)
{
    typedef decltype(fn(Item())) mapped_item_type;
    size_t item_count = items.size();
    std::vector<mapped_item_type> result(item_count);
    for (size_t i = 0; i != item_count; ++i)
        result[i] = fn(items[i]);
    return result;
}

// functional map over a map
template<class Key, class Value, class Fn>
auto
map(Fn const& fn, std::map<Key, Value> const& items)
    -> std::map<Key, decltype(fn(Value()))>
{
    typedef decltype(fn(Value())) mapped_item_type;
    std::map<Key, mapped_item_type> result;
    for (auto const& item : items)
        result[item.first] = fn(item.second);
    return result;
}

// CRADLE_LAMBDIFY(f) produces a lambda that calls f, which is essentially a
// version of f that can be passed as an argument and still allows normal
// overload resolution.
#define CRADLE_LAMBDIFY(f) [](auto&&... args) { return f(args...); }

// CRADLE_AGGREGATOR(f) produces a lambda that assembles its arguments into an
// aggregate expression (i.e., "{args...}") and passes that into f.
// This is useful, for example, when you want to explicitly refer to the
// aggregate constructor of a type as an invocable function.
#define CRADLE_AGGREGATOR(f)                                                  \
    [](auto&&... args) { return f{std::forward<decltype(args)>(args)...}; }

// function_view is the non-owning equivalent of std::function.
template<class Signature>
class function_view;
template<class Return, class... Args>
class function_view<Return(Args...)>
{
 private:
    using signature_type = Return(void*, Args...);

    void* _ptr;
    Return (*_erased_fn)(void*, Args...);

 public:
    template<typename T>
    function_view(T&& x) noexcept : _ptr{(void*) std::addressof(x)}
    {
        _erased_fn = [](void* ptr, Args... xs) -> Return {
            return (*reinterpret_cast<std::add_pointer_t<T>>(ptr))(
                std::forward<Args>(xs)...);
        };
    }

    decltype(auto)
    operator()(Args... xs) const
        noexcept(noexcept(_erased_fn(_ptr, std::forward<Args>(xs)...)))
    {
        return _erased_fn(_ptr, std::forward<Args>(xs)...);
    }
};

} // namespace cradle

#endif
