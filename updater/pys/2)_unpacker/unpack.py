import os
import shutil
from datetime import datetime
import subprocess
import stat
import time

def log_message(message : str):
    log = open("..\\..\\log.txt", "a")
    t = datetime.now()
    log.write(f"[{t.year}-{t.month}-{t.day} | {t.hour}:{t.minute}:{t.second} v2]\t{message}\n")

def remove_readonly(func, path, _):
    """
    Функция для обработки ошибок при удалении.
    Снимает атрибут "только для чтения" и повторяет операцию.
    """
    try:
        # Снимаем атрибут "только для чтения"
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except Exception as e:
        print(f"Не удалось удалить файл/папку '{path}': {e}")

def force_remove_folder(folder_path):
    """
    Принудительно удаляет папку, даже если файлы в ней имеют атрибут "только для чтения".
    """
    if not os.path.exists(folder_path):
        print(f"Папка '{folder_path}' не существует.")
        return

    try:
        # Удаляем папку с обработкой ошибок
        shutil.rmtree(folder_path, onerror=remove_readonly)
        print(f"Папка '{folder_path}' успешно удалена.")
    except Exception as e:
        print(f"Ошибка при удалении папки '{folder_path}': {e}")

def extract_and_replace_to(source_folder, destination_folder):
    """
    Перемещает файлы из source_folder в destination_folder, обновляя существующие файлы.
    Папка source_folder удаляется после выполнения. Ошибки доступа игнорируются с выводом в консоль.

    Args:
        source_folder (str): Путь к исходной папке.
        destination_folder (str): Путь к целевой папке.

    Returns:
        bool: True, если все файлы перемещены и source_folder удалена, False, если были ошибки.
    """
    source_folder = os.path.abspath(source_folder)
    destination_folder = os.path.abspath(destination_folder)
    success = True

    if not os.path.exists(source_folder):
        log_message(f"Error: Source folder does not exist: {source_folder}")
        return False

    for root, dirs, files in os.walk(source_folder, topdown=False):
        for file in files:
            src_file = os.path.join(root, file)
            relative_path = os.path.relpath(src_file, source_folder)
            dst_file = os.path.join(destination_folder, relative_path)
            dst_folder = os.path.dirname(dst_file)

            try:
                os.makedirs(dst_folder, exist_ok=True)

                if os.path.exists(dst_file):
                    os.remove(dst_file)

                shutil.move(src_file, dst_file)
                log_message(f"Moved: {src_file} -> {dst_file}")

            except (OSError, shutil.Error, PermissionError) as e:
                log_message(f"Error moving {src_file} to {dst_file}: {type(e).__name__}: {e}")
                success = False

        # Попробуем удалить пустые подпапки
        for dir in dirs:
            dir_path = os.path.join(root, dir)
            try:
                if os.path.isdir(dir_path) and not os.listdir(dir_path):
                    os.rmdir(dir_path)
                    log_message(f"Removed empty folder: {dir_path}")
            except Exception as e:
                log_message(f"Error removing directory {dir_path}: {type(e).__name__}: {e}")
                success = False

        # Удаляем текущую папку, если она пуста
        try:
            if os.path.isdir(root) and not os.listdir(root):
                os.rmdir(root)
                log_message(f"Removed empty folder: {root}")
        except Exception as e:
            log_message(f"Error removing directory {root}: {type(e).__name__}: {e}")
            success = False

    # Финальная попытка удалить исходную папку (если осталась)
    if os.path.isdir(source_folder):
        try:
            os.rmdir(source_folder)
            log_message(f"Removed source folder: {source_folder}")
        except Exception as e:
            log_message(f"Error removing source folder {source_folder}: {type(e).__name__}: {e}")
            success = False

    return success



log_message(f"Распаковка")
main_folder = "..\\..\\..\\"

# чтение файлов
with open("black_files.txt", "r") as file:
    black_files = file.read().splitlines()

with open("black_folders.txt", "r") as file:
    black_folders = file.read().splitlines()


# удаление файлов
for file in black_files:
    try:
        os.remove(main_folder + "rBot\\" + file)
    except Exception as e:
        log_message(f"ERROR_1: {type(e).__name__}: {e} file: {file}")


# удаление папок
for folder in black_folders:
    try:
        force_remove_folder(main_folder + "rBot\\" + folder)
    except Exception as e:
        log_message(f"ERROR_2: {type(e).__name__}: {e} folder: {folder}")


# распаковка архива
try:
    extract_and_replace_to(main_folder + "rBot", main_folder)
except Exception as e:
    log_message(f"ERROR_3: {type(e).__name__}: {e}")


# следующий модуль
os.system("installer.bat")