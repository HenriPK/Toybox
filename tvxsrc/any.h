#include "tvxutil.h"

using namespace std;

template<class T>
using StorageType = typename decay<typename remove_reference<T>::type>::type;

struct any
{
    bool is_null() const { return !ptr; }
    bool not_null() const { return ptr; }

    template<typename U> any(U&& value)
        : ptr(new Derived<StorageType<U>>(forward<U>(value)))
    {

    }

    template<class U> bool is() const
    {
        typedef StorageType<U> T;

        auto derived = dynamic_cast<Derived<T>*> (ptr);

        return derived;
    }

    template<class U>
    StorageType<U>& as()
    {
        typedef StorageType<U> T;

        auto derived = dynamic_cast<Derived<T>*> (ptr);

        if (!derived)
            throw bad_cast();

        return derived->value;
    }

    template<class U>
    operator U()
    {
        return as<StorageType<U>>();
    }

    any()
        : ptr(nullptr)
    {

    }

    any(any& that)
        : ptr(that.clone())
    {

    }

    any(any&& that)
        : ptr(that.ptr)
    {
        that.ptr = nullptr;
    }

    any(const any& that)
        : ptr(that.clone())
    {

    }

    any(const any&& that)
        : ptr(that.clone())
    {

    }

    any& operator=(const any& a)
    {
        if (ptr == a.ptr)
            return *this;

        auto old_ptr = ptr;

        ptr = a.clone();

        if (old_ptr)
            delete old_ptr;

        return *this;
    }

    any& operator=(any&& a)
    {
        if (ptr == a.ptr)
            return *this;

        swap(ptr, a.ptr);

        return *this;
    }

    ~any()
    {
        if (ptr)
            delete ptr;
    }

private:
    struct Base
    {
        virtual ~Base() {}

        virtual Base* clone() const = 0;
    };

    template<typename T>
    struct Derived : Base
    {
        template<typename U> Derived(U&& value) : value(forward<U>(value)) { }

        T value;

        Base* clone() const { return new Derived<T>(value); }
    };

    Base* clone() const
    {
        if (ptr)
            return ptr->clone();
        else
            return nullptr;
    }

    Base* ptr;
};