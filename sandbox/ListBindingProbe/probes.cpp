// ListBindingProbe, половина на проекции cppwinrt: окно, сценарий вопросов (а)-(д) и итог. Половина на wxl -- main.cpp
// (точка входа и списки для вопроса (г)), вопрос (е) -- дочерняя программа windowless.cpp. Что спрашивается и как
// читать ответ -- README.md рядом.
//
// Источник списков здесь -- свой объект COM на ABI (как wxl.ui/src/impl/value_box.h): IObservableVector<Object>,
// IVector<Object>, IIterable<Object> и IWeakReferenceSource, элементы -- такие же объекты-слоты. Каждый
// QueryInterface, каждый вызов и каждый AddRef/Release из чужого потока записываются -- это и есть ответ на (д) и на
// вопрос, можно ли считать ссылки такого объекта без атомиков (d2). Счётчик ссылок в пробе всё же атомарный: проба
// должна дожить до итога, даже если чужой поток придёт.

#include "platform.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "common.h"
#include "probe.h"

namespace probe {

namespace {

namespace data = winrt::Microsoft::UI::Xaml::Data;
namespace interop = winrt::Microsoft::UI::Xaml::Interop;
namespace peers = winrt::Microsoft::UI::Xaml::Automation::Peers;
namespace provider = winrt::Microsoft::UI::Xaml::Automation::Provider;
namespace dispatching = winrt::Microsoft::UI::Dispatching;

using Item = wf::IInspectable;
using ObservableVector = wfc::IObservableVector<Item>;
using Vector = wfc::IVector<Item>;
using Iterable = wfc::IIterable<Item>;

// ---- вывод -------------------------------------------------------------------------------------------------------

std::FILE* logFile = nullptr;

std::string vformat(char const* format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int const size = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (size <= 0) return {};
    std::string text(static_cast<size_t>(size), '\0');
    std::vsnprintf(text.data(), text.size() + 1, format, args);
    return text;
}

std::string strf(char const* format, ...) {
    va_list args;
    va_start(args, format);
    std::string text = vformat(format, args);
    va_end(args);
    return text;
}

// Строка -- в stdout (виден, когда вывод пробы перенаправлен) и в журнал рядом с exe. Только ASCII: кодировку
// перенаправленного вывода оболочка выбирает сама.
void say(char const* format, ...) {
    va_list args;
    va_start(args, format);
    std::string const text = vformat(format, args);
    va_end(args);
    std::fputs(text.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
    if (logFile) {
        std::fputs(text.c_str(), logFile);
        std::fputc('\n', logFile);
        std::fflush(logFile);
    }
}

struct Verdict {
    std::string line;
    bool surprise;
};

std::vector<Verdict> verdicts;
int unhandledErrors = 0;

// Строка ответа: вопрос -- наблюдение -- вывод, и сошлось ли с ожиданием (README.md, столбец «Ожидание»).
void verdict(char const* label, bool expected, std::string const& question, std::string const& observed,
             std::string const& conclusion) {
    std::string line = std::string {"("} + label + ") " + question + " -- " + observed + " -- " + conclusion +
                       (expected ? " [as expected]" : " [SURPRISE]");
    say("%s", line.c_str());
    verdicts.push_back({std::move(line), !expected});
}

std::string exeDirectory() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring text {path};
    text.resize(text.find_last_of(L'\\') + 1);
    return winrt::to_string(text);
}

std::wstring exeDirectoryW() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring text {path};
    text.resize(text.find_last_of(L'\\') + 1);
    return text;
}

// ---- журнал интерфейсов --------------------------------------------------------------------------------------------

struct QiCount {
    int hits = 0;
    int misses = 0;
};

struct GuidLess {
    bool operator()(winrt::guid const& a, winrt::guid const& b) const noexcept { return std::memcmp(&a, &b, sizeof a) < 0; }
};

// Всё, что XAML сделал с источниками одного вида контрола и их элементами.
struct Family {
    explicit Family(char const* name) : name(name) {}

