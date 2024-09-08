#pragma once

#include <kernel/base.h>


#define INTRUSIVE_LINKED_LIST_HEADER(Type) \
    Type *prev; \
    Type *next;


template<typename T>
struct IntrusiveLinkedList {
    T *head;
    T *tail;

    template<typename Func>
    void foreach(Func func)
    {
        auto *current = head;
        while (current) {
            auto *next = current->next;
            func(current);
            current = next;
        }
    }

    template<typename Func>
    void foreach_reverse(Func func)
    {
        auto *current = tail;
        while (current) {
            auto *prev = current->prev;
            func(current);
            current = prev;
        }
    }

    template<typename Filter>
    T* find(Filter filter)
    {
        auto *current = head;
        while (current) {
            if (filter(current))
                return current;
            current = current->next;
        }

        return nullptr;
    }

    void remove(T *node)
    {
        if (node->prev == nullptr) {
            head = node->next;
            if (head == nullptr)
                tail = nullptr;
            else
                head->prev = nullptr;
            return;
        } else if (node->next == nullptr) {
            tail = node->prev;
            tail->next = nullptr;
            return;
        } else {
            node->prev->next = node->next;
            node->next->prev = node->prev;
        }
    }

    void add(T *node)
    {
        node->prev = nullptr;
        node->next = head;
        if (head)
            head->prev = node;
        head = node;
        if (tail == nullptr)
            tail = head;
    }

    void append(T *node)
    {
        if (head == nullptr) {
            return add(node);
        }
        
        node->next = nullptr;
        node->prev = tail;
        tail->next = node;
        tail = node;
    }

    T *pop()
    {
        T *el = first();
        if (el)
            remove(el);
        return el;
    }

    size_t length() const
    {
        size_t count = 0;
        foreach([&](auto*) { count++; });
        return count;
    }

    bool is_empty() const { return head != nullptr; }

    T* first() const { return head; }
    T* last() const { return tail; }

    void free() const
    {
        while (!is_empty())
            remove(head);
    }
};
