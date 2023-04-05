
#pragma once

#include <mutex>
#include <set>
#include <map>
#include <utility>

namespace utility
{
    namespace shared_mutex
    {
        template <class Key, class Compare = std::less<Key>,
            class Allocator = std::allocator<Key>>
        struct set : public std::set<Key, Compare, Allocator>, public std::shared_mutex
        {
            using std::set<Key, Compare, Allocator>::set;
        };

        template <class _Key, class _Tp, class _Compare = std::less<_Key>,
            class _Allocator = std::allocator<std::pair<const _Key, _Tp> >>
        struct map : public std::map<_Key, _Tp, _Compare, _Allocator>, public std::shared_mutex
        {
            using std::map<_Key, _Tp, _Compare, _Allocator>::map;
        };

        template <typename ContainerType>
        struct container_with_mutex : public ContainerType
        {
            using ContainerType::ContainerType;

            mutable std::shared_mutex lock_;

            operator std::shared_mutex& () const
            {
                return lock_;
            }
        };

    };

    namespace mutex
    {
        template <class Key, class Compare = std::less<Key>,
            class Allocator = std::allocator<Key>>
        struct set : public std::set<Key, Compare, Allocator>, public std::mutex
        {
            using std::set<Key, Compare, Allocator>::set;
        };

        template <class _Key, class _Tp, class _Compare = std::less<_Key>,
            class _Allocator = std::allocator<std::pair<const _Key, _Tp> >>
        struct map : public std::map<_Key, _Tp, _Compare, _Allocator>, public std::mutex
        {
            using std::map<_Key, _Tp, _Compare, _Allocator>::map;
        };

        template <typename ContainerType>
        struct container_with_mutex : public ContainerType
        {
            using ContainerType::ContainerType;
            mutable std::mutex lock_;

            operator std::mutex& () const
            {
                return lock_;
            }
        };
    };

}
