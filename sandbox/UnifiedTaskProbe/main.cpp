// Что стоят горячие пути wxl.async и держит ли цепочка корутин любой глубины одну схему:
// результат, исключение, отмена токеном, отказ от середины.
//
// Один исходник для двух устройств библиотеки -- с отдельным типом операции рядом с task
// и с единым task<R> на обе роли. Поэтому тип того, что возвращают операции, здесь не
// назван нигде: с результатом sta_loop::async_call, call_here и async_file делают только
// co_await, auto и decltype, а своё -- корутины task<T>. Одна и та же проба собирается на
// обоих устройствах, и её таблицы сравниваются строка в строку. Формы с токеном у sta_loop
// нет: тело -- код приложения, и отвечать ли на отмену, решает оно само. Строки «+token»
// берут внутренние формы, которыми пользуются операции самой wxl --
// cancellation_detail::call_under(fn, stop) и call_under(orphanable, fn, stop); у ответа,
// готового в вызове (call_here), такой формы нет, и токен спрашивают до вызова. Поэтому
// строки «+token» собираются только на едином устройстве -- их имена прежние, чтобы
// сравниваться с замером «до».
//
// Замеры -- наносекунды на итерацию, медиана и лучший из прогонов:
//
//   task      без петли: корутина, которая кончается в вызове, и цепочки, дно которых
//             стоит на ожидании, возобновляемом руками, -- машинерия task и ничего под ней;
//   op        операция и цепочки глубиной 1..10 до неё в трёх режимах: здесь (call_here --
//             ответ готов в вызове, петли нет), по одной (вызов, приостановка, воркер,
//             обратный канал, результат) и 64 в полёте (пробуждения делятся на пачку, и
//             остаётся сама машинерия);
//   file      файл по имени -- exists и read_all, по одной.
//
// Каждая строка снята и без токена, и под токеном, который никто не отменяет: разница --
// цена формы с токеном для тех, кого ни разу не попросили.
//
// Проверки -- цепочки, где подпрограммы перемежаются операциями: результат доходит,
// исключение со дна доходит до верха, отмена сверху кончает цепочку
// operation_canceled_exception, отказ от среднего уровня не оставляет ни операций, ни
// возобновлений, и петля после всего останавливается чисто. Код возврата -- число
// провалившихся проверок.
//
// Запуск: sandbox.unified-task-probe [прогонов [множитель итераций]]; 0 прогонов -- только
// проверки.

import std;
import wxl.core;
import wxl.async;

namespace {

using namespace wxl::async;
using wxl::core::path;

using bench_clock = std::chrono::steady_clock;

double nanoseconds_between(bench_clock::time_point from, bench_clock::time_point to) {
    return std::chrono::duration<double, std::nano>(to - from).count();
}

#ifdef NDEBUG
constexpr const char* build_kind = "NDEBUG";
#else
constexpr const char* build_kind = "Debug";
#endif

#ifdef STRICT_CORO
constexpr const char* checks_kind = "STRICT_CORO";
#else
constexpr const char* checks_kind = "assert";
#endif

int g_failures = 0;

void check(const char* what, bool ok) {
    if (!ok) ++g_failures;

    std::printf("  %-4s %s\n", ok ? "ok" : "FAIL", what);
}

/// Что итерация прибавляет к сумме: число -- себя, ответ «есть ли» -- единицу, байты --
/// их длину. Сумма сверяется с ожидаемой, и замер без неё -- замер того, что оставил
/// оптимизатор.
long long weight(int value) { return value; }
long long weight(bool value) { return value ? 1 : 0; }
long long weight(const std::string& bytes) { return static_cast<long long>(bytes.size()); }

long long sum_below(int n) { return static_cast<long long>(n) * (n - 1) / 2; }

// ---------------------------------------------------------------------------------------
// Ожидание, которое возобновляют руками: дно цепочки без петли под ним.

struct parked {
    std::coroutine_handle<>* slot;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) const noexcept { *slot = here; }
    void await_resume() const noexcept {}
};

// ---------------------------------------------------------------------------------------
// Корутины замеров. Вызываемые -- noinline: кадр, вся жизнь которого видна вызывающему,
// компилятор вправе держать на стеке, и замер такого кадра не мерит ничего.