    char const* name;
    std::map<winrt::guid, QiCount, GuidLess> sourceQi;
    std::map<winrt::guid, QiCount, GuidLess> itemQi;
    std::map<std::string, int> calls;
    int handlersAdded = 0;
    int handlersRemoved = 0;
    int weakTaken = 0;
    int weakResolved = 0;
    std::atomic<int> foreign {0};
    std::atomic<char const*> foreignWhat {nullptr};
};

// QueryInterface самой пробы (сравнение по идентичности) в журнал не идёт.
int quiet = 0;

struct Quiet {
    Quiet() noexcept { ++quiet; }
    ~Quiet() { --quiet; }
    Quiet(Quiet const&) = delete;
    Quiet& operator=(Quiet const&) = delete;
};

constexpr int32_t ok = 0;
constexpr int32_t noInterface = static_cast<int32_t>(0x80004002);     // E_NOINTERFACE
constexpr int32_t notImplemented = static_cast<int32_t>(0x80004001);  // E_NOTIMPL
constexpr int32_t outOfBounds = static_cast<int32_t>(0x8000000B);     // E_BOUNDS

constexpr winrt::guid iidWeakReference {0x00000037, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
constexpr winrt::guid iidWeakReferenceSource {0x00000038, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct Known {
    winrt::guid id;
    char const* name;
};

std::vector<Known> const& known() {
    static std::vector<Known> const all {
        {winrt::guid_of<wf::IUnknown>(), "IUnknown"},
        {winrt::guid_of<wf::IInspectable>(), "IInspectable"},
        {winrt::guid_of<Iterable>(), "IIterable<Object>"},
        {winrt::guid_of<Vector>(), "IVector<Object>"},
        {winrt::guid_of<wfc::IVectorView<Item>>(), "IVectorView<Object>"},
        {winrt::guid_of<ObservableVector>(), "IObservableVector<Object>"},
        {winrt::guid_of<interop::IBindableIterable>(), "IBindableIterable"},
        {winrt::guid_of<interop::IBindableVector>(), "IBindableVector"},
        {winrt::guid_of<interop::IBindableVectorView>(), "IBindableVectorView"},
        {winrt::guid_of<interop::IBindableObservableVector>(), "IBindableObservableVector"},
        {winrt::guid_of<interop::INotifyCollectionChanged>(), "INotifyCollectionChanged"},
        {winrt::guid_of<controls::IKeyIndexMapping>(), "IKeyIndexMapping"},
        {winrt::guid_of<data::ICollectionView>(), "ICollectionView"},
        {winrt::guid_of<data::ICollectionViewFactory>(), "ICollectionViewFactory"},
        {winrt::guid_of<data::ISupportIncrementalLoading>(), "ISupportIncrementalLoading"},
        {winrt::guid_of<data::IItemsRangeInfo>(), "IItemsRangeInfo"},
        {winrt::guid_of<data::ISelectionInfo>(), "ISelectionInfo"},
        {winrt::guid_of<data::ICustomPropertyProvider>(), "ICustomPropertyProvider"},
        {winrt::guid_of<data::INotifyPropertyChanged>(), "INotifyPropertyChanged"},
        {winrt::guid_of<wf::IStringable>(), "IStringable"},
        {winrt::guid_of<wf::IPropertyValue>(), "IPropertyValue"},
        {winrt::guid_of<wf::IClosable>(), "IClosable"},
        {iidWeakReferenceSource, "IWeakReferenceSource"},
        {iidWeakReference, "IWeakReference"},
        {{0x94EA2B94, 0xE9CC, 0x49E0, {0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90}}, "IAgileObject"},
        {{0x00000003, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}, "IMarshal"},
        {{0x000001CF, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}, "IMarshal2"},
        {{0xECC8691B, 0xC1DB, 0x4DC0, {0x85, 0x5E, 0x65, 0xF6, 0xC5, 0x51, 0xAF, 0x49}}, "INoMarshal"},
        {{0x00000018, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}, "IStdMarshalInfo"},
        {{0x00000019, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}, "IExternalConnection"},
        {{0x00000040, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}, "IFastRundown"},
        {{0x1C733A30, 0x2A1C, 0x11CE, {0xAD, 0xE5, 0x00, 0xAA, 0x00, 0x44, 0x77, 0x3D}}, "ICallFactory"},
        {{0x64BD43F8, 0xBFEE, 0x4EC4, {0xB7, 0xEB, 0x29, 0x35, 0x15, 0x8D, 0xAE, 0x21}}, "IReferenceTrackerTarget"},
        {{0x11D3B13A, 0x180E, 0x4789, {0xA8, 0xBE, 0x77, 0x12, 0x88, 0x28, 0x93, 0xE6}}, "IReferenceTracker"},
        {{0xB196B283, 0xBAB4, 0x101A, {0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1D, 0x07}}, "IProvideClassInfo"},
        {{0x00020400, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}}, "IDispatch"},
        {{0xC3FCC19E, 0xA970, 0x11D2, {0x8B, 0x5A, 0x00, 0xA0, 0xC9, 0xB7, 0xC9, 0xC4}}, "IManagedObject"},
    };
    return all;
}

std::string nameOf(winrt::guid const& id) {
    for (auto const& entry : known()) {
        if (entry.id == id) return entry.name;
    }
    return strf("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", id.Data1, id.Data2, id.Data3, id.Data4[0],
                  id.Data4[1], id.Data4[2], id.Data4[3], id.Data4[4], id.Data4[5], id.Data4[6], id.Data4[7]);
}

std::string qiText(std::map<winrt::guid, QiCount, GuidLess> const& log) {
    if (log.empty()) return "nothing";
    std::string text;
    for (auto const& [id, count] : log) {
        if (!text.empty()) text += ", ";
        text += nameOf(id);
        if (count.hits) text += strf(" hit x%d", count.hits);
        if (count.misses) text += strf(" miss x%d", count.misses);
    }
    return text;
}

std::string missedText(std::map<winrt::guid, QiCount, GuidLess> const& log) {
    std::string text;
    for (auto const& [id, count] : log) {
        if (count.hits || !count.misses) continue;
        if (!text.empty()) text += ", ";
        text += nameOf(id);
    }
    return text.empty() ? "none" : text;
}

std::string callsText(std::map<std::string, int> const& calls) {
    if (calls.empty()) return "none";
    std::string text;
    for (auto const& [name, count] : calls) {
        if (!text.empty()) text += ", ";
        text += strf("%s x%d", name.c_str(), count);
    }
    return text;
}

// ---- свой COM: слот и источник -------------------------------------------------------------------------------------

using unknown_abi = winrt::impl::unknown_abi;

// Слабая ссылка COM на ABI: IWeakReference и IWeakReferenceSource описаны здесь, а не взяты из внутренностей cppwinrt.
struct weak_reference_abi : unknown_abi {
    virtual int32_t __stdcall Resolve(winrt::guid const& id, void** object) noexcept = 0;
};

struct weak_source_vtable : unknown_abi {
    virtual int32_t __stdcall GetWeakReference(weak_reference_abi** reference) noexcept = 0;
};

// cppwinrt зовёт каждую таблицу ABI `type`; два базовых класса одного имени MSVC не примет -- поэтому слой с именем на
// каждый интерфейс (как interface_vtable в impl/value_box.h).
template <typename Interface>
struct vtable_of : winrt::impl::abi_t<Interface> {};

struct item_vtable : winrt::impl::inspectable_abi {};

struct WeakReference final : weak_reference_abi {
    WeakReference(unknown_abi* target, Family* family) : target(target), family(family) {}

    int32_t __stdcall QueryInterface(winrt::guid const& id, void** object) noexcept final {
        if (id == winrt::guid_of<wf::IUnknown>() || id == iidWeakReference) {
            AddRef();
            *object = static_cast<weak_reference_abi*>(this);
            return ok;
        }
        *object = nullptr;
        return noInterface;
    }

    uint32_t __stdcall AddRef() noexcept final { return ++refs; }

    uint32_t __stdcall Release() noexcept final {
        uint32_t const left = --refs;
        if (left == 0) delete this;
        return left;
    }

    int32_t __stdcall Resolve(winrt::guid const& id, void** object) noexcept final {
        ++family->weakResolved;
        if (!target) {
            *object = nullptr;
            return ok;
        }
        return target->QueryInterface(id, object);
    }

    unknown_abi* target;  // обнуляет сам объект, умирая
    Family* family;
    std::atomic<uint32_t> refs {1};
};

// IUnknown, IInspectable и IWeakReferenceSource объекта с журналом; Canonical -- таблица, которой объект отвечает на
// IUnknown (его идентичность), Others -- остальные таблицы. Одно определение метода покрывает его во всех базовых
// таблицах (компилятор ставит переходники), как в impl/value_box.h.
template <typename Derived, typename Canonical, typename... Others>
struct Logged : Canonical, Others..., weak_source_vtable {
    Logged(Family* family, bool item) : family_(family), item_(item), home_(::GetCurrentThreadId()) {}

    void* canonical() const noexcept { return const_cast<Canonical*>(static_cast<Canonical const*>(this)); }

    int32_t __stdcall QueryInterface(winrt::guid const& id, void** object) noexcept final {
        onThread("QueryInterface");
        void* found = nullptr;
        if (id == winrt::guid_of<wf::IUnknown>() || id == winrt::guid_of<wf::IInspectable>()) {
            found = canonical();
        } else if (id == iidWeakReferenceSource) {
            found = static_cast<weak_source_vtable*>(this);
        } else {
            found = static_cast<Derived*>(this)->find(id);
        }
        if (quiet == 0 && onHome()) {
            auto& count = (item_ ? family_->itemQi : family_->sourceQi)[id];
            if (found) {
                ++count.hits;
            } else {
                ++count.misses;
            }
        }
        if (!found) {
            *object = nullptr;
            return noInterface;
        }
        AddRef();
        *object = found;
        return ok;
    }

    uint32_t __stdcall AddRef() noexcept final {
        onThread("AddRef");
        return ++refs_;
    }

    uint32_t __stdcall Release() noexcept final {
        onThread("Release");
        uint32_t const left = --refs_;
        if (left == 0) delete static_cast<Derived*>(this);
        return left;
    }

    int32_t __stdcall GetIids(uint32_t* count, winrt::guid** ids) noexcept final {
        called("GetIids");
        *count = 0;
        *ids = nullptr;
        return ok;
    }

    int32_t __stdcall GetRuntimeClassName(void** name) noexcept final {
        called("GetRuntimeClassName");
        *name = winrt::detach_abi(winrt::hstring {Derived::className});
        return ok;
    }

    int32_t __stdcall GetTrustLevel(wf::TrustLevel* level) noexcept final {
        called("GetTrustLevel");
        *level = wf::TrustLevel::BaseTrust;
        return ok;
    }

    int32_t __stdcall GetWeakReference(weak_reference_abi** reference) noexcept final {
        called("GetWeakReference");
        ++family_->weakTaken;
        if (!weak_) weak_ = new WeakReference(static_cast<unknown_abi*>(static_cast<Canonical*>(this)), family_);
        weak_->AddRef();
        *reference = weak_;
        return ok;
    }

    // Вызов метода: в журнал, если пришёл из своего потока; иначе -- в счёт чужих.
    void called(char const* what) noexcept {
        onThread(what);
        if (quiet == 0 && onHome()) ++family_->calls[std::string {item_ ? "item." : ""} + what];
    }

protected:
    ~Logged() {
        if (weak_) {
            weak_->target = nullptr;
            weak_->Release();
        }
    }

    bool onHome() const noexcept { return ::GetCurrentThreadId() == home_; }

    void onThread(char const* what) noexcept {
        if (onHome()) return;
        ++family_->foreign;
        char const* none = nullptr;
        family_->foreignWhat.compare_exchange_strong(none, what);
    }

    Family* family_;
    bool item_;
    DWORD home_;
    std::atomic<uint32_t> refs_ {1};
    WeakReference* weak_ = nullptr;
};

// Элемент источника -- бокс-слот: свой маленький объект COM, отвечает только IInspectable (и слабой
// ссылкой -- чтобы увидеть, берёт ли её XAML).
struct Slot final : Logged<Slot, item_vtable> {
    static constexpr wchar_t const* className = L"wxl.probe.Slot";

    Slot(Family* family, int id) : Logged(family, true), id(id) {}

    void* find(winrt::guid const&) noexcept { return nullptr; }

    int const id;
};

// Сама ссылка на объект для сравнения: его IUnknown, без записи в журнал.
void* identityOf(void* object) noexcept {
    if (!object) return nullptr;
    Quiet const silence;
    void* unknown = nullptr;
    if (static_cast<unknown_abi*>(object)->QueryInterface(winrt::guid_of<wf::IUnknown>(), &unknown) < 0) return nullptr;
    static_cast<unknown_abi*>(unknown)->Release();
    return unknown;
}

void* identity(wf::IInspectable const& object) noexcept { return identityOf(winrt::get_abi(object)); }

struct ChangedArgs : winrt::implements<ChangedArgs, wfc::IVectorChangedEventArgs> {
    ChangedArgs(wfc::CollectionChange change, uint32_t index) : change_(change), index_(index) {}

    wfc::CollectionChange CollectionChange() const noexcept { return change_; }
    uint32_t Index() const noexcept { return index_; }

private:
    wfc::CollectionChange change_;
    uint32_t index_;
};

struct Source;

struct Iterator : winrt::implements<Iterator, wfc::IIterator<Item>> {
    explicit Iterator(Source* source);
    ~Iterator();

    Item Current() const;
    bool HasCurrent() const;
    bool MoveNext();
    uint32_t GetMany(winrt::array_view<Item> items);

private:
    Source* source_;
    uint32_t index_ = 0;
};

struct View : winrt::implements<View, wfc::IVectorView<Item>, wfc::IIterable<Item>> {
    explicit View(Source* source);
    ~View();

    Item GetAt(uint32_t index) const;
    uint32_t Size() const;
    bool IndexOf(Item const& value, uint32_t& index) const;
    uint32_t GetMany(uint32_t start, winrt::array_view<Item> items) const;
    wfc::IIterator<Item> First() const;

private:
    Source* source_;
};

// Источник: IObservableVector<Object> поверх слотов -- источник списка, привязанного к коллекции модели. Пишет
// его только проба (insertFront), изменяющие методы интерфейса -- E_NOTIMPL.
struct Source final : Logged<Source, vtable_of<ObservableVector>, vtable_of<Vector>, vtable_of<Iterable>> {
    static constexpr wchar_t const* className = L"wxl.probe.Source";

    Source(Family* family, int count) : Logged(family, false) {
        for (int i = 0; i < count; ++i) slots_.push_back(new Slot(family, i));
    }

    ~Source() {
        for (auto* slot : slots_) slot->Release();
    }

    void* find(winrt::guid const& id) noexcept {
        if (id == winrt::guid_of<ObservableVector>()) return static_cast<vtable_of<ObservableVector>*>(this);
        if (id == winrt::guid_of<Vector>()) return static_cast<vtable_of<Vector>*>(this);
        if (id == winrt::guid_of<Iterable>()) return static_cast<vtable_of<Iterable>*>(this);
        return nullptr;
    }

    // ---- со стороны пробы ----

    Item inspectable() const {
        Item result {nullptr};
        winrt::copy_from_abi(result, canonical());
        return result;
    }

    uint32_t size() const noexcept { return static_cast<uint32_t>(slots_.size()); }

    Item itemAt(uint32_t at) const {
        Item result {nullptr};
        winrt::copy_from_abi(result, slots_[at]->canonical());
        return result;
    }

    void* identityAt(uint32_t at) const noexcept { return slots_[at]->canonical(); }

    bool indexOf(void* identity, uint32_t& index) const noexcept {
        for (uint32_t i = 0; i < slots_.size(); ++i) {
            if (slots_[i]->canonical() == identity) {
                index = i;
                return true;
            }
        }
        index = 0;
        return false;
    }

    // Вставка перед всеми: по ItemInserted на элемент, после каждой вставки -- как сделал бы список модели.
    void insertFront(int count, int firstId) {
        for (int i = 0; i < count; ++i) {
            slots_.insert(slots_.begin() + i, new Slot(family_, firstId + i));
            raise(wfc::CollectionChange::ItemInserted, static_cast<uint32_t>(i));
        }
    }

    std::vector<uint32_t>* trace = nullptr;  // куда писать индексы GetAt, пока задано

    // ---- IIterable<Object> ----

    int32_t __stdcall First(void** iterator) noexcept override {
        called("First");
        try {
            *iterator = winrt::detach_abi(winrt::make<Iterator>(this));
            return ok;
        } catch (...) {
            *iterator = nullptr;
            return winrt::to_hresult();
        }
    }

    // ---- IVector<Object> ----

    int32_t __stdcall GetAt(uint32_t index, void** item) noexcept override {
        called("GetAt");
        if (trace && onHome()) trace->push_back(index);
        if (index >= slots_.size()) {
            *item = nullptr;
            return outOfBounds;
        }
        slots_[index]->AddRef();
        *item = slots_[index]->canonical();
        return ok;
    }

    int32_t __stdcall get_Size(uint32_t* size) noexcept override {
        called("Size");
        *size = static_cast<uint32_t>(slots_.size());
        return ok;
    }

    int32_t __stdcall GetView(void** view) noexcept override {
        called("GetView");
        try {
            *view = winrt::detach_abi(winrt::make<View>(this));
            return ok;
        } catch (...) {
            *view = nullptr;
            return winrt::to_hresult();
        }
    }

    int32_t __stdcall IndexOf(void* value, uint32_t* index, bool* found) noexcept override {
        called("IndexOf");
        *found = indexOf(identityOf(value), *index);
        return ok;
    }

    int32_t __stdcall SetAt(uint32_t, void*) noexcept override {
        called("SetAt");
        return notImplemented;
    }

    int32_t __stdcall InsertAt(uint32_t, void*) noexcept override {
        called("InsertAt");
        return notImplemented;
    }

    int32_t __stdcall RemoveAt(uint32_t) noexcept override {
        called("RemoveAt");
        return notImplemented;
    }

    int32_t __stdcall Append(void*) noexcept override {
        called("Append");
        return notImplemented;
    }

    int32_t __stdcall RemoveAtEnd() noexcept override {
        called("RemoveAtEnd");
        return notImplemented;
    }

    int32_t __stdcall Clear() noexcept override {
        called("Clear");
        return notImplemented;
    }

    int32_t __stdcall GetMany(uint32_t start, uint32_t capacity, void** items, uint32_t* actual) noexcept override {
        called("GetMany");
        if (start > slots_.size()) {
            *actual = 0;
            return outOfBounds;
        }
        uint32_t n = 0;
        for (; n < capacity && start + n < slots_.size(); ++n) {
            slots_[start + n]->AddRef();
            items[n] = slots_[start + n]->canonical();
        }
        *actual = n;
        return ok;
    }

    int32_t __stdcall ReplaceAll(uint32_t, void**) noexcept override {
        called("ReplaceAll");
        return notImplemented;
    }

    // ---- IObservableVector<Object> ----

    int32_t __stdcall add_VectorChanged(void* handler, winrt::event_token* token) noexcept override {
        called("add_VectorChanged");
        ++family_->handlersAdded;
        wfc::VectorChangedEventHandler<Item> delegate {nullptr};
        winrt::copy_from_abi(delegate, handler);
        token->value = ++nextToken_;
        handlers_.emplace_back(token->value, std::move(delegate));
        return ok;
    }

    int32_t __stdcall remove_VectorChanged(winrt::event_token token) noexcept override {
        called("remove_VectorChanged");
        ++family_->handlersRemoved;
        std::erase_if(handlers_, [&](auto const& entry) { return entry.first == token.value; });
        return ok;
    }

private:
    void raise(wfc::CollectionChange change, uint32_t index) {
        ObservableVector self {nullptr};
        winrt::copy_from_abi(self, static_cast<vtable_of<ObservableVector>*>(this));
        auto const args = winrt::make<ChangedArgs>(change, index);
        auto const handlers = handlers_;  // обработчик может снять себя
        for (auto const& entry : handlers) entry.second(self, args);
    }

    std::vector<Slot*> slots_;
    std::vector<std::pair<int64_t, wfc::VectorChangedEventHandler<Item>>> handlers_;
    int64_t nextToken_ = 0;
};

Iterator::Iterator(Source* source) : source_(source) { source_->AddRef(); }
Iterator::~Iterator() { source_->Release(); }

Item Iterator::Current() const {
    source_->called("iterator.Current");
    if (index_ >= source_->size()) throw winrt::hresult_out_of_bounds();
    return source_->itemAt(index_);
}

bool Iterator::HasCurrent() const {
    source_->called("iterator.HasCurrent");
    return index_ < source_->size();
}

bool Iterator::MoveNext() {
    source_->called("iterator.MoveNext");
    if (index_ < source_->size()) ++index_;
    return index_ < source_->size();
}

uint32_t Iterator::GetMany(winrt::array_view<Item> items) {
    source_->called("iterator.GetMany");
    uint32_t n = 0;
    while (n < items.size() && index_ < source_->size()) items[n++] = source_->itemAt(index_++);
    return n;
}

View::View(Source* source) : source_(source) { source_->AddRef(); }
View::~View() { source_->Release(); }

Item View::GetAt(uint32_t index) const {
    source_->called("view.GetAt");
    if (index >= source_->size()) throw winrt::hresult_out_of_bounds();
    return source_->itemAt(index);
}

uint32_t View::Size() const {
    source_->called("view.Size");
    return source_->size();
}

bool View::IndexOf(Item const& value, uint32_t& index) const {
    source_->called("view.IndexOf");
    return source_->indexOf(identity(value), index);
}

uint32_t View::GetMany(uint32_t start, winrt::array_view<Item> items) const {
    source_->called("view.GetMany");
    uint32_t n = 0;
    while (n < items.size() && start + n < source_->size()) {
        items[n] = source_->itemAt(start + n);
        ++n;
    }
    return n;
}

wfc::IIterator<Item> View::First() const {
    source_->called("view.First");
    return winrt::make<Iterator>(source_);
}

// ---- сценарий ------------------------------------------------------------------------------------------------------

// Шаги по таймеру диспетчера: шаг зовётся через свою паузу и возвращает, через сколько мс позвать его снова, или
// done -- тогда идёт следующий. Ожидание здесь -- время на раскладку и фазы контейнеров, которое XAML берёт кадрами.
class Script {
public:
    static constexpr int done = -1;

    void add(int delay, std::function<int()> body) { steps_.push_back({delay, std::move(body)}); }

    void once(int delay, std::function<void()> body) {
        add(delay, [body = std::move(body)] {
            body();
            return done;
        });
    }

    void start() {
        timer_ = dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
        timer_.IsRepeating(false);
        timer_.Tick([this](auto&&, auto&&) { tick(); });
        if (!steps_.empty()) arm(steps_.front().delay);
    }

    void stop() {
        if (timer_) timer_.Stop();
        at_ = steps_.size();
    }

private:
    struct Step {
        int delay;
        std::function<int()> body;
    };

    void arm(int delay) {
        timer_.Interval(std::chrono::milliseconds {delay});
        timer_.Start();
    }

    void tick() {
        if (at_ >= steps_.size()) return;
        int again = done;
        try {
            again = steps_[at_].body();
        } catch (winrt::hresult_error const& error) {
            verdict("step", false, strf("step %zu of the scenario", at_), strf("threw 0x%08X %s",
                    static_cast<uint32_t>(error.code().value), winrt::to_string(error.message()).c_str()),
                    "the step is skipped; the answers that depend on it are incomplete");
        } catch (std::exception const& error) {
            verdict("step", false, strf("step %zu of the scenario", at_), strf("threw %s", error.what()),
                    "the step is skipped; the answers that depend on it are incomplete");
        } catch (...) {
            verdict("step", false, strf("step %zu of the scenario", at_), "threw a foreign exception",
                    "the step is skipped; the answers that depend on it are incomplete");
        }
        if (at_ >= steps_.size()) return;
        if (again >= 0) {
            arm(again);
            return;
        }
        if (++at_ < steps_.size()) arm(steps_[at_].delay);
    }

    std::deque<Step> steps_;  // deque: ссылка на шаг живёт, пока он исполняется
    size_t at_ = 0;
    dispatching::DispatcherQueueTimer timer_ {nullptr};
};

// Список, построенный wxl (вопрос (г)).
struct WxlList {
    winrt::weak_ref<controls::ListView> weak;
    controls::ListView strong {nullptr};
    uintptr_t key = 0;  // адрес ABI ListViewBase -- ключ карты templates() в impl/item_template.cpp
    int builds = 0;
    bool keep = false;
};

// Всё состояние пробы. Не разрушается никогда: XAML может отпустить слот, элемент или источник и после итога, а они
// пишут сюда.
struct Run {
    WxlListMaker makeList = nullptr;
    xaml::Window window {nullptr};
    Script script;
    dispatching::DispatcherQueueTimer guard {nullptr};
    bool finished = false;

    Family listFamily {"ListView"};
    Family repeaterFamily {"ItemsRepeater"};
    Family itemsViewFamily {"ItemsView"};

    // (а)
    Census a1;
    Census a2;
    Census a3;
    Source* a1Source = nullptr;
    Source* a2Source = nullptr;
    Source* a3Source = nullptr;
    winrt::com_ptr<Factory> a1Factory;
    winrt::com_ptr<Factory> a2Factory;
    winrt::com_ptr<Factory> a3Factory;
    controls::ItemsRepeater a1Repeater {nullptr};
    controls::ItemsRepeater a2Repeater {nullptr};
    controls::ScrollViewer a1Viewer {nullptr};
    controls::ScrollViewer a2Viewer {nullptr};
    controls::ItemsView a3View {nullptr};
    double aOffset = 0;
    bool aDown = true;
    int aSteps = 0;

    // (б)
    Census b;
    TemplateStats bStats;
    Source* bSource = nullptr;
    controls::ListView bList {nullptr};
    controls::ScrollViewer bViewer {nullptr};
    double bOffset = 0;
    bool bDown = true;
    int bSteps = 0;
    std::string bFirstLook;

    // (в)
    Census v;
    TemplateStats vStats;
    Census vBox;
    TemplateStats vBoxStats;
    Source* vSource = nullptr;
    wfc::IObservableVector<Item> vBoxes {nullptr};
    controls::ListView vOwn {nullptr};
    controls::ListView vBoxList {nullptr};
    std::vector<void*> vContainersBefore;
    std::vector<Item> vOldItems;
    std::vector<void*> vBoxContainersBefore;
    std::vector<void*> vBoxIds;
    bool vSameBefore = true;
    bool vBoxSameBefore = true;
    int vIndexOk = 0;
    int vItemOk = 0;
    int vSameContainer = 0;
    int vBoxOk = 0;
    int vBoxSameContainer = 0;
    std::vector<uint32_t> getAtAfterInsert;
    bool insertShown = false;
    void* clicked = nullptr;
    void* selectedAdded = nullptr;
    void* selected = nullptr;
    std::string clickRoute;

    // (г)
    std::vector<WxlList> dead;
    std::vector<WxlList> born;
    std::set<uintptr_t> deadKeys;
    int deadConfirmed = 0;

    // закрытие окна со списком
    Census closing;
    TemplateStats closingStats;
    controls::ListView closeList {nullptr};
};

Run* current = nullptr;

Run& run() { return *current; }

HWND windowHandle() {
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(run().window.AppWindow().Id().Value));
}

void layout() {
    if (auto const root = run().window.Content()) root.UpdateLayout();
}

controls::ScrollViewer scrollerOf(xaml::UIElement const& content) {
    controls::ScrollViewer viewer;
    viewer.Width(220);
    viewer.Height(320);
    viewer.Content(content);
    return viewer;
}

// ---- (а) ItemsRepeater и ItemsView: элемент, отданный пустой RecycleElement ----------------------------------------

constexpr char const* questionA =
    "what ItemsRepeater/ItemsView do with an element handed to an empty RecycleElement (does it stay a child)";

void buildA() {
    auto& r = run();
    r.a1Source = new Source(&r.repeaterFamily, 600);
    r.a2Source = new Source(&r.repeaterFamily, 600);
    r.a3Source = new Source(&r.itemsViewFamily, 600);
    r.a1Factory = winrt::make_self<Factory>(&r.a1, Recycle::Nothing, false);
    r.a2Factory = winrt::make_self<Factory>(&r.a2, Recycle::RemoveFromParent, false);
    r.a3Factory = winrt::make_self<Factory>(&r.a3, Recycle::Nothing, true);

    r.a1Repeater = controls::ItemsRepeater {};
    r.a1Repeater.ItemTemplate(r.a1Factory.as<xaml::IElementFactory>());
    r.a1Repeater.ItemsSource(r.a1Source->inspectable());
    r.a1Viewer = scrollerOf(r.a1Repeater);

    r.a2Repeater = controls::ItemsRepeater {};
    r.a2Repeater.ItemTemplate(r.a2Factory.as<xaml::IElementFactory>());
    r.a2Repeater.ItemsSource(r.a2Source->inspectable());
    r.a2Viewer = scrollerOf(r.a2Repeater);

    r.a3View = controls::ItemsView {};
    r.a3View.Width(220);
    r.a3View.Height(320);
    r.a3View.ItemTemplate(r.a3Factory.as<xaml::IElementFactory>());
    r.a3View.ItemsSource(r.a3Source->inspectable());

    controls::StackPanel row;
    row.Orientation(controls::Orientation::Horizontal);
    row.Spacing(24);
    row.Children().Append(r.a1Viewer);
    row.Children().Append(r.a2Viewer);
    row.Children().Append(r.a3View);
    r.window.Content(row);
}

// Все три -- вниз до конца шагами по 240 px и обратно, по шагу в 40 мс.
int sweepA() {
    auto& r = run();
    if (!r.a3View) return Script::done;  // не построено: шаг постройки уже сказал почему
    auto const view = r.a3View.ScrollView();
    double const extent = std::max({r.a1Viewer.ScrollableHeight(), r.a2Viewer.ScrollableHeight(),
                                    view ? view.ScrollableHeight() : 0.0});
    bool last = ++r.aSteps > 400;
    if (r.aDown) {
        r.aOffset += 240;
        if (r.aOffset >= extent) {
            r.aOffset = extent;
            r.aDown = false;
        }
    } else {
        r.aOffset -= 240;
        if (r.aOffset <= 0) {
            r.aOffset = 0;
            last = true;
        }
    }
    r.a1Viewer.ChangeView(nullptr, wf::IReference<double> {r.aOffset}, nullptr, true);
    r.a2Viewer.ChangeView(nullptr, wf::IReference<double> {r.aOffset}, nullptr, true);
    if (view) {
        view.ScrollTo(0, r.aOffset,
                      controls::ScrollingScrollOptions {controls::ScrollingAnimationMode::Disabled,
                                                        controls::ScrollingSnapPointsMode::Ignore});
    }
    layout();
    return last ? Script::done : 40;
}

void reportA() {
    auto& r = run();
    if (!r.a3View) {
        verdict("a", false, questionA, "the controls were not built (the failed step above)", "not answered");
        return;
    }
    auto const inner = findDescendant<controls::ItemsRepeater>(r.a3View);
    int const children1 = xaml::Media::VisualTreeHelper::GetChildrenCount(r.a1Repeater);
    int const children2 = xaml::Media::VisualTreeHelper::GetChildrenCount(r.a2Repeater);
    int const children3 = inner ? xaml::Media::VisualTreeHelper::GetChildrenCount(inner) : -1;
    say("    (a) sweep: %d steps of 240 px down and back up", r.aSteps);
    say("    (a) ItemsRepeater, empty RecycleElement: built %d, children of the repeater %d, alive %d, given back %d",
        r.a1.built, children1, r.a1.alive, r.a1Factory->recycled);
    say("    (a) ItemsRepeater, RecycleElement removes it from the children: built %d, children %d, alive %d, given back %d",
        r.a2.built, children2, r.a2.alive, r.a2Factory->recycled);
    say("    (a) ItemsView, empty RecycleElement: built %d, children of its repeater %d, alive %d, given back %d",
        r.a3.built, children3, r.a3.alive, r.a3Factory->recycled);

    // Остаётся ребёнком: почти всё построенное -- до сих пор дети повторителя; со снятием -- детей втрое меньше, чем
    // построено, и живы только они.
    bool const stays1 = r.a1.built > 100 && children1 * 10 >= r.a1.built * 9;
    bool const stays3 = r.a3.built > 100 && children3 * 10 >= r.a3.built * 9;
    bool const bounded2 = r.a2.built > 100 && children2 * 3 < r.a2.built && r.a2.alive <= children2 + 8;
    std::string const observed =
        strf("empty RecycleElement: ItemsRepeater keeps %d of %d built as children (%d alive), ItemsView %d of %d (%d alive); "
               "removing in RecycleElement: %d children of %d built, %d alive",
               children1, r.a1.built, r.a1.alive, children3, r.a3.built, r.a3.alive, children2, r.a2.built, r.a2.alive);
    std::string conclusion;
    if (stays1 && stays3) {
        conclusion = "an element given back stays a child of the repeater, alive, for good: Gallery leaks on every scroll; "
                     "the factory must take it off the repeater's children itself (or keep it for reuse)";
    } else if (!stays1 && !stays3) {
        conclusion = "the repeater lets go of an element given back by itself: no leak in the empty RecycleElement";
    } else {
        conclusion = "ItemsRepeater and ItemsView differ (see the numbers): each needs its own answer";
    }
    if (bounded2) {
        conclusion += "; removing it from the parent in RecycleElement keeps the children to the realized window";
    } else {
        conclusion += "; removing it from the parent in RecycleElement did NOT bound the children (see the numbers)";
    }
    verdict("a", stays1 && stays3 && bounded2, questionA, observed, conclusion);
}

// ---- (б) ListView: элемент после Content(nullptr) в очереди повторного использования ------------------------------

constexpr char const* questionB =
    "ListView: is the element released once its container is in the recycle queue and the place is cleared with Content(nullptr)";

void buildB() {
    auto& r = run();
    r.bSource = new Source(&r.listFamily, 600);
    r.bList = controls::ListView {};
    r.bList.Width(300);
    r.bList.Height(320);
    hookTemplate(r.bList, &r.b, &r.bStats);
    r.bList.ItemsSource(r.bSource->inspectable());
    r.window.Content(r.bList);
}

int sweepB() {
    auto& r = run();
    if (!r.bViewer) r.bViewer = findDescendant<controls::ScrollViewer>(r.bList);
    if (!r.bViewer) throw winrt::hresult_error(E_FAIL, L"no ScrollViewer in the ListView template");
    bool last = ++r.bSteps > 400;
    if (r.bDown) {
        r.bOffset += 400;
        if (r.bOffset >= r.bViewer.ScrollableHeight()) {
            r.bOffset = r.bViewer.ScrollableHeight();
            r.bDown = false;
        }
    } else {
        r.bOffset -= 400;
        if (r.bOffset <= 0) {
            r.bOffset = 0;
            last = true;
        }
    }
    r.bViewer.ChangeView(nullptr, wf::IReference<double> {r.bOffset}, nullptr, true);
    layout();
    return last ? Script::done : 40;
}

std::string lookB() {
    auto& r = run();
    if (!r.bList) return "the list was not built";
    int const placed = placedIn(r.bList);
    auto const panel = r.bList.ItemsPanelRoot();
    int const containers = panel ? static_cast<int>(panel.Children().Size()) : -1;
    return strf("built %d, alive %d, held by containers %d, alive beyond them %d, containers %d, phase 0 x%d, phase 1 x%d, "
                  "recycled x%d", r.b.built, r.b.alive, placed, r.b.alive - placed, containers, r.bStats.phase0,
                  r.bStats.phase1, r.bStats.recycled);
}

void reportB() {
    auto& r = run();
    if (!r.bList) {
        verdict("b", false, questionB, "the list was not built (the failed step above)", "not answered");
        return;
    }
    std::string const later = lookB();
    say("    (b) sweep: %d steps of 400 px down and back up", r.bSteps);
    say("    (b) 0.8 s after the sweep: %s", r.bFirstLook.c_str());
    say("    (b) 2.3 s after the sweep: %s", later.c_str());
    int const extra = r.b.alive - placedIn(r.bList);
    bool const released = extra <= 0 && r.bStats.recycled > 0;
    std::string const conclusion =
        released ? "XAML keeps no element beyond the containers that hold one: the build scope can close at InRecycleQueue "
                   "(Content(nullptr)) and the element dies with it"
                 : strf("%d cleared elements are still alive beyond the containers: something in XAML holds them after "
                          "Content(nullptr) (or recycling did not happen)", extra);
    verdict("b", released, questionB, later, conclusion);
}

// ---- (в) ListView держит те же боксы после вставки перед ними ------------------------------------------------------

constexpr char const* questionV =
    "ListView after an insertion before the realized items: same box objects (clickedItem/selectedItem is the very slot)";

controls::ListView clickList(Census* census, TemplateStats* stats) {
    controls::ListView list;
    list.Width(300);
    list.Height(320);
    list.SelectionMode(controls::ListViewSelectionMode::Single);
    list.IsItemClickEnabled(true);
    hookTemplate(list, census, stats);
    return list;
}

void buildV() {
    auto& r = run();
    r.vSource = new Source(&r.listFamily, 50);
    r.vBoxes = winrt::single_threaded_observable_vector<Item>();
    for (int64_t i = 0; i < 50; ++i) r.vBoxes.Append(winrt::box_value(i));

    r.vOwn = clickList(&r.v, &r.vStats);
    r.vOwn.ItemsSource(r.vSource->inspectable());
    r.vOwn.ItemClick([](wf::IInspectable const&, controls::ItemClickEventArgs const& args) {
        run().clicked = identity(args.ClickedItem());
    });
    r.vOwn.SelectionChanged([](wf::IInspectable const&, controls::SelectionChangedEventArgs const& args) {
        if (args.AddedItems().Size() > 0) run().selectedAdded = identity(args.AddedItems().GetAt(0));
    });

    r.vBoxList = clickList(&r.vBox, &r.vBoxStats);
    r.vBoxList.ItemsSource(r.vBoxes);

    controls::StackPanel row;
    row.Orientation(controls::Orientation::Horizontal);
    row.Spacing(24);
    row.Children().Append(r.vOwn);
    row.Children().Append(r.vBoxList);
    r.window.Content(row);
}

void beforeInsertV() {
    auto& r = run();
    if (!r.vBoxList) return;
    for (int k = 0; k < 5; ++k) {
        auto const container = r.vOwn.ContainerFromIndex(k);
        r.vContainersBefore.push_back(identity(container));
        r.vOldItems.push_back(r.vSource->itemAt(static_cast<uint32_t>(k)));
        r.vSameBefore = r.vSameBefore && container && identity(r.vOwn.ItemFromContainer(container)) == r.vSource->identityAt(k);

        auto const boxContainer = r.vBoxList.ContainerFromIndex(k);
        r.vBoxIds.push_back(identity(r.vBoxes.GetAt(static_cast<uint32_t>(k))));
        r.vBoxContainersBefore.push_back(identity(boxContainer));
        r.vBoxSameBefore =
            r.vBoxSameBefore && boxContainer && identity(r.vBoxList.ItemFromContainer(boxContainer)) == r.vBoxIds.back();
    }
}

void insertV() {
    auto& r = run();
    if (r.vOldItems.size() < 5) return;
    r.vSource->trace = &r.getAtAfterInsert;
    r.vSource->insertFront(3, 1000);
    for (uint32_t i = 0; i < 3; ++i) r.vBoxes.InsertAt(i, winrt::box_value(int64_t {1000} + i));
}

void afterInsertV() {
    auto& r = run();
    if (r.vOldItems.size() < 5) return;
    r.vSource->trace = nullptr;
    for (int k = 0; k < 5; ++k) {
        auto const container = r.vOwn.ContainerFromItem(r.vOldItems[k]);
        if (container) {
            if (r.vOwn.IndexFromContainer(container) == k + 3) ++r.vIndexOk;
            if (identity(r.vOwn.ItemFromContainer(container)) == identity(r.vOldItems[k])) ++r.vItemOk;
            if (identity(container) == r.vContainersBefore[k]) ++r.vSameContainer;
        }
        auto const boxContainer = r.vBoxList.ContainerFromIndex(k + 3);
        if (boxContainer) {
            if (identity(r.vBoxList.ItemFromContainer(boxContainer)) == r.vBoxIds[k]) ++r.vBoxOk;
            if (identity(boxContainer) == r.vBoxContainersBefore[k]) ++r.vBoxSameContainer;
        }
    }
    if (auto const first = r.vOwn.ContainerFromIndex(0)) {
        r.insertShown = identity(r.vOwn.ItemFromContainer(first)) == r.vSource->identityAt(0);
    }

    // Щелчок без человека: сперва образец Invoke автоматизации у контейнера.
    auto const container = r.vOwn.ContainerFromIndex(5).try_as<xaml::UIElement>();
    if (!container) {
        r.clickRoute = "no container at index 5";
        return;
    }
    auto const peer = peers::FrameworkElementAutomationPeer::CreatePeerForElement(container);
    wf::IInspectable pattern {nullptr};
    if (peer) pattern = peer.GetPattern(peers::PatternInterface::Invoke);
    if (pattern) {
        if (auto const invoke = pattern.try_as<provider::IInvokeProvider>()) {
            invoke.Invoke();
            r.clickRoute = "UI Automation Invoke on the container";
            return;
        }
    }
    r.clickRoute = "the container has no Invoke pattern";
}

// Не вышло автоматизацией -- Enter по контейнеру в фокусе клавиатуры, если окно пробы впереди.
void keyV() {
    auto& r = run();
    if (r.vOldItems.size() < 5) return;
    if (r.clicked) return;
    if (::GetForegroundWindow() != windowHandle()) {
        r.clickRoute += "; the probe window is not in front, no key sent";
        return;
    }
    auto const control = r.vOwn.ContainerFromIndex(5).try_as<controls::Control>();
    if (!control || !control.Focus(xaml::FocusState::Keyboard)) {
        r.clickRoute += "; the container refused focus";
        return;
    }
    INPUT inputs[2] {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_RETURN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_RETURN;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(2, inputs, sizeof(INPUT));
    r.clickRoute += "; Enter sent to the focused container";
}

void reportV() {
    auto& r = run();
    if (r.vOldItems.size() < 5) {
        verdict("v", false, questionV, "the lists were not built or not realized (the failed step above)", "not answered");
        return;
    }
    r.vOwn.SelectedIndex(5);
    r.selected = identity(r.vOwn.SelectedItem());
    void* const target = identity(r.vOldItems[2]);

    std::string trace;
    for (auto const index : r.getAtAfterInsert) trace += strf(trace.empty() ? "%u" : " %u", index);
    say("    (v) own slots, before the insertion: ItemFromContainer is the slot itself for indices 0-4: %s",
        r.vSameBefore ? "yes" : "NO");
    say("    (v) own slots, after 3 inserted at 0: old items 0-4 -> index+3 %d/5, ItemFromContainer is the old slot %d/5, "
        "same container object %d/5", r.vIndexOk, r.vItemOk, r.vSameContainer);
    say("    (v) GetAt after the insertion: %s", trace.empty() ? "none" : trace.c_str());
    say("    (v) the new slot shows at index 0: %s", r.insertShown ? "yes" : "NO");
    say("    (v) click: %s; ItemClick.ClickedItem is the old slot 2: %s", r.clickRoute.c_str(),
        !r.clicked ? "no click arrived" : (r.clicked == target ? "yes" : "NO, another object"));
    say("    (v) SelectedIndex = 5: SelectedItem is the old slot 2: %s, SelectionChanged.AddedItems[0] is it: %s",
        r.selected == target ? "yes" : "NO", r.selectedAdded == target ? "yes" : "NO");
    say("    (v) PropertyValue boxes (winrt::box_value) for comparison: ItemFromContainer is the box before: %s, after the "
        "insertion %d/5 (same container %d/5)", r.vBoxSameBefore ? "yes" : "no", r.vBoxOk, r.vBoxSameContainer);

    bool const clickOk = !r.clicked || r.clicked == target;
    bool const held = r.vSameBefore && r.vIndexOk == 5 && r.vItemOk == 5 && r.selected == target &&
                      r.selectedAdded == target && clickOk;
    std::string const click = !r.clicked             ? "not observed (" + r.clickRoute + ")"
                              : r.clicked == target ? std::string {"is the slot"}
                                                    : std::string {"is NOT the slot"};
    std::string const observed =
        strf("after the insertion the old slots sit at index+3 (%d/5) as the same objects (%d/5); selectedItem %s; "
               "clickedItem %s; boxes of winrt::box_value keep identity %d/5",
               r.vIndexOk, r.vItemOk, r.selected == target ? "is the slot" : "is NOT the slot", click.c_str(), r.vBoxOk);
    std::string const conclusion =
        held ? "ListView stores the object the source gave and hands it back as is: a slot box with a position that follows "
               "insertions is enough for clickedItem/selectedItem to lead to the current element"
             : "ListView does not keep the object the source gave (see the numbers): a slot needs another way to its element";
    verdict("v", held, questionV, observed, conclusion);
}

// ---- (г) состояние шаблона по адресу ABI (impl/item_template.cpp) -------------------------------------------------

constexpr char const* questionG =
    "wxl itemTemplate state keyed by the ABI address: a list that never loaded dies, a new one at its address "
    "-- is it left without the template";

void takeList(::IInspectable* raw, void* context) {
    auto& entry = *static_cast<WxlList*>(context);
    wf::IInspectable object {nullptr};
    winrt::copy_from_abi(object, raw);
    auto const list = object.as<controls::ListView>();
    entry.key = reinterpret_cast<uintptr_t>(winrt::get_abi(list.as<controls::ListViewBase>()));
    entry.weak = winrt::make_weak(list);
    if (entry.keep) entry.strong = list;
}

// Первые 64 строит wxl, они ни разу не загружаются и умирают с обёрткой: запись в карте templates() остаётся.
void deadG() {
    auto& r = run();
    r.dead.resize(64);
    for (auto& entry : r.dead) r.makeList(&entry.builds, &takeList, &entry);
}

// Вторые 64 -- в окно; у тех, что встали на адрес умершего, строитель не должен звать ни разу.
void bornG() {
    auto& r = run();
    for (auto const& entry : r.dead) {
        if (!entry.weak.get()) {
            ++r.deadConfirmed;
            r.deadKeys.insert(entry.key);
        }
    }
    r.born.resize(64);
    for (auto& entry : r.born) {
        entry.keep = true;
        r.makeList(&entry.builds, &takeList, &entry);
    }
    controls::StackPanel rows;
    rows.Spacing(4);
    for (int row = 0; row < 8; ++row) {
        controls::StackPanel line;
        line.Orientation(controls::Orientation::Horizontal);
        line.Spacing(4);
        for (int column = 0; column < 8; ++column) line.Children().Append(r.born[row * 8 + column].strong);
        rows.Children().Append(line);
    }
    r.window.Content(rows);
}

void reportG() {
    auto& r = run();
    int reused = 0;
    int reusedSilent = 0;
    int fresh = 0;
    int freshBuilt = 0;
    for (auto const& entry : r.born) {
        if (r.deadKeys.contains(entry.key)) {
            ++reused;
            if (entry.builds == 0) ++reusedSilent;
        } else {
            ++fresh;
            if (entry.builds > 0) ++freshBuilt;
        }
    }
    say("    (g) 64 lists built by wxl and never loaded: %d confirmed dead (weak reference empty)", r.deadConfirmed);
    say("    (g) 64 new lists in the window: %d at an address of a dead one (%d of them never called the builder), "
        "%d at a new address (%d called it)", reused, reusedSilent, fresh, freshBuilt);
    std::string const observed =
        strf("%d of 64 never-loaded lists confirmed dead; %d of 64 new lists took the address of one; %d of those never "
             "built an item; %d of %d at new addresses did", r.deadConfirmed, reused, reusedSilent, freshBuilt, fresh);
    bool const confirmed = reused > 0 && reusedSilent == reused && freshBuilt == fresh;
    std::string conclusion;
    if (reused == 0) {
        conclusion = "INCONCLUSIVE: no address came back in this run (run again); the stale entry itself is certain from "
                     "the code";
    } else if (confirmed) {
        conclusion = "confirmed: the stale entry is taken for the new list, its ContainerContentChanging is never hooked and "
                     "the template silently does nothing -- the state must live with the control, not in a map by address";
    } else {
        conclusion = "not as the code reads (see the numbers)";
    }
    verdict("g", confirmed, questionG, observed, conclusion);
}

// ---- (д) интерфейсы источника и (d2) чужие потоки ------------------------------------------------------------------

void reportFamily(Family const& family) {
    say("    (d) %s, source asked: %s", family.name, qiText(family.sourceQi).c_str());
    say("    (d) %s, items asked: %s", family.name, qiText(family.itemQi).c_str());
    say("    (d) %s, calls: %s", family.name, callsText(family.calls).c_str());
    char const* const what = family.foreignWhat.load();
    say("    (d) %s: VectorChanged handlers added %d, removed %d; weak references taken %d, resolved %d; "
        "calls from another thread %d%s%s", family.name, family.handlersAdded, family.handlersRemoved, family.weakTaken,
        family.weakResolved, family.foreign.load(), what ? ", first: " : "", what ? what : "");
}

void reportD() {
    auto& r = run();
    reportFamily(r.listFamily);
    reportFamily(r.repeaterFamily);
    reportFamily(r.itemsViewFamily);

    bool const listWorks = r.b.built > 0 && r.v.built > 0;
    bool const repeaterWorks = r.a1.built > 0 && r.a2.built > 0;
    bool const itemsViewWorks = r.a3.built > 0;
    std::string const observed =
        strf("ListView missed %s; ItemsRepeater (ItemsSourceView) missed %s; ItemsView missed %s; items missed: ListView %s, "
               "ItemsRepeater %s, ItemsView %s; weak references taken: ListView %d, ItemsRepeater %d, ItemsView %d; "
               "VectorChanged subscribed: ListView %d, ItemsRepeater %d, ItemsView %d",
               missedText(r.listFamily.sourceQi).c_str(), missedText(r.repeaterFamily.sourceQi).c_str(),
               missedText(r.itemsViewFamily.sourceQi).c_str(), missedText(r.listFamily.itemQi).c_str(),
               missedText(r.repeaterFamily.itemQi).c_str(), missedText(r.itemsViewFamily.itemQi).c_str(),
               r.listFamily.weakTaken, r.repeaterFamily.weakTaken, r.itemsViewFamily.weakTaken,
               r.listFamily.handlersAdded, r.repeaterFamily.handlersAdded, r.itemsViewFamily.handlersAdded);
    bool const works = listWorks && repeaterWorks && itemsViewWorks && r.insertShown;
    std::string const conclusion =
        works ? "a source answering IObservableVector<Object>/IVector<Object>/IIterable<Object> is taken by all three and its "
                "ItemInserted is honoured; what is missed above is optional (full lists in the detail lines)"
              : strf("the source is not enough: ListView %s, ItemsRepeater %s, ItemsView %s, insertion %s",
                       listWorks ? "works" : "FAILED", repeaterWorks ? "works" : "FAILED",
                       itemsViewWorks ? "works" : "FAILED", r.insertShown ? "shown" : "NOT shown");
    verdict("d", works,
            "which interfaces of the source ItemsSourceView (ItemsRepeater, ItemsView) and ListView ask for (own COM source)",
            observed, conclusion);

    int const foreign = r.listFamily.foreign + r.repeaterFamily.foreign + r.itemsViewFamily.foreign;
    verdict("d2", foreign == 0,
            "does XAML touch the source or its slots from another thread (closing the window with a list open "
            "included)",
            strf("%d calls from another thread (ListView %d, ItemsRepeater %d, ItemsView %d)", foreign,
                   r.listFamily.foreign.load(), r.repeaterFamily.foreign.load(), r.itemsViewFamily.foreign.load()),
            foreign == 0 ? "everything came on the UI thread: a non-interlocked count is safe as far as this run shows"
                         : "XAML crosses threads: the count of the source and the slots must stay interlocked");
}

// ---- (е) дочерней программой -------------------------------------------------------------------------------------

constexpr char const* questionE =
    "can XAML run on a thread without Application::Start and without a shown window, for a ctest test";

void runWindowless() {
    std::wstring const directory = exeDirectoryW();
    std::wstring const path = directory + L"sandbox.list-binding-windowless.exe";
    if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        verdict("e", false, questionE, "sandbox.list-binding-windowless.exe is not beside the probe",
                "build the target sandbox.list-binding-windowless (the probe target depends on it)");
        return;
    }

    SECURITY_ATTRIBUTES security {sizeof security, nullptr, TRUE};
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!::CreatePipe(&readEnd, &writeEnd, &security, 1 << 20)) {
        verdict("e", false, questionE, strf("CreatePipe failed: %lu", ::GetLastError()), "not answered");
        return;
    }
    ::SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup {};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;
    PROCESS_INFORMATION process {};
    std::wstring command = L"\"" + path + L"\"";
    BOOL const started = ::CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                          directory.c_str(), &startup, &process);
    ::CloseHandle(writeEnd);
    if (!started) {
        ::CloseHandle(readEnd);
        verdict("e", false, questionE, strf("CreateProcess failed: %lu", ::GetLastError()), "not answered");
        return;
    }

    bool const timedOut = ::WaitForSingleObject(process.hProcess, 90000) == WAIT_TIMEOUT;
    if (timedOut) {
        ::TerminateProcess(process.hProcess, 0xDEAD);
        ::WaitForSingleObject(process.hProcess, 5000);
    }
    std::string output;
    char buffer[4096];
    DWORD read = 0;
    while (::ReadFile(readEnd, buffer, sizeof buffer, &read, nullptr) && read > 0) output.append(buffer, read);
    DWORD code = 0;
    ::GetExitCodeProcess(process.hProcess, &code);
    ::CloseHandle(process.hThread);
    ::CloseHandle(process.hProcess);
    ::CloseHandle(readEnd);

    bool answered = false;
    size_t begin = 0;
    while (begin < output.size()) {
        size_t end = output.find('\n', begin);
        if (end == std::string::npos) end = output.size();
        std::string line = output.substr(begin, end - begin);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        begin = end + 1;
        if (line.rfind("(e)", 0) == 0) {
            say("%s", line.c_str());
            verdicts.push_back({line, line.find("[SURPRISE]") != std::string::npos});
            answered = true;
        } else if (!line.empty()) {
            say("    (e) windowless| %s", line.c_str());
        }
    }
    if (timedOut) {
        verdict("e", false, questionE, "the helper program hung for 90 s and was stopped", "not answered");
    } else if (!answered) {
        verdict("e", false, questionE, strf("the helper program ended with 0x%08lX and no answer", code),
                "it crashed: the lines above are how far it got");
    }
}

