# /// script
# dependencies = [
#   "pillow",
# ]
# ///

import sys
from pathlib import Path
from PIL import Image

def main():
    # Проверяем, передан ли аргумент
    if len(sys.argv) < 2:
        print("Ошибка: Не указан путь к исходному PNG файлу.")
        print("Использование: uv run png2ico.py <путь_к_файлу.png>")
        sys.exit(1)

    # Работаем с путями через Path
    input_path = Path(sys.argv[1])
    
    if not input_path.exists():
        print(f"Ошибка: Файл не найден: {input_path}")
        sys.exit(1)

    # Формируем имя выходного файла (.ico вместо .png)
    output_path = input_path.with_suffix(".ico")

    try:
        img = Image.open(input_path)
        
        # Набор MIP-текстур для Windows 10/11
        icon_sizes = [(16, 16), (32, 32), (48, 48), (256, 256)]
        
        # Запекаем контейнер
        img.save(output_path, sizes=icon_sizes)
        print(f"Успешно: {input_path.name} -> {output_path.name}")
        
    except Exception as e:
        print(f"Ошибка при конвертации: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
