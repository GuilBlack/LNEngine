#pragma once
#include "../BOOST/include/Unordered/boost_unordered.hpp"

namespace lne
{
template <class Key, class T, class Hash = boost::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<std::pair<const Key, T>>>
using FlatHashMap = boost::unordered::unordered_flat_map<Key, T, Hash, KeyEqual, Allocator>;

template <class Key, class Hash = boost::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<Key> >
using FlatHashSet = boost::unordered::unordered_flat_set<Key, Hash, KeyEqual, Allocator>;
}
