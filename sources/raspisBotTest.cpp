
//54
#include "functions.h"
#include "raspisCore.h"


struct myUser
{
    int group = -1;//группа
    string Tea;//преподаватель
    char mode = 0;//режим
    int64_t tgId = 0;//тг id
    //ну а хотя зачем тут эти комментарии????
};

struct myCommand //костыль для удобства
{
    string command;
    string description;

    myCommand(const string& command, const string& description) : command(command), description(description) {}
};


std::vector<myUser> subscribedUsers;  // Список подписанных пользователей
set <int64_t> mutedUsers;
set <int64_t> blockedUsers;


//список команд
vector<myCommand> Commands = {
    {"start", "Обновить клавиатуру с кнопками"},
    {"status", "Состояние подписки"},
    {"un_button", "Удалить клавиатуру с кнопками"},
    {"sub_o", "Подписаться на общее расписание"},
    {"sub_g", "Подписаться на расписание группы"},
    {"sub_go", "Подписаться на отдельное расписание группы"},
    {"sub_p", "Подписаться на расписание преподавателя"},
    {"t_dop", "Получать расписание и для другого корпуса"},
    {"get_o", "Получить общее расписание"},
    {"get_g", "Получить расписание группы"},
    {"get_p", "Получить расписание преподавателя"},
    {"m", "Доп. подписка"},
    {"unm", "отписаться от доп. подписки"},
    {"unsubscribe", "Отписаться от расписания"}
};

//ошибки за которые бот отключает пользователя от расписания
vector <string> errorsForBan = {
    "bot was kicked",
    "user is deactivated",
    "bot was blocked by the user",
    "chat not found",
    "user not found",
    "Bot is not a member of the channel chat",
    "Group migrated to supergroup"
};


vector<int64_t> GroupsForSpam;//буферная группа для рассылки
int64_t RootTgId = 0;//тг id владельца
int64_t SecondRootTgId = 6266601544;//мой тг id, для прав чуть по ниже
string BotKey = "";//ключ бота
string StartText = "";//приветсвенное сообщение
bool ModeSend = 0;//режим отправки 0 - v1, 1 - v2
//DWORD SleepTime = 60000;
void (*update)();//функция для отправки расписания, (указатель) на неё
//bool EnableAd = 1;
//bool EnableAutoUpdate = 1;

string formatG(string str) {
    str = Utf8_to_cp1251(str.c_str());


    int size = str.size();
    if (size < 5)
        return(cp1251_to_utf8(str.c_str()));

    string postfix;
    {
        int i = 1;

        while (i <= size && !(str[size - i] > -1 || str[size - i] < -64)) {
            if (str[size - i] < -32)
                str[size - i] += 32;
            i++;
        }

        postfix = str.substr(size - i + 1, i - 1);
        str = str.substr(0, size - i + 1);
        size = str.size();
    }

    if (str.find('-') == string::npos)
        str.insert(size - 3, 1, '-');

    size = str.size();

    for (int i = 5; i <= size; i++) {
        if (str[size - i] <= -1 && str[size - i] >= -64) {
            if (str[size - i] > -33)
                str[size - i] -= 32;
        }
    }

    str += postfix;


    return(cp1251_to_utf8(str.c_str()));
}

string formatT(string str) {
    str = Utf8_to_cp1251(str.c_str());
    str.erase(std::remove(str.begin(), str.end(), 32), str.end());
    str.erase(std::remove(str.begin(), str.end(), 46), str.end());

    if (str.size() < 2)
        return(cp1251_to_utf8(str.c_str()));

    if (str[0] > -33 && str[0] <= -1)
        str[0] -= 32;

    for (int i = 1; i < str.size() - 2; i++)
        if (str[i] <= -33 && str[i] >= -64)
            str[i] += 32;

    if (str[str.size() - 1] > -33 && str[str.size() - 1] <= -1)
        str[str.size() - 1] -= 32;

    if (str[str.size() - 2] > -33 && str[str.size() - 2] <= -1)
        str[str.size() - 2] -= 32;

    str.insert(str.end() - 1, 46);
    str.insert(str.end(), 46);

    return(cp1251_to_utf8(str.c_str()));
}

std::string escapeMarkdownV2(const std::string& text) {
    static const std::regex specialChars(R"([_*\[\]()~`>#+\-={}.!])");
    return std::regex_replace(text, specialChars, R"(\$&)");
}

bool getConfig(string confName) {
    ifstream configIfs(confName);
    string line;

    if (!configIfs.is_open())
        return 0;

    while (getline(configIfs, line)) {
        size_t commPointer = line.find("#");

        if (commPointer != string::npos)
            line = line.substr(0, commPointer);

        size_t equPointer = line.find(" = ");

        if (equPointer != string::npos) {
            string parameter = line.substr(0, equPointer), value = line.substr(equPointer + 3);

            if (parameter == "" || value == "")
                continue;

            if (parameter == "BotKey")
                BotKey = value;
            else if (parameter == "RootTgId")
                RootTgId = stoll(value);
            else if (parameter == "GroupId" && GroupsForSpam.size() < 11)
                GroupsForSpam.push_back(stoll(value));
            else if (parameter == "Mode")
                ModeSend = value == "1";
            else if (parameter == "StartText")
                StartText = value;
            else if (parameter == "SleepTime")
                SleepTime = stoi(value) * 1000;
            else if (parameter == "EnableAd")
                EnableAd = value == "1";
            else if (parameter == "EnableAutoUpdate")
                EnableAutoUpdate = value == "1";
        }
    }

    if (BotKey == "")
        return 0;

    if (ModeSend && GroupsForSpam.size() == 0)
        return 0;

    if (StartText == "")
        StartText = "Бот с расписанием VKSIT!";

    return 1;
}

void saveUsers() {

    for (int i = 0; i < DisabledGroups.size(); i++)
        DisabledGroups[i] = 0;

    std::ofstream outputFile("..\\users.txt");
    for (const auto& us : subscribedUsers) {
        if (us.mode != 2 && us.mode != 3)
            outputFile << us.tgId << ' ' << us.group << ' ' << (char)(us.mode + '0') << endl;  // Записываем оставшиеся строки
        else
            outputFile << us.tgId << ' ' << us.Tea << ' ' << (char)(us.mode + '0') << endl;  // Записываем оставшиеся строки

        if (us.mode < 2 && us.group != -1 && DisabledGroups[us.group] == 0)
            DisabledGroups[us.group] = 1;
    }
    outputFile.close();  // Закрываем файл
}

//0 - мут, 1 - бан
void saveBadUsers(int8_t mode) {
    if (mode == 0) {
        std::ofstream outputFile("..\\mut.txt");
        for (const auto& us : mutedUsers) {
            outputFile << us << endl;
        }
        outputFile.close();  // Закрываем файл
    }
    else if (mode == 1) {
        std::ofstream outputFile("..\\ban.txt");
        for (const auto& us : blockedUsers) {
            outputFile << us << endl;
        }
        outputFile.close();  // Закрываем файл
    }
}

bool IsNormalCfg = getConfig("..\\config.txt");
TgBot::Bot bot(BotKey);

int chooseGS(int id, __int8 mode) {//выбрать GroupsForSpam
    for (int i = 0; i < edgeGroups[mode].size(); i++) {
        if (id <= edgeGroups[mode][i])
            return i;
    }
    return GroupsForSpam.size() - 1;
}