// ---- окно, порядок вопросов ---------------------------------------------------------------------------------------

void closeWindow() {
    auto& r = run();
    if (r.finished) return;
    r.finished = true;
    r.script.stop();
    if (r.guard) r.guard.Stop();
    r.window.Close();
    xaml::Application::Current().Exit();
}

void plan(Script& script) {
    // (а)
    script.once(800, [] { buildA(); });
    script.add(900, [] { return sweepA(); });
    script.once(800, [] { reportA(); });
    // (б)
    script.once(0, [] { buildB(); });
    script.add(900, [] { return sweepB(); });
    script.once(800, [] { run().bFirstLook = lookB(); });
    script.once(1500, [] { reportB(); });
    // (в)
    script.once(0, [] { buildV(); });
    script.once(900, [] { beforeInsertV(); });
    script.once(0, [] { insertV(); });
    script.once(900, [] { afterInsertV(); });
    script.once(500, [] { keyV(); });
    script.once(600, [] { reportV(); });
    // (г)
    script.once(0, [] { deadG(); });
    script.once(500, [] { bornG(); });
    script.once(1500, [] { reportG(); });
    // Закрытие окна с открытым списком -- для (d2): кто и из какого потока отпускает источник.
    script.once(0, [] {
        auto& r = run();
        if (!r.vSource) return;
        r.closeList = controls::ListView {};
        r.closeList.Width(300);
        r.closeList.Height(320);
        hookTemplate(r.closeList, &r.closing, &r.closingStats);
        r.closeList.ItemsSource(r.vSource->inspectable());
        r.window.Content(r.closeList);
    });
    script.once(800, [] { closeWindow(); });
}

}  // namespace

