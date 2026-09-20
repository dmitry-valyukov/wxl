// Выполняются ли деструкторы локальных объектов тела корутины — и когда её
// возобновляют до конца, и когда приостановленный кадр сносят destroy().
//
// Проба стоит отдельно от wxl и ничего из дерева не включает: если разница
// между Debug и Release здесь повторится, дело в компиляторе, а если нет —
// искать надо в библиотеке.
//
// Каждая проверка печатает строку; код возврата — число провалившихся.

#include <coroutine>
#include <cstdio>

namespace {

int failures = 0;

void check(const char* what, bool ok) {
    if (!ok) ++failures;
    std::printf("%-58s %s\n", what, ok ? "да" : "НЕТ");
}

// То, чьё уничтожение видно снаружи: то же, что Trace в тестах wxl.async.
struct Trace {
    bool* released;

    explicit Trace(bool* flag) noexcept : released(flag) {}
    Trace(const Trace&) = delete;
    ~Trace() { *released = true; }
};

// Ожидатель, который отдаёт наружу ручку собственной корутины и на этом
// приостанавливает её.
struct capture {
    std::coroutine_handle<>* out;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
    void await_resume() const noexcept {}
};

// Сколько раз кадр брали из своей кучи и сколько возвращали: заодно видно,
// выделяется ли кадр вообще или устранён компилятором.
int allocations = 0;
int deallocations = 0;

// Корутина, которая сама себе хозяин: оба конца suspend_never, кадр
// освобождается, как только тело кончилось. Это форма wxl::async::task.
struct self_owning {
    struct promise_type {
        self_owning get_return_object() const noexcept { return {}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };
};

// Она же, но кадр берётся своим operator new — как в wxl, где он идёт в пул.
struct self_owning_counted {
    struct promise_type {
        static void* operator new(std::size_t size) {
            ++allocations;
            return ::operator new(size);
        }

        static void operator delete(void* mem, std::size_t size) noexcept {
            ++deallocations;
            ::operator delete(mem, size);
        }

        self_owning_counted get_return_object() const noexcept { return {}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };
};

// Корутина, которую держит вызывающий: в конце она остаётся приостановленной,
// и кадр сносит владелец. Это форма wxl::async::managed_task.
struct owned {
    struct promise_type {
        owned get_return_object() noexcept {
            return owned{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };

    std::coroutine_handle<promise_type> handle;
};

self_owning waits_then_finishes(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};
    co_await capture{&out};
}

self_owning_counted waits_then_finishes_counted(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};
    co_await capture{&out};
}

owned waits_then_finishes_owned(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};
    co_await capture{&out};
}

}  // namespace

int main() {
    // 1. Тело доходит до конца: локальные объекты уничтожаются на выходе.
    {
        std::coroutine_handle<> waiter;
        bool released = false;

        waits_then_finishes(waiter, &released);
        check("сам себе хозяин: приостановился", static_cast<bool>(waiter));
        check("сам себе хозяин: до resume деструктор не звали", !released);

        waiter.resume();
        check("сам себе хозяин: resume довёл тело и отпустил локальное", released);
    }

    // 2. Приостановленный кадр сносят: живые локальные объекты обязаны
    // уничтожиться и здесь — на этом держится всякий RAII в корутине.
    {
        std::coroutine_handle<> waiter;
        bool released = false;

        waits_then_finishes(waiter, &released);
        check("сам себе хозяин: приостановился", static_cast<bool>(waiter));

        waiter.destroy();
        check("сам себе хозяин: destroy отпустил локальное", released);
    }

    // 3. То же со своим operator new — и видно, выделялся ли кадр.
    {
        std::coroutine_handle<> waiter;
        bool released = false;

        waits_then_finishes_counted(waiter, &released);
        waiter.destroy();
        check("свой operator new: destroy отпустил локальное", released);
        check("свой operator new: кадр выделялся", allocations == 1);
        check("свой operator new: кадр освобождён", deallocations == 1);
    }

    // 4. Форма с владельцем: кадр остаётся после конца тела, сносит его
    // владелец — деструкторы локальных объектов должны отработать раньше.
    {
        std::coroutine_handle<> waiter;
        bool released = false;

        const owned task = waits_then_finishes_owned(waiter, &released);
        waiter.resume();
        check("с владельцем: resume довёл тело и отпустил локальное", released);
        check("с владельцем: кадр дожил до владельца", task.handle && task.handle.done());
        task.handle.destroy();
    }

    // 5. Форма с владельцем, снос на приостановке.
    {
        std::coroutine_handle<> waiter;
        bool released = false;

        const owned task = waits_then_finishes_owned(waiter, &released);
        task.handle.destroy();
        check("с владельцем: destroy на приостановке отпустил локальное", released);
    }

    std::printf("\nпровалов: %d\n", failures);
    return failures;
}