__declspec(noinline) task<int> at_once(int i) { co_return i; }

__declspec(noinline) task<int> at_once_under(int i, cancellation_token) { co_return i; }

__declspec(noinline) task<int> paused(int depth, std::coroutine_handle<>* slot, int i) {
    if (depth > 1) co_return co_await paused(depth - 1, slot, i);

    co_await parked{slot};
    co_return i;
}

__declspec(noinline) task<int> paused_under(int depth, std::coroutine_handle<>* slot, int i,
                                            cancellation_token stop) {
    if (depth > 1) co_return co_await paused_under(depth - 1, slot, i, stop);

    // Ожидание приложения, до которого wxl не дотягивается: токен корутина спрашивает сама.
    co_await parked{slot};
    stop.throw_if_canceled();
    co_return i;
}

/// Какая операция на дне: готовая в вызове, у воркера или сирота.
enum class leaf
{
    here,
    call,
    orphan
};

template <leaf kind>
auto operation(int i) {
    if constexpr (kind == leaf::here)
        return sta_loop::call_here([i] { return i; });
    else if constexpr (kind == leaf::call)
        return sta_loop::async_call([i] { return i; });
    else
        return sta_loop::async_call(orphanable, [i] { return i; });
}

/// Операция под токеном -- внутренняя форма с токеном, как у операций самой wxl: операция
/// стоит в событии токена, пока жива. У ответа, готового в вызове, такой формы нет: токен
/// спрашивают до вызова, как его спрашивает код приложения.
template <leaf kind>
auto operation_under(int i, const cancellation_token& stop) {
    if constexpr (kind == leaf::here) {
        stop.throw_if_canceled();
        return sta_loop::call_here([i] { return i; });
    } else if constexpr (kind == leaf::call) {
        return cancellation_detail::call_under([i] { return i; }, stop);
    } else {
        return cancellation_detail::call_under(orphanable, [i] { return i; }, stop);
    }
}

template <leaf kind>
__declspec(noinline) task<int> chain(int depth, int i) {
    if (depth > 1) co_return co_await chain<kind>(depth - 1, i);

    co_return co_await operation<kind>(i);
}

/// Токен идёт вниз параметром, копией в каждый кадр, как его передаёт приложение.
template <leaf kind>
__declspec(noinline) task<int> chain_under(int depth, int i, cancellation_token stop) {
    if (depth > 1) co_return co_await chain_under<kind>(depth - 1, i, stop);

    co_return co_await operation_under<kind>(i, stop);
}

/// Итерации одна за другой: make(i) -- что ждать на i-й.
template <class Make>
__declspec(noinline) task<long long> drive(int n, Make make, int& remaining) {
    long long sum = 0;

    for (int i = 0; i < n; ++i) sum += weight(co_await make(i));

    --remaining;
    co_return sum;
}

// ---------------------------------------------------------------------------------------
// Режимы. Каждый отвечает временем на итерацию или -1, если сумма не сошлась.

/// Одна корутина, операции по одной: каждая -- сон и пробуждение обоих потоков. Для
/// операции, готовой в вызове, петля не нужна вовсе: корутина кончается внутри вызова.
template <class Make>
double one_at_a_time(int n, long long expected, const Make& make) {
    int remaining = 1;

    const bench_clock::time_point from = bench_clock::now();
    task<long long> t = drive(n, make, remaining);
    sta_loop::run_until([&] { return remaining == 0; });
    const bench_clock::time_point to = bench_clock::now();

    return t.result() == expected ? nanoseconds_between(from, to) / n : -1;
}

/// Много корутин сразу: операции стоят в очереди воркера пачкой, и пробуждение одно на
/// пачку, а не на операцию.
template <class Make>
double in_flight(int tasks, int n, long long expected, const Make& make) {
    std::vector<task<long long>> running;
    running.reserve(static_cast<std::size_t>(tasks));
    int remaining = tasks;

    const bench_clock::time_point from = bench_clock::now();

    for (int k = 0; k < tasks; ++k) running.push_back(drive(n, make, remaining));

    sta_loop::run_until([&] { return remaining == 0; });
    const bench_clock::time_point to = bench_clock::now();

    bool right = true;

    for (task<long long>& t : running) right = t.result() == expected && right;

    return right ? nanoseconds_between(from, to) / (double(tasks) * n) : -1;
}

