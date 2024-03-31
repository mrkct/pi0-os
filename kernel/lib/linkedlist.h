#pragma once

#include <kernel/memory/kheap.h>
#include <kernel/error.h>


template<typename T>
struct ListNode {
    T value;
    ListNode<T> *prev, *next;
};

template<typename T>
struct LinkedList {
    ListNode<T> *head;
    ListNode<T> *tail;

    template<typename Func>
    void foreach(Func func)
    {
        ListNode<T> *current = head;
        while (current) {
            auto *next = current->next;
            func(current);
            current = next;
        }
    }

    template<typename Func>
    void foreach_reverse(Func func)
    {
        ListNode<T> *current = tail;
        while (current) {
            auto *prev = current->prev;
            func(current);
            current = prev;
        }
    }

    template<typename Filter>
    ListNode<T>* find(Filter filter)
    {
        ListNode<T> *current = head;
        while (current) {
            if (filter(current))
                return current;
            current = current->next;
        }

        return nullptr;
    }

    void remove(ListNode<T> *node)
    {
        if (node->prev == nullptr) {
            head = node->next;
            if (head == nullptr)
                tail = nullptr;
            return;
        } else if (node->next == nullptr) {
            tail = node->prev;
            return;
        } else {
            node->prev->next = node->next;
        }
    }

    kernel::Error add(T const& value)
    {
        ListNode<T> *node;
        TRY(kernel::kmalloc(sizeof(*node), node));
        *node = {
            .value = value,
            .prev = nullptr,
            .next = head
        };
        add(node);

        return kernel::Success;
    }

    void add(ListNode<T> *node)
    {
        node->prev = nullptr;
        node->next = head;
        head = node;
        if (tail == nullptr)
            tail = head;
    }

    size_t length() const
    {
        size_t count = 0;
        foreach([&](auto*) { count++; });
        return count;
    }

    bool is_empty() const { return head != nullptr; }

    ListNode<T>* first() const { return head; }
    ListNode<T>* last() const { return tail; }

    void free() const
    {
        while (!is_empty())
            remove(head);
    }
};