void updateV2() {

    try
    {
        //ожидание сообщения об отправке от ядра
        {
            mtx1.lock();
            if (syncMode == 1) {

                try {
                    if (isUpdate && RootTgId != 0) {
                        bot.getApi().sendMessage(RootTgId, "Обнова (качается и устанавливается сама)\n" + CurrentVers + " -> " + newVersion +
                            "\n\nПодробнее об оновлении:\nhttps://t.me/backgroundbotvksit", false, 0, NULL);
                    }
                }
                catch (const std::exception& e)
                {
                    string str = e.what();
                    logMessage(std::format("Error: Не удалось скинуть сообщение о обновлении - {}", e.what()), "system", 222);
                }

                bool wait = 1;
                syncMode = 2;

                mtx1.unlock();

                while (wait) {
                    this_thread::sleep_for(300ms);
                    mtx1.lock();
                    wait = syncMode != 3;
                    mtx1.unlock();
                }
            }
            else {
                mtx1.unlock();
                return;
            }
        }



        int32_t mi1, mi2;//messageId для основного и доп фото
        int triesToSend = 0;//попытки отправки одному человеку

        if (!ErrorOnCore) {//расписание успешно обработано

            logMessage("Отправка расписания в группу", "system", 3);
            //отправка в группу
            {
                int countG = 0, localCount = 0;
                edgeGroups[0].clear();

                for (auto& a : GroupsB[ModeS]) {//распределение 
                    if (a.isExists)
                        countG++;
                }

                countG = countG / GroupsForSpam.size() + 1;

                for (int i = 0; i < GroupsB[ModeS].size(); i++) {
                    if (GroupsB[ModeS][i].isExists)
                        localCount++;

                    if (localCount == countG) {
                        localCount = 0;
                        edgeGroups[0].push_back(i);
                    }
                }

                bool success = 0;

                for (int i = 0; i < Groups.size(); i++) {
                    try
                    {
                        if (GroupsB[ModeS][i].isExists) {
                            auto message = bot.getApi().sendPhoto(GroupsForSpam[chooseGS(i, 0)], TgBot::InputFile::fromFile(ModeStr + "\\" + Groups1251[i] + ".png", "image/png"));
                            GroupsB[ModeS][i].messageId = message->messageId;

                            if (IsNewRaspis[0] || AltGroupsB[0][i]) {
                                message = bot.getApi().sendPhoto(GroupsForSpam[chooseGS(i, 0)], TgBot::InputFile::fromFile(ModeStr + "\\" + Groups1251[i] + "S.png", "image/png"));
                                GroupsB[ModeS][i].messageIdS = message->messageId;
                            }
                        }
                    }
                    catch (const std::exception& e)
                    {
                        logMessage(std::format("Error: {} | {}", e.what(), Groups[i]), "system", 4);
                        i--;
                    }

                }
                while (!success) {
                    try
                    {
                        auto message = bot.getApi().sendPhoto(GroupsForSpam[GroupsForSpam.size() - 1], TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                        mi1 = message->messageId;
                        success = 1;
                    }
                    catch (const std::exception& e)
                    {
                        logMessage(std::format("Error: {}", e.what()), "system", 5);
                    }
                }




                if (Mode > 2) {
                    countG = 0, localCount = 0;
                    edgeGroups[1].clear();

                    for (auto& a : GroupsB[ModeVs]) {//распределение 
                        if (a.isExists)
                            countG++;
                    }

                    countG = countG / GroupsForSpam.size() + 1;

                    for (int i = 0; i < GroupsB[ModeVs].size(); i++) {
                        if (GroupsB[ModeVs][i].isExists)
                            localCount++;

                        if (localCount == countG) {
                            localCount = 0;
                            edgeGroups[1].push_back(i);
                        }
                    }


                    success = 0;

                    for (int i = 0; i < Groups.size(); i++) {
                        try
                        {
                            if (GroupsB[ModeVs][i].isExists) {
                                auto message = bot.getApi().sendPhoto(GroupsForSpam[chooseGS(i, 1)], TgBot::InputFile::fromFile(AltModeStr + "\\" + Groups1251[i] + ".png", "image/png"));
                                GroupsB[ModeVs][i].messageId = message->messageId;

                                if (IsNewRaspis[1] || AltGroupsB[1][i]) {
                                    message = bot.getApi().sendPhoto(GroupsForSpam[chooseGS(i, 1)], TgBot::InputFile::fromFile(AltModeStr + "\\" + Groups1251[i] + "S.png", "image/png"));
                                    GroupsB[ModeVs][i].messageIdS = message->messageId;
                                }
                            }
                        }
                        catch (const std::exception& e)
                        {
                            logMessage(std::format("Error: {}", e.what()), "system", 6);
                            i--;
                        }
                    }

                    while (!success) {
                        try
                        {
                            auto message = bot.getApi().sendPhoto(GroupsForSpam[GroupsForSpam.size() - 1], TgBot::InputFile::fromFile(AltModeStr + ".png", "image/png"));
                            mi2 = message->messageId;
                            success = 1;
                        }
                        catch (const std::exception& e)
                        {
                            logMessage(std::format("Error: {}", e.what()), "system", 7);
                        }
                    }

                }
            }


            logMessage("Отправка расписания людям", "system", 8);
            //рассылка
            if (Mode < 3) {
                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        if (subscribedUsers[i].group == -1) {
                            bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[GroupsForSpam.size() - 1], mi1);
                        }
                        else {
                            if (subscribedUsers[i].mode == 0 && GroupsB[ModeS][subscribedUsers[i].group].isExists)
                                bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[chooseGS(subscribedUsers[i].group, 0)], GroupsB[ModeS][subscribedUsers[i].group].messageId);

                            else if (subscribedUsers[i].mode == 1 && GroupsB[ModeS][subscribedUsers[i].group].isExists) {
                                if (IsNewRaspis[0] || AltGroupsB[0][subscribedUsers[i].group])
                                    bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[chooseGS(subscribedUsers[i].group, 0)], GroupsB[ModeS][subscribedUsers[i].group].messageIdS);
                            }


                            else if (subscribedUsers[i].mode == 2 || subscribedUsers[i].mode == 3) {
                                if (Teachers[ModeS].find(subscribedUsers[i].Tea) != Teachers[ModeS].end())
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Utf8_to_cp1251(subscribedUsers[i].Tea.c_str()) + ".png", "image/png"));
                                else if (subscribedUsers[i].mode == 2)
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                            }
                        }

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 9);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 10);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 11);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 12);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }
            else {

                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        if (subscribedUsers[i].group == -1) {
                            bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[GroupsForSpam.size() - 1], mi1);
                            bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[GroupsForSpam.size() - 1], mi2);
                        }
                        else {
                            if (subscribedUsers[i].mode == 0) {
                                if (GroupsB[ModeS][subscribedUsers[i].group].isExists)
                                    bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[chooseGS(subscribedUsers[i].group, 0)], GroupsB[ModeS][subscribedUsers[i].group].messageId);

                                if (GroupsB[ModeVs][subscribedUsers[i].group].isExists)
                                    bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[chooseGS(subscribedUsers[i].group, 1)], GroupsB[ModeVs][subscribedUsers[i].group].messageId);
                            }


                            else if (subscribedUsers[i].mode == 1) {
                                if (GroupsB[ModeS][subscribedUsers[i].group].isExists && (IsNewRaspis[0] || AltGroupsB[0][subscribedUsers[i].group]))
                                    bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[chooseGS(subscribedUsers[i].group, 0)], GroupsB[ModeS][subscribedUsers[i].group].messageIdS);

                                if (GroupsB[ModeVs][subscribedUsers[i].group].isExists && (IsNewRaspis[1] || AltGroupsB[1][subscribedUsers[i].group]))
                                    bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[chooseGS(subscribedUsers[i].group, 1)], GroupsB[ModeVs][subscribedUsers[i].group].messageIdS);
                            }


                            else if (subscribedUsers[i].mode == 2 || subscribedUsers[i].mode == 3) {
                                if (Teachers[ModeS].find(subscribedUsers[i].Tea) != Teachers[ModeS].end()) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Utf8_to_cp1251(subscribedUsers[i].Tea.c_str()) + ".png", "image/png"));
                                }
                                else if (subscribedUsers[i].mode == 2) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                                }

                                if (Teachers[ModeVs].find(subscribedUsers[i].Tea) != Teachers[ModeVs].end()) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + "\\" + Utf8_to_cp1251(subscribedUsers[i].Tea.c_str()) + ".png", "image/png"));
                                }
                                else if (subscribedUsers[i].mode == 2) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + ".png", "image/png"));
                                }

                            }
                        }

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 13);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 14);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 15);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 16);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }

            //удаление из группы
            {
                int tryingToDelete = 0;
                bool success = 0;

                for (int i = 0; i < Groups.size(); i++) {
                    try
                    {
                        if (GroupsB[ModeS][i].isExists) {
                            bot.getApi().deleteMessage(GroupsForSpam[chooseGS(i, 0)], GroupsB[ModeS][i].messageId);
                            if (IsNewRaspis[0] || AltGroupsB[0][i])
                                bot.getApi().deleteMessage(GroupsForSpam[chooseGS(i, 0)], GroupsB[ModeS][i].messageIdS);

                            tryingToDelete = 0;
                        }
                    }
                    catch (const std::exception& e)
                    {
                        string str = e.what();
                        if (str.find("message to delete not found") == string::npos) {
                            tryingToDelete++;

                            logMessage(std::format("Error: {}", e.what()), "system", 17);
                            if (tryingToDelete < 10)
                                i--;
                            else
                                tryingToDelete = 0;
                        }
                    }

                }
                tryingToDelete = 0;
                while (!success) {
                    try
                    {
                        bot.getApi().deleteMessage(GroupsForSpam[GroupsForSpam.size() - 1], mi1);
                        success = 1;
                    }
                    catch (const std::exception& e)
                    {
                        string str = e.what();
                        if (str.find("message to delete not found") == string::npos) {
                            tryingToDelete++;

                            logMessage(std::format("Error: {}", e.what()), "system", 18);
                            if (tryingToDelete > 10)
                                success = 1;
                        }
                        else
                            success = 1;
                    }
                }




                if (Mode > 2) {
                    tryingToDelete = 0;
                    success = 0;

                    for (int i = 0; i < Groups.size(); i++) {
                        try
                        {
                            if (GroupsB[ModeVs][i].isExists) {
                                bot.getApi().deleteMessage(GroupsForSpam[chooseGS(i, 1)], GroupsB[ModeVs][i].messageId);
                                if (IsNewRaspis[1] || AltGroupsB[1][i])
                                    bot.getApi().deleteMessage(GroupsForSpam[chooseGS(i, 1)], GroupsB[ModeVs][i].messageIdS);

                                tryingToDelete = 0;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            string str = e.what();
                            if (str.find("message to delete not found") == string::npos) {
                                tryingToDelete++;

                                logMessage(std::format("Error: {}", e.what()), "system", 19);
                                if (tryingToDelete < 10)
                                    i--;
                                else
                                    tryingToDelete = 0;
                            }
                        }
                    }
                    tryingToDelete = 0;

                    while (!success) {
                        try
                        {
                            bot.getApi().deleteMessage(GroupsForSpam[GroupsForSpam.size() - 1], mi2);
                            success = 1;
                        }
                        catch (const std::exception& e)
                        {
                            string str = e.what();
                            if (str.find("message to delete not found") == string::npos) {
                                tryingToDelete++;

                                logMessage(std::format("Error: {}", e.what()), "system", 20);
                                if (tryingToDelete > 10)
                                    success = 1;
                            }
                            else
                                success = 1;
                        }
                    }

                }
            }

            //сохранение пользователей
            saveUsers();

            logMessage("Конец отправки", "system", 21);
        }
        else {//отработка ощибки
            //отправка в группу
            bool success = 0;
            while (!success) {
                try
                {
                    auto message = bot.getApi().sendPhoto(GroupsForSpam[GroupsForSpam.size() - 1], TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                    mi1 = message->messageId;

                    if (Mode > 2) {
                        auto message = bot.getApi().sendPhoto(GroupsForSpam[GroupsForSpam.size() - 1], TgBot::InputFile::fromFile(AltModeStr + ".png", "image/png"));
                        mi2 = message->messageId;
                    }

                    success = 1;
                }
                catch (const std::exception& e)
                {
                    logMessage(std::format("Error: {}", e.what()), "system", 22);
                }
            }

            //рассылка
            if (Mode < 3) {
                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[GroupsForSpam.size() - 1], mi1);

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 23);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 24);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 25);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 26);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }
            else {

                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[GroupsForSpam.size() - 1], mi1);
                        bot.getApi().copyMessage(subscribedUsers[i].tgId, GroupsForSpam[GroupsForSpam.size() - 1], mi2);

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 27);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 28);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 29);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 30);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }

            //сохранение пользователей
            saveUsers();
        }

    }
    catch (const std::exception& e)
    {
        logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 11) EROR | " + (string)e.what(), "system", 31);
    }

    mtx1.lock();
    syncMode = 0;
    mtx1.unlock();
}