/// Без петли: дно возобновляют руками, пока корутина сверху не кончится.
template <class MakeFor>
double by_hand(int n, long long expected, const MakeFor& make_for) {
    std::coroutine_handle<> slot;
    int remaining = 1;

    const bench_clock::time_point from = bench_clock::now();
    task<long long> t = drive(n, make_for(&slot), remaining);
    while (!t.done()) slot.resume();
    const bench_clock::time_point to = bench_clock::now();

    return t.result() == expected ? nanoseconds_between(from, to) / n : -1;
}

struct settings {
    int rounds = 7;
    int hand = 100'000;
    int here = 50'000;
    int one = 2'000;
    int fan_tasks = 64;
    int fan_each = 1'000;
    int file = 1'000;
};

/// Медиана и лучший из прогонов. -1 -- столбца у строки нет, -2 -- сумма не сошлась.
struct timing {
    double median = -1;
    double best = -1;
};

template <class Round>
timing over_rounds(int rounds, const Round& round) {
    std::vector<double> runs;

    for (int r = 0; r < rounds; ++r) {
        const double ns = round();

        if (ns < 0) {
            ++g_failures;
            return {-2, -2};
        }

        runs.push_back(ns);
    }

    std::ranges::sort(runs);
    return {runs[runs.size() / 2], runs.front()};
}

void cell(timing t) {
    if (t.median == -1)
        std::printf(" %9s %9s", "-", "-");
    else if (t.median < 0)
        std::printf(" %9s %9s", "WRONG", "");
    else
        std::printf(" %9.1f %9.1f", t.median, t.best);
}

template <class Make>
timing here_column(const settings& how, const Make& make) {
    return over_rounds(how.rounds,
                       [&] { return one_at_a_time(how.here, sum_below(how.here), make); });
}

template <class Make>
timing one_column(const settings& how, const Make& make) {
    return over_rounds(how.rounds,
                       [&] { return one_at_a_time(how.one, sum_below(how.one), make); });
}

template <class Make>
timing fan_column(const settings& how, const Make& make) {
    return over_rounds(how.rounds, [&] {
        return in_flight(how.fan_tasks, how.fan_each, sum_below(how.fan_each), make);
    });
}

void op_row(const char* name, timing here, timing one, timing fan) {
    std::printf("  %-26s", name);
    cell(here);
    cell(one);
    cell(fan);
    std::putchar('\n');
}

/// Операция одна, глубина 0: в столбце «здесь» -- call_here, в остальных -- async_call.
template <leaf loop_kind>
void op_alone_row(const char* name, const settings& how) {
    const auto at_loop = [](int i) { return operation<loop_kind>(i); };

    timing here;

    if constexpr (loop_kind == leaf::call)
        here = here_column(how, [](int i) { return operation<leaf::here>(i); });

    op_row(name, here, one_column(how, at_loop), fan_column(how, at_loop));
}

template <leaf loop_kind>
void op_alone_row_under(const char* name, const settings& how, const cancellation_token& stop) {
    const auto at_loop = [&stop](int i) { return operation_under<loop_kind>(i, stop); };

    timing here;

    if constexpr (loop_kind == leaf::call)
        here = here_column(how, [&stop](int i) { return operation_under<leaf::here>(i, stop); });

    op_row(name, here, one_column(how, at_loop), fan_column(how, at_loop));
}

void chain_row(int depth, const settings& how) {
    char name[64];
    std::snprintf(name, sizeof name, "chain d=%d", depth);

    op_row(name, here_column(how, [depth](int i) { return chain<leaf::here>(depth, i); }),
           one_column(how, [depth](int i) { return chain<leaf::call>(depth, i); }),
           fan_column(how, [depth](int i) { return chain<leaf::call>(depth, i); }));
}

