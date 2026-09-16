export module wxl.core:noncopyable;

export namespace wxl::core {

class noncopyable
{
protected:
    noncopyable() = default;
    ~noncopyable() = default;

public:
    noncopyable(noncopyable const&) = delete;
    noncopyable& operator=(noncopyable const&) = delete;
    noncopyable(noncopyable&&) = delete;
    noncopyable& operator=(noncopyable&&) = delete;
};

}  // export namespace wxl::core