void updateV1() {

    try
    {
        //ожидание сообщения об отправке от ядра
        {
            mtx1.lock();
            if (syncMode == 1) {

                try {
                    if (isUpdate && RootTgId != 0) {
                        bot.getApi().sendMessage(RootTgId, "Обнова (качается и устанавливается сама)\n" + CurrentVers + " -> " + newVersion +
                            "\n\nПодробнее об оновлении:\nhttps://t.me/backgroundbotvksit", false, 0, NULL);
                    }
                }
                catch (const std::exception& e)
                {
                    string str = e.what();
                    logMessage(std::format("Error: Не удалось скинуть сообщение о обновлении - {}", e.what()), "system", 222);
                }

                bool wait = 1;
                syncMode = 2;

                mtx1.unlock();

                while (wait) {
                    this_thread::sleep_for(300ms);
                    mtx1.lock();
                    wait = syncMode != 3;
                    mtx1.unlock();
                }
            }
            else {
                mtx1.unlock();
                return;
            }
        }



        int triesToSend = 0;//попытки отправки одному человеку

        if (!ErrorOnCore) {//расписание успешно обработано

            logMessage("Отправка расписания людям", "system", 32);
            //рассылка
            if (Mode < 3) {
                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        if (subscribedUsers[i].group == -1) {
                            bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                        }
                        else {
                            if (subscribedUsers[i].mode == 0 && GroupsB[ModeS][subscribedUsers[i].group].isExists)
                                bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Groups1251[subscribedUsers[i].group] + ".png", "image/png"));

                            else if (subscribedUsers[i].mode == 1 && GroupsB[ModeS][subscribedUsers[i].group].isExists) {
                                if (IsNewRaspis[0] || AltGroupsB[0][subscribedUsers[i].group])
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Groups1251[subscribedUsers[i].group] + "S.png", "image/png"));
                            }


                            else if (subscribedUsers[i].mode == 2 || subscribedUsers[i].mode == 3) {
                                if (Teachers[ModeS].find(subscribedUsers[i].Tea) != Teachers[ModeS].end())
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Utf8_to_cp1251(subscribedUsers[i].Tea.c_str()) + ".png", "image/png"));
                                else if (subscribedUsers[i].mode == 2)
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                            }
                        }

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 33);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 34);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 35);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 36);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }
            else {

                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        if (subscribedUsers[i].group == -1) {
                            bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                            bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + ".png", "image/png"));
                        }
                        else {
                            if (subscribedUsers[i].mode == 0) {
                                if (GroupsB[ModeS][subscribedUsers[i].group].isExists)
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Groups1251[subscribedUsers[i].group] + ".png", "image/png"));

                                if (GroupsB[ModeVs][subscribedUsers[i].group].isExists)
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + "\\" + Groups1251[subscribedUsers[i].group] + ".png", "image/png"));
                            }


                            else if (subscribedUsers[i].mode == 1) {
                                if (GroupsB[ModeS][subscribedUsers[i].group].isExists && (IsNewRaspis[0] || AltGroupsB[0][subscribedUsers[i].group]))
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Groups1251[subscribedUsers[i].group] + "S.png", "image/png"));

                                if (GroupsB[ModeVs][subscribedUsers[i].group].isExists && (IsNewRaspis[1] || AltGroupsB[1][subscribedUsers[i].group]))
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + "\\" + Groups1251[subscribedUsers[i].group] + "S.png", "image/png"));
                            }


                            else if (subscribedUsers[i].mode == 2 || subscribedUsers[i].mode == 3) {
                                if (Teachers[ModeS].find(subscribedUsers[i].Tea) != Teachers[ModeS].end()) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + "\\" + Utf8_to_cp1251(subscribedUsers[i].Tea.c_str()) + ".png", "image/png"));
                                }
                                else if (subscribedUsers[i].mode == 2) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                                }

                                if (Teachers[ModeVs].find(subscribedUsers[i].Tea) != Teachers[ModeVs].end()) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + "\\" + Utf8_to_cp1251(subscribedUsers[i].Tea.c_str()) + ".png", "image/png"));
                                }
                                else if (subscribedUsers[i].mode == 2) {
                                    bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + ".png", "image/png"));
                                }

                            }
                        }

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 37);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 38);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 39);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 40);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }

            //сохранение пользователей
            saveUsers();

            logMessage("Конец отправки", "system", 41);
        }
        else {

            //рассылка
            if (Mode < 3) {
                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 42);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 51);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 54);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 43);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }
            else {

                for (int i = 0; i < subscribedUsers.size(); i++) {
                    try {
                        bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(ModeStr + ".png", "image/png"));
                        bot.getApi().sendPhoto(subscribedUsers[i].tgId, TgBot::InputFile::fromFile(AltModeStr + ".png", "image/png"));

                        triesToSend = 0;
                    }

                    catch (const std::exception& e) {
                        string error = e.what();
                        if (find_if(errorsForBan.begin(), errorsForBan.end(), [error](const string& obj) { return error.find(obj) != string::npos; }) != errorsForBan.end()) {
                            logMessage("БАН! " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 44);

                            auto UserNumber = find_if(subscribedUsers.begin(), subscribedUsers.end(),
                                [i](const myUser& obj) { return obj.tgId == subscribedUsers[i].tgId; });

                            subscribedUsers.erase(UserNumber);

                            i--;
                        }
                        else {
                            if (triesToSend > 20) {
                                logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123!) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 45);
                                try {
                                    bot.getApi().sendMessage(subscribedUsers[i].tgId, "Простите за не удобство, попытка отправки расписания вам не удалась", false, 0);
                                }
                                catch (const std::exception& e) {
                                    error = e.what();
                                    logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 123! в извинениях) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 46);
                                }

                                triesToSend = 0;
                            }
                            else {
                                logMessage("123) " + (string)e.what() + " | " + to_string(subscribedUsers[i].tgId), "system", 47);
                                i--;
                                triesToSend++;
                                Sleep(1000);
                            }
                        }
                    }
                }
            }

            //сохранение пользователей
            saveUsers();
        }

    }
    catch (const std::exception& e)
    {
        logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 11) EROR | " + (string)e.what(), "system", 48);
    }

    mtx1.lock();
    syncMode = 0;
    mtx1.unlock();
}

