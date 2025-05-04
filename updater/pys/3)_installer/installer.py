import os
from datetime import datetime

def log_message(message : str):
    log = open("..\\..\\log.txt", "a")
    t = datetime.now()
    log.write(f"[{t.year}-{t.month}-{t.day} | {t.hour}:{t.minute}:{t.second} v3]\t{message}\n")

log_message(f"Установка")
main_folder = "..\\..\\..\\"

# Чтение конфига
def get_params(file_path: str, mode = False):
    with open(file_path, "r") as file:
        lines = file.read().splitlines()

    lines_2 = [line.split("#")[0].strip() for line in lines]
    result = [line.split("=")[0].strip() for line in lines_2]
    if mode:
        return result, lines, lines_2
    else:
        return result, lines

# Запись новых параметров в конфиг
try:
    # чтение данных из конфигов
    conf_params, conf_lines, conf_lines_2 = get_params(main_folder + "config.txt", True)
    new_params, new_lines = get_params(main_folder + "new_config.txt")
    last_param = 0

    # поиск последней настройки
    for i in range(len(conf_lines_2) - 1, -1, -1):
        if " = " in conf_lines_2[i]:
            last_param = i
            break

    # добавление новых настроек
    i = 0
    for param in new_params:
        if param not in conf_params:
            last_param += 1
            conf_lines.insert(last_param, "#NEW!!!")
            last_param += 1
            conf_lines.insert(last_param, new_lines[i])

        i += 1

    # запись конфига и удаление лишнего файла
    with open(main_folder + "config.txt", "w") as file:
        file.write("\n".join(conf_lines))

    os.remove(main_folder + "new_config.txt")


except Exception as e:
    log_message(f"ERROR_1: {type(e).__name__}: {e}")



# запуск бота
disk_name = "C"

# получение буквы системного диска
try:
    file = open(r"..\..\1_SYSTEM_DISK_NAME.txt", "r")
    disk_name = file.readline().strip()
except Exception as e:
    log_message(f"ERROR_2: {type(e).__name__}: {e}")

# запись батника и старт
try:
    file = open(r"bot_run.bat", "w")
    file.write(f"cd {main_folder}bot\\\nstart {disk_name}:\Windows\System32\conhost.exe raspisBot.exe")
    file.close()
    os.system("bot_run.bat")
    log_message("Bot started!!!")
except Exception as e:
    log_message(f"ERROR_3: {type(e).__name__}: {e}")