void chain_row_under(int depth, const settings& how, const cancellation_token& stop) {
    char name[64];
    std::snprintf(name, sizeof name, "chain d=%d +token", depth);

    op_row(
        name,
        here_column(how, [depth, &stop](int i) { return chain_under<leaf::here>(depth, i, stop); }),
        one_column(how, [depth, &stop](int i) { return chain_under<leaf::call>(depth, i, stop); }),
        fan_column(how, [depth, &stop](int i) { return chain_under<leaf::call>(depth, i, stop); }));
}

template <class MakeFor>
void hand_row(const char* name, const settings& how, const MakeFor& make_for) {
    std::printf("  %-26s", name);
    cell(over_rounds(how.rounds, [&] { return by_hand(how.hand, sum_below(how.hand), make_for); }));
    std::putchar('\n');
}

template <class Make>
void file_row(const char* name, const settings& how, long long each, const Make& make) {
    std::printf("  %-26s", name);
    cell(over_rounds(how.rounds, [&] { return one_at_a_time(how.file, each * how.file, make); }));
    std::putchar('\n');
}

constexpr int depths[] = {1, 2, 3, 5, 10};

void task_table(const settings& how, const cancellation_token& stop) {
    std::printf(
        "\ntask without the loop -- ns per iteration, "
        "median and best of %d rounds; n = %d\n\n",
        how.rounds, how.hand);
    std::printf("  %-26s %9s %9s\n", "", "median", "best");

    hand_row("task at once", how,
             [](std::coroutine_handle<>*) { return [](int i) { return at_once(i); }; });
    hand_row("task at once +token", how, [&stop](std::coroutine_handle<>*) {
        return [&stop](int i) { return at_once_under(i, stop); };
    });

    for (const int depth : depths) {
        char name[64];
        std::snprintf(name, sizeof name, "pause d=%d", depth);
        hand_row(name, how, [depth](std::coroutine_handle<>* slot) {
            return [depth, slot](int i) { return paused(depth, slot, i); };
        });
    }

    for (const int depth : depths) {
        char name[64];
        std::snprintf(name, sizeof name, "pause d=%d +token", depth);
        hand_row(name, how, [depth, &stop](std::coroutine_handle<>* slot) {
            return [depth, slot, &stop](int i) { return paused_under(depth, slot, i, stop); };
        });
    }
}

void op_table(const settings& how, const cancellation_token& stop) {
    std::printf(
        "\noperation, and chains of coroutines down to one -- ns per iteration, "
        "median and best of %d rounds\n",
        how.rounds);
    std::printf("  here: the leaf is call_here, answered inside the call, no loop; n = %d\n",
                how.here);
    std::printf(
        "  1 in flight: one coroutine, one operation at a time through the worker; n = %d\n",
        how.one);
    std::printf("  64 in flight: %d coroutines at once; n = %d x %d\n\n", how.fan_tasks,
                how.fan_tasks, how.fan_each);
    std::printf("  %-26s %19s %19s %19s\n", "", "here", "1 in flight", "64 in flight");
    std::printf("  %-26s %9s %9s %9s %9s %9s %9s\n", "", "median", "best", "median", "best",
                "median", "best");

    op_alone_row<leaf::call>("op d=0", how);
    op_alone_row<leaf::orphan>("op orphanable d=0", how);

    for (const int depth : depths) chain_row(depth, how);

    op_alone_row_under<leaf::call>("op d=0 +token", how, stop);
    op_alone_row_under<leaf::orphan>("op orphanable d=0 +token", how, stop);

    for (const int depth : depths) chain_row_under(depth, how, stop);
}

/// Файл таблицы файлов, записанный и спрошенный теми же операциями.
__declspec(noinline) task<bool> prepare(path file, std::string bytes) {
    co_await async_file::write_all(file, std::move(bytes));
    co_return co_await async_file::exists(file);
}

void file_table(const settings& how, const path& file, std::size_t size,
                const cancellation_token& stop) {
    std::printf(
        "\nfile by name, one at a time -- ns per iteration, "
        "median and best of %d rounds; n = %d\n\n",
        how.rounds, how.file);
    std::printf("  %-26s %9s %9s\n", "", "median", "best");

    const auto bytes = static_cast<long long>(size);

    file_row("exists", how, 1, [&file](int) { return async_file::exists(file); });
    file_row("exists +token", how, 1,
             [&file, &stop](int) { return async_file::exists(file, stop); });
    file_row("read_all 4 KB", how, bytes, [&file](int) { return async_file::read_all(file); });
    file_row("read_all 4 KB +token", how, bytes,
             [&file, &stop](int) { return async_file::read_all(file, stop); });
}