int main() {
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    //чтение конфига
    if (!IsNormalCfg) {
        logMessage("Неверный конфиг!", "system", 49);
        ShowWindow(GetConsoleWindow(), SW_SHOW);

        system("pause");
        exit(-1);
    }
    else {
        if (ModeSend)
            update = updateV2;
        else
            update = updateV1;
    }

    //проверка на двойной запуск
    if (isAlreadyRunning()) {
        logMessage("Бот запущен дважды!", "system", 49);
        ShowWindow(GetConsoleWindow(), SW_SHOW);

        system("pause");
        exit(-1);
    }

    //инициализация групп
    for (int i = 0; i < Groups.size(); i++) {
        GroupsB[0].push_back(false);
        GroupsB[1].push_back(false);
        GroupsB[2].push_back(false);
        GroupsB[3].push_back(false);
        AltGroupsB[0].push_back(false);
        AltGroupsB[1].push_back(false);
        DisabledGroups.push_back(false);

        Groups1251.push_back(Utf8_to_cp1251(Groups[i].c_str()));
    }

    //чтение папок
    {
        string currentFile;
        for (int j = 0; j < 4; j++) {
            for (const auto& entry : fs::directory_iterator(to_string(j + 1))) {
                bool isGroup = 0;
                currentFile = cp1251_to_utf8(entry.path().filename().string().c_str());
                currentFile = currentFile.erase(currentFile.size() - 4);


                for (int i = 0; i < Groups.size(); i++) {
                    if (Groups[i] == currentFile) {
                        isGroup = 1;
                        GroupsB[j][i] = 1;
                        i = Groups.size();
                    }
                }

                if (!isGroup && currentFile[currentFile.size() - 1] != 'S') {
                    Teachers[j].insert(currentFile);
                }
            }
        }

        ifstream ifs("4\\t.txt");
        while (getline(ifs, currentFile)) {
            DefTeachers.insert(currentFile);
        }
    }

    //инициализация пользователей
    {
        string str;
        ifstream getChatIdsOnFile("..\\users.txt");
        while (getline(getChatIdsOnFile, str)) {
            if (str != "") {
                myUser myU;
                size_t pos = 0;
                string setting[3];

                for (int i = 0; i < 3; i++) {
                    pos = str.find(' ');
                    setting[i] = str.substr(0, pos);
                    str.erase(0, pos + 1);
                }

                if (setting[2] == "2" || setting[2] == "3") {
                    myU.tgId = stoll(setting[0]);
                    myU.Tea = setting[1];
                    myU.mode = setting[2][0] - '0';
                    myU.group = 0;
                }
                else {
                    myU.tgId = stoll(setting[0]);
                    myU.group = stoi(setting[1]);
                    myU.mode = setting[2][0] - '0';
                }

                subscribedUsers.push_back(myU);

                if (myU.mode < 2 && myU.group != -1 && DisabledGroups[myU.group] == 0)
                    DisabledGroups[myU.group] = 1;
            }
        }
        getChatIdsOnFile.close();
    }

    //инициализация наказанных пользователей
    {
        ifstream ifs("..\\mut.txt");
        string str;

        if (!ifs.is_open()) {
            ofstream ofs("..\\mut.txt");
            ofs.close();
        }
        else
            while (getline(ifs, str)) mutedUsers.insert(stoll(str));

        ifs.close();

        ifs.open("..\\ban.txt");

        if (!ifs.is_open()) {
            ofstream ofs("..\\ban.txt");
            ofs.close();
        }
        else
            while (getline(ifs, str)) blockedUsers.insert(stoll(str));
        ifs.close();
    }

    //добавление списка команд
    {
        TgBot::BotCommand::Ptr commandPtr;
        std::vector<BotCommand::Ptr> commands2;

        for (myCommand& command : Commands) {
            commandPtr = std::make_shared<TgBot::BotCommand>();
            commandPtr->command = command.command;
            commandPtr->description = command.description;
            commands2.push_back(commandPtr);
        }

        bot.getApi().setMyCommands(commands2);
    }


    //меню с кнопками
    TgBot::ReplyKeyboardMarkup::Ptr subscribeKeyboard(new TgBot::ReplyKeyboardMarkup);
    TgBot::ReplyKeyboardMarkup::Ptr mainMenuKeyboard(new TgBot::ReplyKeyboardMarkup);
    TgBot::ReplyKeyboardMarkup::Ptr raspisForGroupKeyboard(new TgBot::ReplyKeyboardMarkup);
    TgBot::ReplyKeyboardMarkup::Ptr raspisForTeacherKeyboard(new TgBot::ReplyKeyboardMarkup);
    TgBot::ReplyKeyboardMarkup::Ptr getRaspisKeyboard(new TgBot::ReplyKeyboardMarkup);
    TgBot::ReplyKeyboardMarkup::Ptr mSubscribeKeyboard(new TgBot::ReplyKeyboardMarkup);
    {
#ifndef __INTELLISENSE__
        // Клавиатура с кнопкой "Подписаться на расписание"

        subscribeKeyboard->resizeKeyboard = true;
        TgBot::KeyboardButton::Ptr subscribeButton(new TgBot::KeyboardButton("Подписаться на расписание"));
        subscribeKeyboard->keyboard.push_back({ subscribeButton });


        //общие кнопки
        TgBot::KeyboardButton::Ptr escButton(new TgBot::KeyboardButton("Назад"));
        TgBot::KeyboardButton::Ptr modeButton(new TgBot::KeyboardButton("Состояние подписки"));


        //главное меню
        mainMenuKeyboard->resizeKeyboard = true;
        //
        TgBot::KeyboardButton::Ptr mainMenuButton1(new TgBot::KeyboardButton("Расписание для групп"));
        TgBot::KeyboardButton::Ptr mainMenuButton2(new TgBot::KeyboardButton("Расписание для преподавателей"));
        TgBot::KeyboardButton::Ptr mainMenuButton3(new TgBot::KeyboardButton("Получить расписание"));
        TgBot::KeyboardButton::Ptr mainMenuButton4(new TgBot::KeyboardButton("Доп. подписка"));
        TgBot::KeyboardButton::Ptr mainMenuButton5(new TgBot::KeyboardButton("Подписаться на общее расписание"));
        TgBot::KeyboardButton::Ptr mainMenuButton7(new TgBot::KeyboardButton("Вопросы и предложения"));
        TgBot::KeyboardButton::Ptr mainMenuButton6(new TgBot::KeyboardButton("Отписаться от расписания"));
        //
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton1 });
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton2 });
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton3 });
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton4 });
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton5 });
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton7 });
        mainMenuKeyboard->keyboard.push_back({ mainMenuButton6 });


        //расписание для групп
        raspisForGroupKeyboard->resizeKeyboard = true;
        //
        TgBot::KeyboardButton::Ptr raspisForGroopButton1(new TgBot::KeyboardButton("Подписаться на расписание группы"));
        TgBot::KeyboardButton::Ptr raspisForGroopButton2(new TgBot::KeyboardButton("Подписаться на отдельное расписание группы"));
        //
        raspisForGroupKeyboard->keyboard.push_back({ raspisForGroopButton1 });
        raspisForGroupKeyboard->keyboard.push_back({ raspisForGroopButton2 });
        raspisForGroupKeyboard->keyboard.push_back({ modeButton });
        raspisForGroupKeyboard->keyboard.push_back({ escButton });


        //расписание для преподавателей
        raspisForTeacherKeyboard->resizeKeyboard = true;
        //
        TgBot::KeyboardButton::Ptr raspisForTeaherButton1(new TgBot::KeyboardButton("Подписаться на расписание преподавателя"));
        TgBot::KeyboardButton::Ptr raspisForTeaherButton2(new TgBot::KeyboardButton("Получать расписание и для другого корпуса"));
        //
        raspisForTeacherKeyboard->keyboard.push_back({ raspisForTeaherButton1 });
        raspisForTeacherKeyboard->keyboard.push_back({ raspisForTeaherButton2 });
        raspisForTeacherKeyboard->keyboard.push_back({ modeButton });
        raspisForTeacherKeyboard->keyboard.push_back({ escButton });


        //получить расписание
        getRaspisKeyboard->resizeKeyboard = true;
        //
        TgBot::KeyboardButton::Ptr getRaspisButton1(new TgBot::KeyboardButton("Получить общее расписание"));
        TgBot::KeyboardButton::Ptr getRaspisButton2(new TgBot::KeyboardButton("Получить расписание преподавателя"));
        TgBot::KeyboardButton::Ptr getRaspisButton3(new TgBot::KeyboardButton("Получить расписание группы"));
        //
        getRaspisKeyboard->keyboard.push_back({ getRaspisButton1 });
        getRaspisKeyboard->keyboard.push_back({ getRaspisButton2 });
        getRaspisKeyboard->keyboard.push_back({ getRaspisButton3 });
        getRaspisKeyboard->keyboard.push_back({ escButton });


        //Доп. подписка
        mSubscribeKeyboard->resizeKeyboard = true;
        //
        TgBot::KeyboardButton::Ptr mSubscribeButton1(new TgBot::KeyboardButton("Подписаться ещё на одно расписание"));
        TgBot::KeyboardButton::Ptr mSubscribeButton2(new TgBot::KeyboardButton("Отписаться от одного из расписаний"));
        //
        mSubscribeKeyboard->keyboard.push_back({ mSubscribeButton1 });
        mSubscribeKeyboard->keyboard.push_back({ modeButton });
        mSubscribeKeyboard->keyboard.push_back({ mSubscribeButton2 });
        mSubscribeKeyboard->keyboard.push_back({ escButton });