void start(WxlListMaker makeList) {
    logFile = ::_wfopen((exeDirectoryW() + L"list-binding-probe.txt").c_str(), L"w");
    current = new Run;
    auto& r = run();
    r.makeList = makeList;
    say("ListBindingProbe: one line per question, details indented");

    // Ошибка под XAML -- неожиданность, но не конец пробы: остальные вопросы ещё можно ответить.
    xaml::Application::Current().UnhandledException(
        [](wf::IInspectable const&, xaml::UnhandledExceptionEventArgs const& args) {
            ++unhandledErrors;
            say("    XAML unhandled error 0x%08X: %s", static_cast<uint32_t>(args.Exception().value),
                winrt::to_string(args.Message()).c_str());
            args.Handled(true);
        });

    r.window = xaml::Window {};
    r.window.Title(L"ListBindingProbe");
    r.window.AppWindow().Resize({1300, 760});
    r.window.Activate();

    plan(r.script);
    r.script.start();

    // Не висеть: всё вместе -- секунд двадцать.
    r.guard = dispatching::DispatcherQueue::GetForCurrentThread().CreateTimer();
    r.guard.IsRepeating(false);
    r.guard.Interval(std::chrono::seconds {150});
    r.guard.Tick([](auto&&, auto&&) {
        verdict("timeout", false, "did the scenario finish", "it ran for 150 s", "the window was closed by the guard");
        closeWindow();
    });
    r.guard.Start();
}

int finish() {
    if (!current) return 1;
    reportD();
    runWindowless();
    if (unhandledErrors > 0) {
        verdict("xaml", false, "did XAML raise unhandled errors", strf("%d (lines above)", unhandledErrors),
                "the answers next to them may be incomplete");
    }

    int surprises = 0;
    say("== summary ==");
    for (auto const& entry : verdicts) {
        say("%s", entry.line.c_str());
        if (entry.surprise) ++surprises;
    }
    say("surprises: %d (the exit code); log: %slist-binding-probe.txt", surprises, exeDirectory().c_str());
    if (logFile) std::fclose(logFile);
    logFile = nullptr;
    return surprises;
}

}  // namespace probe