void size_table(const path& file, const cancellation_token& stop) {
    const auto one = [] { return 1; };

    std::printf("\nsizes, bytes: what a call returns, unnamed\n\n");
    std::printf("  %-38s %4zu\n", "async_call", sizeof(decltype(sta_loop::async_call(one))));
    std::printf("  %-38s %4zu\n", "async_call orphanable",
                sizeof(decltype(sta_loop::async_call(orphanable, one))));
    std::printf("  %-38s %4zu\n", "call_here", sizeof(decltype(sta_loop::call_here(one))));
    // Имя строки прежнее, чтобы сравниваться с замером «до»; меряется внутренняя форма с
    // токеном, cancellation_detail::call_under(fn, stop).
    std::printf("  %-38s %4zu\n", "cancellable(async_call, stop)",
                sizeof(decltype(cancellation_detail::call_under(one, stop))));
    std::printf("  %-38s %4zu\n", "async_file::read_all",
                sizeof(decltype(async_file::read_all(file))));
    std::printf("  %-38s %4zu\n", "async_file::read_all +token",
                sizeof(decltype(async_file::read_all(file, stop))));
    std::printf("  %-38s %4zu\n", "async_file::exists +token",
                sizeof(decltype(async_file::exists(file, stop))));
    std::printf("  %-38s %4zu\n", "task<int>", sizeof(task<int>));
    std::printf("  %-38s %4zu\n", "task<>", sizeof(task<>));
}

// ---------------------------------------------------------------------------------------
// Проверки.

template <class T>
T run_to_end(task<T> t) {
    sta_loop::run_until([&] { return t.done(); });
    return t.result();
}

/// Уровень вперемешку: операция у воркера, ниже -- подпрограмма или, на дне, операция-сирота,
/// и напоследок операция, готовая в вызове.
__declspec(noinline) task<int> mixed(int depth, int value) {
    const int before = co_await sta_loop::async_call([value] { return value + 1; });

    int below = 0;

    if (depth > 1)
        below = co_await mixed(depth - 1, before);
    else
        below = co_await sta_loop::async_call(orphanable, [before] { return before * 2; });

    co_return co_await sta_loop::call_here([below] { return below + 1; });
}

constexpr int mixed_answer(int depth, int value) {
    const int before = value + 1;
    const int below = depth > 1 ? mixed_answer(depth - 1, before) : before * 2;
    return below + 1;
}

/// То же с файлами под токеном: на каждом уровне операция под ним, на дне файл пишется,
/// спрашивается и читается целиком.
__declspec(noinline) task<std::string> files_under(int depth, path file, std::string text,
                                                   cancellation_token stop) {
    co_await cancellation_detail::call_under([] { return 0; }, stop);

    if (depth > 1)
        co_return co_await files_under(depth - 1, std::move(file), std::move(text), stop);

    co_await async_file::write_all(file, text, stop);

    if (!co_await async_file::exists(file, stop)) co_return std::string();

    co_return co_await async_file::read_all(file, stop);
}

/// Цепочка, дно которой бросает -- телом операции на воркере или телом самой корутины.
/// Код уровня после co_await не исполняется ни разу: `passed` остаётся нулём.
__declspec(noinline) task<int> throws_at_bottom(int depth, bool from_worker, int* passed) {
    co_await sta_loop::async_call([] { return 0; });

    if (depth > 1) {
        const int below = co_await throws_at_bottom(depth - 1, from_worker, passed);
        ++*passed;
        co_return below;
    }

    const auto fails = []() -> int { throw std::runtime_error("bottom"); };

    if (from_worker) co_return co_await sta_loop::async_call(fails);

    throw std::runtime_error("bottom");
}

