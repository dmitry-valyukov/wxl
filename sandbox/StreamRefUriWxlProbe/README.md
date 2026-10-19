# StreamRefUriWxlProbe

Те же шаги, что у `sandbox/StreamRefUriProbe` (голый cppwinrt), но через обёртки
wxl: `RandomAccessStreamReference::createFromUri` / `createFromFile`,
`RandomAccessStreamReference::openReadAsync` (`wxl::Operation`), `DataPackage`,
`Clipboard`, `DataPackageView::getBitmapAsync`, `BitmapImage::setSourceAsync`
(поток разбирается как картинка) — так, как их зовёт пример Gallery.
Если голый cppwinrt читает ссылку, а обёртка нет, дефект в обёртке; здесь
ответы совпали до HRESULT.

## Результат

Windows 11 (10.0.26200), приложение без идентификатора пакета, Debug. Строки те же,
что у голой пробы, и подписаны так же:

* `createFromUri` с `file:///M:/…` и `file://localhost/M:/…` — `openReadAsync`
  завершается `0x80004001` с тем же текстом о поддерживаемых схемах; `ms-appx:///` —
  `0x80070057`; `http://127.0.0.1:порт/…` — читается и разбирается как картинка;
* `createFromFile` над `StorageFile::getFileFromPathAsync` — читается, путь с прямыми
  слэшами — `0x800700A1` уже на `getFileFromPathAsync`;
* через буфер: `Clipboard::setContent` и `Clipboard::setContentWithOptions` с параметрами
  по умолчанию (как в примере) для `file:///` проходят, `DataPackageView::getBitmapAsync` отдаёт
  ссылку, `openReadAsync` над ней — `0x80004001` и при первой, и при второй вставке;
  с `Clipboard::flush` — `getBitmapAsync` падает с `0x800401D3`; `http` и `createFromFile` читаются
  при любом способе положить.

Исключение `Operation` доходит до `co_await` тем же `winrt::hresult_error`, что и в голой
пробе, код и текст не меняются.

## Запуск

Приложение на wxl: точка входа — `wxl_launched()` в `main.cpp`, окна нет, кроме простого
окна с надписью (`Clipboard` в классическом приложении работает у окна переднего плана,
проба берёт его сама). **Сценарий перезаписывает содержимое буфера обмена.** Занимает
около минуты, потом окно закрывается. Журнал — `stream-ref-uri-wxl-probe.log` рядом
с исполняемым файлом. Картинка от системного кодировщика, сервер на петле и разбор
исключения — на cppwinrt, в `server.cpp`: это не предмет пробы.