#else
#endif
    }


    //запуск бота пользователем
    bot.getEvents().onCommand("start", [subscribeKeyboard, mainMenuKeyboard](TgBot::Message::Ptr message) {
        bool isUserSubs = find_if(subscribedUsers.begin(), subscribedUsers.end(),
            [message](const myUser& obj) { return obj.tgId == message->chat->id; }) != subscribedUsers.end();

        try
        {
            if (!isUserSubs) {
                bot.getApi().sendMessage(message->chat->id, StartText, false, 0, subscribeKeyboard);
            }
            else {
                bot.getApi().sendMessage(message->chat->id, "Вы уже запустили бота", false, 0, mainMenuKeyboard);
            }
        }
        catch (const std::exception& e)
        {
            logMessage("(9) EROR | " + (string)e.what() + " | by_user: " + to_string(message->chat->id), "system", 50);
        }
        });


    //команды и меню
    bot.getEvents().onAnyMessage([subscribeKeyboard, mainMenuKeyboard, raspisForGroupKeyboard, raspisForTeacherKeyboard, getRaspisKeyboard, mSubscribeKeyboard](TgBot::Message::Ptr message) {
        try
        {
            if (blockedUsers.find(message->chat->id) == blockedUsers.end()) {
                int64_t userId = message->chat->id;

                int UserNumber = subscribedUsers.size();
                vector<int> UserNumbers;
                for (int i = 0; i < subscribedUsers.size(); i++) {

                    if (subscribedUsers[i].tgId == message->chat->id) {
                        if (UserNumber == subscribedUsers.size())
                            UserNumber = i;
                        else
                            UserNumbers.push_back(i);
                    }
                }
                bool isUserSubs = UserNumber != subscribedUsers.size();
                bool isMMessage = 0, isNormalCommand = 1;
                int8_t isStandartMessage = 1;


                if (isUserSubs) {
                    if (message->text[0] == '/') {
                        string commandParam;
                        if (message->text.find(' ') != string::npos)
                            commandParam = message->text.substr(message->text.find(' ') + 1);

                        if (message->text.find("/m") == 0) {
                            message->text = commandParam;
                            isMMessage = 1;

                            if (message->text.find(' ') != string::npos)
                                commandParam = message->text.substr(message->text.find(' ') + 1);
                            else if (message->text != "/sub_o")
                                message->text = "Подписаться ещё на одно расписание";

                            if (message->text != "Подписаться ещё на одно расписание") {
                                if (UserNumbers.size() > 3) {
                                    message->text = "e";
                                    isStandartMessage = 2;
                                    bot.getApi().sendMessage(userId, "Нельзя подписаться более чем на 5 различных расписаний", false, 0, NULL);
                                }
                            }
                        }

                        if (message->text.find("/unsubscribe") == 0) {
                            bot.getApi().sendMessage(userId, "Обработка...", false, 0, NULL);

                            // Убираем пользователя из списка подписанных
                            subscribedUsers.erase(subscribedUsers.begin() + UserNumber);
                            if (UserNumbers.size() != 0) {
                                for (int i = 0; i < subscribedUsers.size(); i++) {
                                    if (subscribedUsers[i].tgId == message->chat->id) {
                                        subscribedUsers.erase(subscribedUsers.begin() + UserNumber);
                                        i--;
                                    }
                                }
                            }

                            saveUsers();

                            bot.getApi().sendMessage(userId, "Вы отписались от расписания.", false, 0, subscribeKeyboard);
                        }
                        else if (message->text.find("/status") == 0) {
                            message->text = "Состояние подписки";
                        }
                        else if (message->text == "/sub_o") {
                            message->text = "Подписаться на общее расписание";
                        }
                        else if (message->text.find("/sub_go") == 0) {
                            if (commandParam != "") {
                                commandParam = formatG(commandParam);

                                myUser* tUher = &subscribedUsers[UserNumber];
                                auto group = find(Groups.begin(), Groups.end(), commandParam);

                                if (isMMessage && group != Groups.end()) {
                                    for (auto& a : UserNumbers)
                                        if (subscribedUsers[a].mode == 1 && subscribedUsers[a].group == group - Groups.begin())
                                            isNormalCommand = 0;

                                    if (subscribedUsers[UserNumber].mode == 1 && subscribedUsers[UserNumber].group == group - Groups.begin())
                                        isNormalCommand = 0;

                                    if (!isNormalCommand)
                                        bot.getApi().sendMessage(userId, "Вы уже подписаны на отдельное расписание этой группы " + commandParam, false, 0, NULL);
                                    else {
                                        myUser MyUs;
                                        MyUs.tgId = userId;
                                        MyUs.group = -1;
                                        MyUs.mode = 0;

                                        subscribedUsers.insert(subscribedUsers.begin() + UserNumber + UserNumbers.size() + 1, MyUs);
                                        tUher = &subscribedUsers[UserNumber + UserNumbers.size() + 1];
                                    }

                                }

                                if (group != Groups.end() && isNormalCommand) {
                                    bot.getApi().sendMessage(userId, "Вы подписались на отдельное расписание группы " + commandParam, false, 0, NULL);

                                    tUher->group = group - Groups.begin();
                                    tUher->mode = 1;
                                    saveUsers();

                                    if (GroupsB[0][tUher->group].isExists)
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1\\" + Utf8_to_cp1251(Groups[tUher->group].c_str()) + "S.png", "image/png"));
                                    if (GroupsB[1][tUher->group].isExists)
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2\\" + Utf8_to_cp1251(Groups[tUher->group].c_str()) + "S.png", "image/png"));
                                    if (GroupsB[1][group - Groups.begin()].isExists && GroupsB[0][group - Groups.begin()].isExists)
                                        bot.getApi().sendMessage(userId, "Расписание группы \"" + commandParam + "\" не найдено", false, 0, NULL);
                                }
                                else if (isNormalCommand)
                                    bot.getApi().sendMessage(userId, "Группа \"" + commandParam + "\" не найдена", false, 0, NULL);
                            }
                            else {
                                bot.getApi().sendMessage(userId, "Добавьте группу в сообщение\nПример: /sub_go ИСП-322р", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/sub_g") == 0) {
                            if (commandParam != "") {

                                commandParam = formatG(commandParam);
                                myUser* tUher = &subscribedUsers[UserNumber];
                                auto group = find(Groups.begin(), Groups.end(), commandParam);

                                if (isMMessage && group != Groups.end()) {
                                    for (auto& a : UserNumbers)
                                        if (subscribedUsers[a].mode == 0 && subscribedUsers[a].group == group - Groups.begin())
                                            isNormalCommand = 0;

                                    if (subscribedUsers[UserNumber].mode == 0 && subscribedUsers[UserNumber].group == group - Groups.begin())
                                        isNormalCommand = 0;

                                    if (!isNormalCommand)
                                        bot.getApi().sendMessage(userId, "Вы уже подписаны на расписание этой группы " + commandParam, false, 0, NULL);
                                    else {
                                        myUser MyUs;
                                        MyUs.tgId = userId;
                                        MyUs.group = -1;
                                        MyUs.mode = 0;

                                        subscribedUsers.insert(subscribedUsers.begin() + UserNumber + UserNumbers.size() + 1, MyUs);
                                        tUher = &subscribedUsers[UserNumber + UserNumbers.size() + 1];
                                    }

                                }

                                if (group != Groups.end() && isNormalCommand) {
                                    bot.getApi().sendMessage(userId, "Вы подписались на расписание группы " + commandParam, false, 0, NULL);

                                    tUher->group = group - Groups.begin();
                                    tUher->mode = 0;
                                    saveUsers();

                                    if (GroupsB[0][tUher->group].isExists)
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1\\" + Utf8_to_cp1251(Groups[tUher->group].c_str()) + ".png", "image/png"));
                                    if (GroupsB[1][tUher->group].isExists)
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2\\" + Utf8_to_cp1251(Groups[tUher->group].c_str()) + ".png", "image/png"));
                                    if (GroupsB[1][group - Groups.begin()].isExists && GroupsB[0][group - Groups.begin()].isExists)
                                        bot.getApi().sendMessage(userId, "Расписание группы \"" + commandParam + "\" не найдено", false, 0, NULL);
                                }
                                else if (isNormalCommand)
                                    bot.getApi().sendMessage(userId, "Группа \"" + commandParam + "\" не найдена", false, 0, NULL);
                            }
                            else {
                                bot.getApi().sendMessage(userId, "Добавьте группу в сообщение\nПример: /sub_g ИСП-322р", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/sub_p") == 0) {
                            if (commandParam != "") {

                                commandParam = formatT(commandParam);
                                myUser* tUher = &subscribedUsers[UserNumber];
                                string teacher = commandParam;
                                string altTeacher = Utf8_to_cp1251(teacher.c_str());
                                bool isActualTea = 1;

                                isActualTea = isActualTea && altTeacher[0] < -32 && altTeacher[0] > -65;
                                for (int i = 1; i < altTeacher.size(); i++) {
                                    isActualTea = isActualTea && (altTeacher[i] < 0 && altTeacher[i] > -33) || altTeacher[i] == 46;//-1 я, -64 А
                                }

                                if (isMMessage && isActualTea) {
                                    for (auto& a : UserNumbers)
                                        if (subscribedUsers[a].mode == 2 && subscribedUsers[a].Tea == teacher)
                                            isNormalCommand = 0;

                                    if (subscribedUsers[UserNumber].mode == 2 && subscribedUsers[UserNumber].Tea == teacher)
                                        isNormalCommand = 0;

                                    if (!isNormalCommand)
                                        bot.getApi().sendMessage(userId, "Вы уже подписаны на расписание преподавателя " + commandParam, false, 0, NULL);
                                    else {
                                        myUser MyUs;
                                        MyUs.tgId = userId;
                                        MyUs.group = -1;
                                        MyUs.mode = 0;

                                        subscribedUsers.insert(subscribedUsers.begin() + UserNumber + UserNumbers.size() + 1, MyUs);
                                        tUher = &subscribedUsers[UserNumber + UserNumbers.size() + 1];
                                    }

                                }

                                if (isActualTea && isNormalCommand) {
                                    commandParam.insert(commandParam.end() - 6, 32);
                                    bot.getApi().sendMessage(userId, "Вы подписались на расписание преподавателя " + commandParam, false, 0, NULL);

                                    tUher->Tea = teacher;
                                    tUher->mode = 3;
                                    tUher->group = 0;
                                    saveUsers();


                                    if (Teachers[0].find(teacher) != Teachers[0].end())
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1\\" + altTeacher + ".png", "image/png"));
                                    else
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1.png", "image/png"));


                                    if (Teachers[1].find(teacher) != Teachers[1].end())
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2\\" + altTeacher + ".png", "image/png"));
                                    else
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2.png", "image/png"));
                                }
                                else if (isNormalCommand)
                                    bot.getApi().sendMessage(userId, "ФИО преподавателя должна состоять из русских символов, точек и пробела (не обязательно)", false, 0, NULL);
                            }
                            else {
                                bot.getApi().sendMessage(userId, "Добавьте ФИО преподавателя в сообщение\nПример: /sub_p Авдуевская Н.С.", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/get_o") == 0) {
                            message->text = "Получить общее расписание";
                        }
                        else if (message->text.find("/get_p") == 0) {
                            if (commandParam != "") {

                                commandParam = formatT(commandParam);
                                string altTeacher = Utf8_to_cp1251(commandParam.c_str());//раньше тут была ещё и переменная teacher
                                bool isActualTea = 1;

                                isActualTea = isActualTea && altTeacher[0] < -32 && altTeacher[0] > -65;
                                for (int i = 1; i < altTeacher.size(); i++) {
                                    isActualTea = isActualTea && (altTeacher[i] < 0 && altTeacher[i] > -33) || altTeacher[i] == 32 || altTeacher[i] == 46;//-1 я, -64 А
                                }

                                if (isActualTea) {
                                    if (Teachers[0].find(commandParam) != Teachers[0].end())
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1\\" + altTeacher + ".png", "image/png"));


                                    if (Teachers[1].find(commandParam) != Teachers[1].end())
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2\\" + altTeacher + ".png", "image/png"));
                                }
                            }
                            else
                                bot.getApi().sendMessage(userId, "Добавьте ФИО преподавателя в сообщение\nПример: /get_p Авдуевская Н.С.", false, 0, NULL);
                        }
                        else if (message->text.find("/get_g") == 0) {
                            if (commandParam != "") {

                                commandParam = formatG(commandParam);
                                auto group = find(Groups.begin(), Groups.end(), commandParam);

                                if (group != Groups.end()) {
                                    if (GroupsB[0][group - Groups.begin()].isExists)
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1\\" + Utf8_to_cp1251(Groups[group - Groups.begin()].c_str()) + ".png", "image/png"));
                                    if (GroupsB[1][group - Groups.begin()].isExists)
                                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2\\" + Utf8_to_cp1251(Groups[group - Groups.begin()].c_str()) + ".png", "image/png"));
                                    if (GroupsB[1][group - Groups.begin()].isExists && GroupsB[0][group - Groups.begin()].isExists)
                                        bot.getApi().sendMessage(userId, "Расписание группы \"" + commandParam + "\" не найдено", false, 0, NULL);
                                }
                                else
                                    bot.getApi().sendMessage(userId, "Группа \"" + commandParam + "\" не найдена", false, 0, NULL);
                            }
                            else
                                bot.getApi().sendMessage(userId, "Добавьте группу в сообщение\nПример: /get_g ИСП-322р", false, 0, NULL);
                        }
                        else if (message->text.find("/t_dop") == 0) {
                            bool isValidUser = 0;
                            int* firstT = NULL;

                            if (subscribedUsers[UserNumber].mode == 2 || subscribedUsers[UserNumber].mode == 3) {
                                firstT = &UserNumber;
                                isValidUser = 1;
                            }
                            else for (int& a : UserNumbers)
                                if (subscribedUsers[a].mode == 2 || subscribedUsers[a].mode == 3) {
                                    isValidUser = 1;
                                    if (firstT == NULL)
                                        firstT = &a;
                                }



                            if (isValidUser) {
                                if (commandParam != "") {

                                    if (commandParam == "да") {
                                        subscribedUsers[*firstT].mode = 2;
                                        saveUsers();
                                        bot.getApi().sendMessage(userId, "Готово", false, 0, NULL);
                                    }
                                    else if (commandParam == "нет") {
                                        subscribedUsers[*firstT].mode = 3;
                                        saveUsers();
                                        bot.getApi().sendMessage(userId, "Готово", false, 0, NULL);
                                    }
                                    else
                                        bot.getApi().sendMessage(userId, "Некорректный ввод, ожидалось \"/t_dop да\" или \"/t_dop нет\"", false, 0, NULL);
                                }
                                else {
                                    bot.getApi().sendMessage(userId, "Пример сообщения целиком: /t_dop да\nОписание: по умолчанию вы получаете расписание обоих корпусов, если вы хотите получить расписание только своего корпуса, используйте эту команду с параметром нет", false, 0, NULL);
                                }
                            }
                            else
                                bot.getApi().sendMessage(userId, "Вы должны быть подписаны на расписание для преподавателя", false, 0, NULL);
                        }
                        else if (message->text.find("/qq") == 0 && (userId == RootTgId || userId == SecondRootTgId)) {

                            if (commandParam == "1")
                                bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("..\\system.txt", "text/plain"));
                            else if (commandParam == "2")
                                bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("4\\t.txt", "text/plain"));
                            else if (commandParam == "3")
                                bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("..\\spamText.txt", "text/plain"));
                            else if (commandParam == "4")
                                bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("..\\updater\\log.txt", "text/plain"));
                            else
                                bot.getApi().sendMessage(userId, "Добавьте номер файла из /info\nПример: /qq 2", false, 0, NULL);
                        }
                        else if (message->text == "/q" && (userId == RootTgId || userId == SecondRootTgId)) {
                            bot.getApi().sendMessage(userId,
                                escapeMarkdownV2("Версия бота: " + version +
                                    "\nВладелец: ||" + to_string(RootTgId) + " " + bot.getApi().getChat(RootTgId)->username +
                                    "||\nМут /mut 123312321\
\nРазмут /unmut 123312321\
\nЗабанить /ban 123312321\
\nРазбанить /unban 123312321\
\nСписок преподавателей /tea\
\nИнфа по tgId /get_us\
\nСтатистика /stats\
\nПро доступ /info")
, false, 0, nullptr, "MarkdownV2");
                        }
                        else if (message->text.find("/mut") == 0 && userId == RootTgId) {
                            if (commandParam != "" && mutedUsers.find(stoll(commandParam)) == mutedUsers.end()) {
                                if (stoll(commandParam) != RootTgId && stoll(commandParam) != SecondRootTgId) {
                                    mutedUsers.insert(stoll(commandParam));
                                    saveBadUsers(0);
                                    bot.getApi().sendMessage(userId, "Готово!", false, 0, NULL);
                                }
                                else
                                    bot.getApi().sendMessage(userId, "Не надо мутить владельца или создателя", false, 0, NULL);
                            }
                            else {
                                if (commandParam == "") {
                                    string str = "\nСписок замученных:\n";

                                    for (auto& a : mutedUsers) {
                                        str += to_string(a);
                                    }

                                    bot.getApi().sendMessage(userId, "Добавьте tgId пользователя" + str, false, 0, NULL);
                                }
                                else
                                    bot.getApi().sendMessage(userId, "Пользователь уже в этом списке", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/unmut") == 0 && userId == RootTgId) {
                            if (commandParam != "" && mutedUsers.find(stoll(commandParam)) != mutedUsers.end()) {
                                mutedUsers.erase(mutedUsers.find(stoll(commandParam)));
                                saveBadUsers(0);
                                bot.getApi().sendMessage(userId, "Готово!", false, 0, NULL);
                            }
                            else {
                                if (commandParam == "")
                                    bot.getApi().sendMessage(userId, "Добавьте tgId пользователя", false, 0, NULL);
                                else
                                    bot.getApi().sendMessage(userId, "Пользователь уже убран из списка", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/ban") == 0 && userId == RootTgId) {
                            if (commandParam != "" && blockedUsers.find(stoll(commandParam)) == blockedUsers.end()) {
                                if (stoll(commandParam) != RootTgId && stoll(commandParam) != SecondRootTgId) {
                                    blockedUsers.insert(stoll(commandParam));

                                    for (int i = 0; i < subscribedUsers.size(); i++) {

                                        if (subscribedUsers[i].tgId == stoll(commandParam)) {
                                            subscribedUsers.erase(subscribedUsers.begin() + i);
                                        }
                                    }

                                    saveUsers();
                                    saveBadUsers(1);

                                    bot.getApi().sendMessage(userId, "Готово!", false, 0, NULL);
                                }
                                else
                                    bot.getApi().sendMessage(userId, "Не надо банить владельца или создателя", false, 0, NULL);
                            }
                            else {
                                if (commandParam == "") {
                                    string str = "\nСписок забаненных:\n";

                                    for (auto& a : blockedUsers) {
                                        str += to_string(a);
                                    }

                                    bot.getApi().sendMessage(userId, "Добавьте tgId пользователя" + str, false, 0, NULL);
                                }
                                else
                                    bot.getApi().sendMessage(userId, "Пользователь уже в этом списке", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/unban") == 0 && userId == RootTgId) {
                            if (commandParam != "" && blockedUsers.find(stoll(commandParam)) != blockedUsers.end()) {
                                blockedUsers.erase(blockedUsers.find(stoll(commandParam)));
                                saveBadUsers(1);
                                bot.getApi().sendMessage(userId, "Готово!", false, 0, NULL);
                            }
                            else {
                                if (commandParam == "")
                                    bot.getApi().sendMessage(userId, "Добавьте tgId пользователя", false, 0, NULL);
                                else
                                    bot.getApi().sendMessage(userId, "Пользователь уже убран из списка", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/unm") == 0) {
                            if (commandParam != "" && -1 < stoi(commandParam) - 1 && stoi(commandParam) - 1 < UserNumbers.size()) {
                                subscribedUsers.erase(subscribedUsers.begin() + UserNumbers[stoi(commandParam) - 1]);
                                bot.getApi().sendMessage(userId, "Готово!", false, 0, NULL);
                                saveUsers();
                            }
                            else if (commandParam == "") {
                                bot.getApi().sendMessage(userId, "Добавьте номер доп. подписки из /status\nПример: /unm 1", false, 0, NULL);
                            }
                            else {
                                bot.getApi().sendMessage(userId, "Не верный номер доп. подписки", false, 0, NULL);
                            }
                        }
                        else if (message->text.find("/stats") == 0 && (userId == RootTgId || userId == SecondRootTgId)) {
                            int g = 0, go = 0, p = 0, o = 0;

                            for (auto& a : subscribedUsers) {
                                if (a.group == -1)
                                    o++;
                                else if (a.mode == 0)
                                    g++;
                                else if (a.mode == 1)
                                    go++;
                                else if (a.mode == 2 || a.mode == 3)
                                    p++;

                            }

                            bot.getApi().sendMessage(userId, "Пользователи: " + to_string(subscribedUsers.size()) +
                                "\nПреподаватели (всего): " + to_string(DefTeachers.size()) +
                                "\nРежим рассылки: " + to_string(ModeSend + 1) +
                                "\nБуферных групп: " + to_string(GroupsForSpam.size()) +
                                "\nЗадержка проверки: " + to_string(SleepTime / 1000) +
                                "\nРеклама: " + (EnableAd ? "Вкл." : "Выкл.") +
                                "\nАвто обнова: " + (EnableAutoUpdate ? "Вкл." : "Выкл.") +
                                "\nГрупп без подписчиков (в последней рассылке): " + to_string(DisabledGroupsC) +
                                "\nP: " + to_string(p) +
                                "\nG: " + to_string(g) +
                                "\nGO: " + to_string(go) +
                                "\nO: " + to_string(o)
                                , false, 0, NULL);
                        }
                        else if (message->text.find("/info") == 0 && (userId == RootTgId || userId == SecondRootTgId)) {
                            bot.getApi().sendMessage(userId, "вопросы и предложения tg: @wyanarba\n\nпро доступ:\nу меня (создателя бота):\
\n1) команды /q и /stats\n2) так же возможность скачивать некоторые файлы (/qq число)\
\n1 - файл с системными логами ошибками и тд. system.txt\
\n2 - файл с преподавателями которые были встречены в расписании когда либо 4/t.txt\
\n3 - файл с списком слов для фильтрации spamText.txt\
\n4 - файл с логом автообновления\
\nвсе эти файлы нужны мне для улучшения бота\
\n\nу тебя (владельца бота):\
\n1 - сообщение пользователю от лица бота: (telegram id человека)Текст сообщения\
\n2 - сообщение всем пользователям: )(Текст сообщения\
\nвсе остальные команды из /q, а так же те, что были перечислены выше\
\nпри подписке на расписание на первое место в очереди попадает владелец, на второе я, дальше кто как успеет"
, false, 0, NULL);
                        }
                        else if (message->text.find("/tea") == 0 && (userId == RootTgId || userId == SecondRootTgId)) {
                            string teachers;
                            int i = 0;

                            for (auto& a : subscribedUsers)
                                if (a.mode > 1 && a.mode < 4) {
                                    i++;
                                    Chat::Ptr chat = bot.getApi().getChat(a.tgId);
                                    teachers += std::format("{}) {}:\n    @{},\n    {}\n", i, a.Tea, (chat->username == "" ? "Тега нет(" : chat->username), a.tgId);
                                }

                            bot.getApi().sendMessage(userId, teachers, false, 0, NULL);
                        }
                        else if (message->text.find("/get_us") == 0 && (userId == RootTgId || userId == SecondRootTgId)) {

                            if (commandParam != "") {
                                __int64 requestUid = stoll(commandParam);//его тг id

                                int RUserNumber = subscribedUsers.size();
                                vector<int> RUserNumbers;

                                string message;

                                for (int i = 0; i < subscribedUsers.size(); i++) {

                                    if (subscribedUsers[i].tgId == requestUid) {
                                        if (RUserNumber == subscribedUsers.size())
                                            RUserNumber = i;
                                        else
                                            RUserNumbers.push_back(i);
                                    }
                                }

                                if (RUserNumber != subscribedUsers.size()) {
                                    if (subscribedUsers[RUserNumber].group == -1)
                                        message = "Подписан на общее расписание";

                                    else if (subscribedUsers[RUserNumber].mode == 0)
                                        message = "Подписан на расписание с выделенной группой " + Groups[subscribedUsers[RUserNumber].group];

                                    else if (subscribedUsers[RUserNumber].mode == 1)
                                        message = "Подписан на отдельное расписание группы " + Groups[subscribedUsers[RUserNumber].group];

                                    else if (subscribedUsers[RUserNumber].mode == 2)
                                        message = "Подписан на расписание преподавателя с ФИО " + subscribedUsers[RUserNumber].Tea + ", и получает расписание двух корпусов";

                                    else if (subscribedUsers[RUserNumber].mode == 3)
                                        message = "Подписан на расписание преподавателя с ФИО " + subscribedUsers[RUserNumber].Tea + ", и получает расписание только корпуса с ним";

                                    if (RUserNumbers.size() > 0) {
                                        message += "\n\nДоп. подписки:";

                                        for (int i = 0; i < RUserNumbers.size(); i++) {
                                            message += "\n" + to_string(i + 1) + ") ";
                                            int& a = RUserNumbers[i];

                                            if (subscribedUsers[a].group == -1)
                                                message += "Общее расписание";

                                            else if (subscribedUsers[a].mode == 0)
                                                message += "Расписание с выделенной группой " + Groups[subscribedUsers[a].group];

                                            else if (subscribedUsers[a].mode == 1)
                                                message += "Отдельное расписание группы " + Groups[subscribedUsers[a].group];

                                            else if (subscribedUsers[a].mode == 2)
                                                message += "Расписание преподавателя с ФИО " + subscribedUsers[a].Tea + ", и получает расписание двух корпусов";

                                            else if (subscribedUsers[a].mode == 3)
                                                message += "Расписание преподавателя с ФИО " + subscribedUsers[a].Tea + ", и получает расписание только корпуса с ним";
                                        }
                                    }
                                }

                                TgBot::Chat::Ptr chat = bot.getApi().getChat(requestUid);
                                bot.getApi().sendMessage(userId, std::format("@{}\n\n{}",
                                    (chat->username == "" ? "Тега нет(" : chat->username),
                                    message
                                ), false, 0, NULL);
                            }

                        }
                        else if (message->text.find("/un_button") == 0) {
                            ReplyKeyboardRemove::Ptr removeKeyboard(new ReplyKeyboardRemove);
                            bot.getApi().sendMessage(userId, "Клавиатура удалена\n\nЧто бы вернуть пропишите /start", false, 0, removeKeyboard);
                        }
                        else if (message->text.find("/update") == 0 && (userId == RootTgId || userId == SecondRootTgId)) {
                            tryesChek = 0;
                        }
                        else if (message->text == "/start");
                        else isStandartMessage = 0;

                        if (isStandartMessage != 0)
                            isStandartMessage = 2;
                    }

                    if (userId == RootTgId && message->text[0] == '(') {
                        string messageE = message->text, messageE2 = "";
                        bool isOK = 0;
                        for (int i = 1; i < messageE.size(); i++) {
                            if (messageE[i] != ')') {
                                messageE2 += messageE[i];
                            }
                            else {
                                isOK = 1;
                                i = messageE.size();
                            }
                        }

                        if (messageE.substr(messageE2.size() + 2) == "") {
                            bot.getApi().sendMessage(userId, "Сообщение не должно быть пустым!", false, 0, NULL);
                            isOK = 0;
                        }

                        if (isOK) {
                            bot.getApi().sendMessage(stoll(messageE2), messageE.substr(messageE2.size() + 2), false, 0);
                            bot.getApi().sendMessage(userId, "Отправлено", false, 0, NULL);
                        }

                    }
                    else if (userId == RootTgId && message->text[0] == ')' && message->text[1] == '(') {
                        string messageE = message->text.substr(2);

                        if (messageE != "") {
                            bot.getApi().sendMessage(RootTgId, "начало", false, 0);

                            for (auto& user : subscribedUsers) {
                                try {
                                    bot.getApi().sendMessage(user.tgId, messageE, false, 0, mainMenuKeyboard);
                                }
                                catch (const std::exception& e) {
                                    logMessage("124) EROR | " + (string)e.what(), "system", 52);
                                    continue;
                                }
                            }

                            bot.getApi().sendMessage(RootTgId, "конец", false, 0);
                        }
                        else
                            bot.getApi().sendMessage(userId, "Сообщение не должно быть пустым!", false, 0, NULL);
                    }
                    else if (message->text == "Отписаться от расписания") {
                        bot.getApi().sendMessage(userId, "Используйте команду /unsubscribe", false, 0, NULL);
                    }
                    else if (message->text == "Подписаться на расписание группы") {

                        bot.getApi().sendMessage(userId, "Пример сообщения целиком: /sub_g ИСП-322р", false, 0, NULL);
                    }
                    else if (message->text == "Подписаться на отдельное расписание группы") {

                        bot.getApi().sendMessage(userId, "Пример сообщения целиком: /sub_go ИСП-322р", false, 0, NULL);

                    }
                    else if (message->text == "Подписаться на расписание преподавателя") {

                        bot.getApi().sendMessage(userId, "Пример сообщения целиком: /sub_p Авдуевская Н.С.", false, 0, NULL);
                    }
                    else if (message->text == "Подписаться на общее расписание") {

                        myUser* tUher = &subscribedUsers[UserNumber];

                        if (isMMessage) {
                            for (auto& a : UserNumbers)
                                if (subscribedUsers[a].group == -1)
                                    isNormalCommand = 0;

                            if (subscribedUsers[UserNumber].group == -1)
                                isNormalCommand = 0;

                            if (!isNormalCommand)
                                bot.getApi().sendMessage(userId, "Вы уже подписаны на общее расписание", false, 0, NULL);
                            else {
                                myUser MyUs;
                                MyUs.tgId = userId;
                                MyUs.group = -1;
                                MyUs.mode = 0;

                                subscribedUsers.insert(subscribedUsers.begin() + UserNumber + UserNumbers.size() + 1, MyUs);
                                tUher = &subscribedUsers[UserNumber + UserNumbers.size() + 1];
                            }

                        }

                        if (isNormalCommand) {
                            bot.getApi().sendMessage(userId, "Вы подписались на общее расписание", false, 0, NULL);
                            tUher->group = -1;
                            saveUsers();
                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1.png", "image/png"));
                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2.png", "image/png"));
                        }
                    }
                    else if (message->text == "Состояние подписки") {
                        string message, teacher;

                        if (subscribedUsers[UserNumber].group == -1)
                            message = "Вы подписаны на общее расписание";

                        else if (subscribedUsers[UserNumber].mode == 0)
                            message = "Вы подписаны на расписание с выделенной группой " + Groups[subscribedUsers[UserNumber].group];

                        else if (subscribedUsers[UserNumber].mode == 1)
                            message = "Вы подписаны на отдельное расписание группы " + Groups[subscribedUsers[UserNumber].group];

                        else if (subscribedUsers[UserNumber].mode == 2) {
                            teacher = subscribedUsers[UserNumber].Tea;
                            teacher.insert(teacher.end() - 6, 32);
                            message = "Вы подписаны на расписание преподавателя с ФИО " + teacher + ", и получаете расписание двух корпусов";
                        }


                        else if (subscribedUsers[UserNumber].mode == 3) {
                            teacher = subscribedUsers[UserNumber].Tea;
                            teacher.insert(teacher.end() - 6, 32);
                            message = "Вы подписаны на расписание преподавателя с ФИО " + teacher + ", и получаете расписание только корпуса с вами";
                        }


                        if (UserNumbers.size() > 0) {
                            message += "\n\nДоп. подписки:";

                            for (int i = 0; i < UserNumbers.size(); i++) {
                                message += "\n" + to_string(i + 1) + ") ";
                                int& a = UserNumbers[i];

                                if (subscribedUsers[a].group == -1)
                                    message += "Общее расписание";

                                else if (subscribedUsers[a].mode == 0)
                                    message += "Расписание с выделенной группой " + Groups[subscribedUsers[a].group];

                                else if (subscribedUsers[a].mode == 1)
                                    message += "Отдельное расписание группы " + Groups[subscribedUsers[a].group];

                                else if (subscribedUsers[a].mode == 2) {
                                    teacher = subscribedUsers[a].Tea;
                                    teacher.insert(teacher.end() - 6, 32);
                                    message += "Расписание преподавателя с ФИО " + teacher + ", и получаете расписание двух корпусов";
                                }

                                else if (subscribedUsers[a].mode == 3) {
                                    teacher = subscribedUsers[a].Tea;
                                    teacher.insert(teacher.end() - 6, 32);
                                    message += "Расписание преподавателя с ФИО " + teacher + ", и получаете расписание только корпуса с вами";
                                }

                            }
                        }

                        bot.getApi().sendMessage(userId, message, false, 0, NULL);
                    }
                    else if (message->text == "Расписание для групп") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, raspisForGroupKeyboard);
                    }
                    else if (message->text == "Расписание для преподавателей") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, raspisForTeacherKeyboard);
                    }
                    else if (message->text == "Доп. подписка") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, mSubscribeKeyboard);
                    }
                    else if (message->text == "Получить расписание") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, getRaspisKeyboard);
                    }
                    else if (message->text == "Назад") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, mainMenuKeyboard);
                    }
                    else if (message->text == "Получить общее расписание") {

                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1.png", "image/png"));
                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2.png", "image/png"));
                    }
                    else if (message->text == "Получить расписание преподавателя") {
                        bot.getApi().sendMessage(userId, "Пример сообщения целиком: /get_p Авдуевская Н.С.", false, 0, NULL);
                    }
                    else if (message->text == "Получить расписание группы") {

                        bot.getApi().sendMessage(userId, "Пример сообщения целиком: /get_g ИСП-322р", false, 0, NULL);
                    }
                    else if (message->text == "Получать расписание и для другого корпуса") {

                        if ((subscribedUsers[UserNumber].mode == 2 || subscribedUsers[UserNumber].mode == 3)) {
                            bot.getApi().sendMessage(userId, "Пример сообщения целиком: /t_dop да\nОписание: по умолчанию вы получаете расписание обоих корпусов, если вы хотите получить расписание только своего корпуса, используйте эту команду с параметром нет", false, 0, NULL);
                        }
                        else {
                            bot.getApi().sendMessage(userId, "Вы должны быть подписаны на расписание для преподавателя", false, 0, NULL);
                        }
                    }
                    else if (message->text == "Подписаться ещё на одно расписание") {

                        bot.getApi().sendMessage(userId, "Можно подписаться сразу на несколько различных расписаний, для этого перед командой подписки на определённое расписание нужно поставить \"/m \"\n\nПримеры:\n/m /sub_o\n/m /sub_go ИСП-322р\n/m /sub_p Трошкина", false, 0, mSubscribeKeyboard);
                    }
                    else if (message->text == "Отписаться от одного из расписаний") {

                        bot.getApi().sendMessage(userId, "Пример сообщения целиком: /unm 1\n1 - номер доп. подписки из /status", false, 0, mSubscribeKeyboard);
                    }
                    else if (message->text == "Вопросы и предложения") {

                        bot.getApi().sendMessage(userId, "Нашли баг или есть вопросы / предложения по функционалу?\nПишите сюда: @wyanarba", false, 0);
                    }

                    else if (isStandartMessage == 1) isStandartMessage = 0;
                }
                else {
                    if (message->text == "Подписаться на расписание") {
                        bot.getApi().sendMessage(userId, "Обработка...", false, 0, NULL);

                        // Добавляем пользователя в список подписанных
                        if (!isUserSubs) {
                            myUser MyUs;
                            MyUs.tgId = userId;
                            MyUs.group = -1;
                            MyUs.mode = 0;

                            if (userId == SecondRootTgId && subscribedUsers.size() > 1)//ну это так, бонус
                                subscribedUsers.insert(subscribedUsers.begin() + 1, MyUs);
                            else if (userId == RootTgId && subscribedUsers.size() > 1)//ну это так, бонус, и владельцу тоже
                                subscribedUsers.insert(subscribedUsers.begin(), MyUs);
                            else
                                subscribedUsers.push_back(MyUs);

                            saveUsers();

                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1.png", "image/png"));
                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2.png", "image/png"));
                        }
                        bot.getApi().sendMessage(userId, "Вы подписались на расписание.", false, 0, mainMenuKeyboard);

                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("..\\imgs\\ad2.png", "image/png"));
                        bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("..\\imgs\\ad3.png", "image/png"));
                        bot.getApi().sendMessage(userId, "Попробуйте более удобный формат расписания для групп / преподавателей.", false, 0);
                    }
                    else
                        bot.getApi().sendMessage(userId, "Сначала подпишитесь на расписание", false, 0, subscribeKeyboard);
                }

                if (mutedUsers.find(userId) == mutedUsers.end()) {
                    if (isStandartMessage != 0)
                        logMessage(to_string(userId) + " | " + message->chat->username + " | " + message->text, "messages");
                    else
                        logMessage(to_string(userId) + " | " + message->chat->username + " | " + message->text, "otherMessages");
                }
                else
                    logMessage(to_string(userId) + " | " + message->chat->username + " | " + message->text, "spamMessages");
            }
        }
        catch (const std::exception& e)
        {
            logMessage("(10) EROR | " + (string)e.what() + " | by_user: " + to_string(message->chat->id) + " | message: " + message->text, "system", 53);
        }
        });

    //работа бота
    bool succsesStart = 0;
    while (!succsesStart) {
        try {

            printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
            bot.getApi().deleteWebhook(); // Удаление веб хука для использования long polling
            TgBot::TgLongPoll longPoll(bot);

            thread th(main2);

            succsesStart = 1;
            logMessage("бот запущен!", "system");

            while (true) {
                try
                {
                    longPoll.start();
                }
                catch (const std::exception& e)
                {
                    if (std::string(e.what()).find("system:1100") != std::string::npos) {
                        continue;
                    }
                    else {
                        logMessage("EROR | " + (string)e.what(), "system", 1);

                        continue;
                    }

                }
                update();
            }
        }
        catch (const std::exception& e) {
            logMessage("EROR | " + (string)e.what(), "system", 2);
            continue;
        }
    }

    return 0;
}