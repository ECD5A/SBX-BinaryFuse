<div align="center">

# SBX Binary Fuse16

Компактный статический Binary Fuse-фильтр для быстрых проверок 64-битных ключей.

![License](https://img.shields.io/badge/license-MIT-2ea44f?style=for-the-badge)
![C++](https://img.shields.io/badge/core-C%2B%2B11-f39c12?style=for-the-badge)
![Keys](https://img.shields.io/badge/keys-64--bit-00b894?style=for-the-badge)
![Fingerprint](https://img.shields.io/badge/fingerprint-16--bit-00d9ff?style=for-the-badge)

[English version](README.md)

</div>

![Схема SBX Binary Fuse16](assets/binary-fuse-flow.svg)

## Что Это

SBX Binary Fuse16 - собственная реализация статического вероятностного фильтра
принадлежности для 64-битных ключей. Фильтр рассчитан на задачи, где набор один
раз строится, а затем выполняется большое количество read-only проверок.

Реализация использует 16-битные отпечатки и три позиции на ключ. Положительный
ответ означает "возможно присутствует", а отрицательный является точным.
Ожидаемая вероятность false positive составляет примерно `1 / 65 536` при
равномерном распределении входного 64-битного хеша.

## Как Это Работает

1. Каждый ключ смешивается с детерминированным seed построения.
2. Полученный хеш выбирает три вершины сегментированного 3-wise графа.
3. Вершины степени один последовательно снимаются в peel-стек.
4. Отпечатки назначаются в обратном порядке peel-стека.
5. Lookup читает три 16-битные ячейки и складывает их по XOR с отпечатком ключа.

Если граф нельзя полностью разобрать, построение повторяется с другим
детерминированным seed. Для повторяющихся ключей предусмотрен резервный проход
с сортировкой и удалением дубликатов.

## Устройство

- 64-битные входные ключи.
- 16-битные отпечатки.
- Три позиции таблицы на одну lookup-проверку.
- Сегментированная раскладка Binary Fuse.
- Детерминированная последовательность seed и ограниченное число попыток.
- Inline lookup-путь в публичном заголовочном файле.
- Поддержка малых наборов и повторяющихся ключей.
- Без внешних runtime-зависимостей.

## Сборка

```bash
make
```

Будет собрана статическая библиотека:

```text
libsbx-binary-fuse.a
```

Ручная сборка:

```bash
g++ -O3 -std=c++11 -c binary_fuse.cpp -o binary_fuse.o
ar rcs libsbx-binary-fuse.a binary_fuse.o
```

Удаление артефактов сборки:

```bash
make clean
```

## Публичный API

```cpp
int sbx_binary_fuse16_build(SbxBinaryFuse16 *filter,
                            uint64_t *keys,
                            uint32_t count);

int sbx_binary_fuse16_contains(const SbxBinaryFuse16 *filter,
                               uint64_t key);

uint64_t sbx_binary_fuse16_estimate_bytes(uint32_t count);
void sbx_binary_fuse16_free(SbxBinaryFuse16 *filter);
```

`sbx_binary_fuse16_build()` возвращает `1` при успехе и `0` при ошибке памяти
или построения. Входной массив является изменяемым: резервная обработка
дубликатов может отсортировать и уплотнить его.

## Минимальный Пример

```cpp
#include "binary_fuse.h"

#include <cstdint>
#include <cstdio>

int main() {
  uint64_t keys[] = {
    UINT64_C(0x0123456789abcdef),
    UINT64_C(0xfedcba9876543210),
    UINT64_C(0x1122334455667788)
  };

  SbxBinaryFuse16 filter = {};
  if(!sbx_binary_fuse16_build(&filter, keys, 3)) {
    return 1;
  }

  uint64_t query = UINT64_C(0x0123456789abcdef);
  std::puts(sbx_binary_fuse16_contains(&filter, query)
              ? "possibly present"
              : "not present");

  sbx_binary_fuse16_free(&filter);
  return 0;
}
```

Сборка примера:

```bash
g++ -O3 -std=c++11 example.cpp binary_fuse.cpp -o example
```

## Модель Памяти

Готовый фильтр хранит один непрерывный массив `uint16_t` и небольшую структуру
метаданных. До построения можно получить оценку размера:

```cpp
uint64_t bytes = sbx_binary_fuse16_estimate_bytes(key_count);
```

Оценка относится к финальному массиву отпечатков. Во время построения также
используются временные массивы степеней, XOR-значений, очереди и peel-стека.
Они освобождаются до возврата из `sbx_binary_fuse16_build()`.

## Многопоточность

После успешного построения параллельные вызовы `contains()` безопасны, пока
фильтр остаётся read-only. Одновременное построение или освобождение одного и
того же фильтра не поддерживается.

## Ограничения

- Фильтр статический: добавление и удаление требуют нового построения.
- Проверка вероятностная: возможны false positive.
- Количество входных ключей хранится в `uint32_t`.
- Память отпечатков принадлежит фильтру и освобождается через
  `sbx_binary_fuse16_free()`.
- Копирование построенного `SbxBinaryFuse16` по значению не копирует выделенную
  память.

## Алгоритм

В основе лежит Binary Fuse-конструкция Томаса Мюллера Графа и Дэниела Лемира,
описанная в работе
[Binary Fuse Filters: Fast and Smaller Than Xor Filters](https://arxiv.org/abs/2201.01174).
Репозиторий содержит собственную MIT-реализацию и не включает исходный код
авторов статьи.

## Поддержать Проект

Если SBX Binary Fuse16 полезен в вашей работе, вы можете поддержать проект:

**Bitcoin (BTC):** `1ECDSA1b4d5TcZHtqNpcxmY8pBH1GgHntN`

**USDT (TRC20):** `TUF4vPdB6QkjCvZq18rBL4Qj4dK5ihCN75`

## Лицензия

MIT License. См. [LICENSE](LICENSE).