/// Цепочка, дно которой стоит под токеном на операции, ждущей ворот.
__declspec(noinline) task<int> waits_at_gate(int depth, cancellation_token stop,
                                             std::atomic<bool>* gate, int* passed) {
    if (depth > 1) {
        const int below = co_await waits_at_gate(depth - 1, stop, gate, passed);
        ++*passed;
        co_return below;
    }

    const auto blocked = [gate] {
        gate->wait(false);
        return 1;
    };

    const int got = co_await cancellation_detail::call_under(blocked, stop);
    ++*passed;
    co_return got;
}

/// Цепочка, дно которой спрашивает файл формой с токеном.
__declspec(noinline) task<bool> asks_under(int depth, path file, cancellation_token stop,
                                           int* passed) {
    if (depth > 1) {
        const bool below = co_await asks_under(depth - 1, std::move(file), stop, passed);
        ++*passed;
        co_return below;
    }

    const bool there = co_await async_file::exists(file, stop);
    ++*passed;
    co_return there;
}

/// Где тело операции: 0 -- не начиналось, 1 -- идёт, 2 -- кончилось.
struct body_stage {
    std::atomic<int> value{0};
    std::binary_semaphore started{0};

    void set(int stage) {
        value.store(stage);

        if (stage == 1) started.release();
    }

    /// Ждёт начала тела, но не дольше пяти секунд: тело, которое так и не началось, --
    /// провал проверки, а не зависшая проба.
    void wait_started() { (void)started.try_acquire_for(std::chrono::seconds(5)); }
};

/// Живые копии того, что захватило тело операции: они живут, пока жива операция.
std::atomic<int> g_live{0};

struct tracked {
    tracked() noexcept { ++g_live; }
    tracked(const tracked&) noexcept { ++g_live; }
    tracked& operator=(const tracked&) = default;
    ~tracked() { --g_live; }
};

