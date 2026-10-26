export module wxl.core:noncopyable;

export namespace wxl::core {

/// Represents a base class whose copy and move operations are deleted, so that a class derived
/// from it can be neither copied nor moved. The constructor and the destructor are protected.
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
