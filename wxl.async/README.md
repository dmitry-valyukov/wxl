# wxl.async

Асинхронная подсистема: корутины `task` и `detached_task`, диспетчер
`sta_loop` с неблокирующим файловым вводом-выводом, отмена операций,
lock-free очереди и каналы, `future`, потоки и компонентная модель. Модуль
для подключения: `import wxl.async;`, цель сборки: `wxl::async`. Устройство —
[docs/async.md](../docs/async.md) и [docs/lockfree.md](../docs/lockfree.md).
