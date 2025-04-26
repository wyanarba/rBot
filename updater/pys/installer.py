import os
from datetime import datetime

def log_message(message : str):
    log = open("..\\log.txt", "a")
    t = datetime.now()
    log.write(f"[{t.year}-{t.month}-{t.day} | {t.hour}:{t.minute}:{t.second} v2]\t{message}\n")

disk_name = "C"

try:
    file = open(r"..\1_SYSTEM_DISK_NAME.txt", "r")
    disk_name = file.readline().strip()
except Exception as e:
    log_message(f"ERROR_1: {type(e).__name__}: {e}")

try:
    file = open(r"bot_run.bat", "w")
    file.write(f"cd ..\\..\\bot\\\nstart {disk_name}:\Windows\System32\conhost.exe raspisBot.exe")
    file.close()
    os.system("bot_run.bat")
    log_message("Bot started!!!")
except Exception as e:
    log_message(f"ERROR_2: {type(e).__name__}: {e}")