/// Дно, которое у воркера пишет в свой кадр: отказ от него обязан дождаться тела.
__declspec(noinline) task<int> bottom_writes(body_stage* stage, int* passed) {
    int into = 0;

    const int got = co_await sta_loop::async_call([&into, stage, keep = tracked{}] {
        stage->set(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        into = 42;
        stage->set(2);
        return 1;
    });

    ++*passed;
    co_return into + got;
}

/// Дно-сирота: в кадр не пишет, и отказ от него тела не ждёт.
__declspec(noinline) task<int> bottom_orphan(body_stage* stage, int* passed) {
    const int got = co_await sta_loop::async_call(orphanable, [stage, keep = tracked{}] {
        stage->set(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        stage->set(2);
        return 1;
    });

    ++*passed;
    co_return got;
}

__declspec(noinline) task<int> middle(int depth, bool orphan, body_stage* stage, int* passed) {
    int below = 0;

    if (depth > 1)
        below = co_await middle(depth - 1, orphan, stage, passed);
    else if (orphan)
        below = co_await bottom_orphan(stage, passed);
    else
        below = co_await bottom_writes(stage, passed);

    ++*passed;
    co_return below;
}

/// Верхний уровень держит средний у себя и отказывается от него, не дождавшись, когда тело
/// на дне уже идёт: владелец среднего уходит, а сам верхний продолжает своё.
__declspec(noinline) task<int> drops_its_middle(int depth, bool orphan, body_stage* stage,
                                                int* passed, int* stage_at_drop) {
    {
        task<int> kept = middle(depth, orphan, stage, passed);
        stage->wait_started();
    }

    *stage_at_drop = stage->value.load();

    co_return co_await sta_loop::async_call([] { return 7; });
}

void checks(const path& chain_file) {
    std::printf("\nchecks: chains of coroutines and operations at any depth\n\n");

    for (const int depth : {3, 5, 10}) {
        char what[96];
        std::snprintf(what, sizeof what, "d=%d, operations and coroutines in turn: result", depth);
        check(what, run_to_end(mixed(depth, 0)) == mixed_answer(depth, 0));
    }

    {
        cancellation_source source;
        const std::string text = "what was written comes back";
        std::string got;

        try {
            got = run_to_end(files_under(5, chain_file, text, source.token()));
        } catch (const std::exception&) {
        }

        check("d=5, files under a token: what was written comes back", got == text);
    }

    for (const bool from_worker : {true, false}) {
        int passed = 0;
        std::string what;

        try {
            run_to_end(throws_at_bottom(10, from_worker, &passed));
        } catch (const std::runtime_error& failure) {
            what = failure.what();
        }

        check(from_worker
                  ? "d=10, thrown by the bottom operation: reaches the top, no level goes on"
                  : "d=10, thrown by the bottom coroutine: reaches the top, no level goes on",
              what == "bottom" && passed == 0);
    }

    {
        cancellation_source source;
        std::atomic<bool> gate{false};
        int passed = 0;
        bool canceled = false;

        task<int> waiting = waits_at_gate(10, source.token(), &gate, &passed);
        const bool suspended = !waiting.done();

        source.cancel();
        gate.store(true);
        gate.notify_one();

        try {
            run_to_end(std::move(waiting));
        } catch (const operation_canceled_exception&) {
            canceled = true;
        }

        check("d=10, token cancelled at the top: operation_canceled_exception, no level goes on",
              suspended && canceled && passed == 0);
    }

    {
        cancellation_source source;
        source.cancel();
        int passed = 0;
        bool canceled = false;

        try {
            run_to_end(asks_under(10, chain_file, source.token(), &passed));
        } catch (const operation_canceled_exception&) {
            canceled = true;
        }

        check("d=10, token cancelled before the start: operation_canceled_exception",
              canceled && passed == 0);
    }

    {
        body_stage stage;
        int passed = 0;
        int at_drop = -1;
        const int got = run_to_end(drops_its_middle(5, false, &stage, &passed, &at_drop));

        check("d=5, middle dropped on a worker operation: body over first, nobody resumed",
              got == 7 && at_drop == 2 && passed == 0 && g_live == 0);
    }

    {
        body_stage stage;
        int passed = 0;
        int at_drop = -1;
        const int got = run_to_end(drops_its_middle(5, true, &stage, &passed, &at_drop));

        check("d=5, middle dropped on an orphanable operation: nobody resumed, operation gone",
              got == 7 && at_drop >= 1 && passed == 0 && g_live == 0);
    }

    {
        body_stage stage;
        int passed = 0;

        {
            task<int> whole = middle(10, false, &stage, &passed);
            stage.wait_started();
        }

        check("d=10, whole chain dropped by its owner: body over first, nobody resumed",
              stage.value.load() == 2 && passed == 0);
    }
}

}  // namespace

int main(int argc, char** argv) {
    settings how;

    if (argc > 1) how.rounds = std::max(0, std::atoi(argv[1]));

    if (argc > 2) {
        const double scale = std::max(0.001, std::atof(argv[2]));
        const auto scaled = [scale](int n) { return std::max(2, static_cast<int>(n * scale)); };

        how.hand = scaled(how.hand);
        how.here = scaled(how.here);
        how.one = scaled(how.one);
        how.fan_each = scaled(how.fan_each);
        how.file = scaled(how.file);
    }

    std::printf("wxl.async unified task probe -- %s, coroutine checks: %s\n", build_kind,
                checks_kind);

    sta_loop::start("unified task probe worker");

    const std::filesystem::path temporary = std::filesystem::temp_directory_path();
    const std::wstring file_name = (temporary / L"wxl.unified-task-probe.bin").wstring();
    const std::wstring chain_file_name =
        (temporary / L"wxl.unified-task-probe.chain.txt").wstring();

    {
        const path file(file_name.c_str());
        const path chain_file(chain_file_name.c_str());

        cancellation_source source;
        const cancellation_token stop = source.token();

        size_table(file, stop);
        checks(chain_file);

        if (how.rounds > 0) {
            task_table(how, stop);
            op_table(how, stop);

            const std::size_t size = 4 * 1024;
            bool prepared = false;

            try {
                prepared = run_to_end(prepare(file, std::string(size, 'x')));
            } catch (const std::exception&) {
            }

            if (prepared)
                file_table(how, file, size, stop);
            else
                check("the file for the file table was written", false);
        }
    }

    std::error_code ignored;
    std::filesystem::remove(file_name, ignored);
    std::filesystem::remove(chain_file_name, ignored);

    sta_loop::stop();

    std::printf("\nafter the loop\n\n");
    check("sta_loop::stop() returned, every operation given up is gone", g_live == 0);

    std::printf("\nfailed: %d\n", g_failures);
    return g_failures;
}
