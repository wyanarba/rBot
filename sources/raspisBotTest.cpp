
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
    {"get_o", "Посмотреть общее расписание"},
    {"get_g", "Посмотреть расписание группы"},
    {"get_p", "Посмотреть расписание преподавателя"},
    {"m", "Доп. подписка"},
    {"unm", "отписаться от доп. подписки"},
    {"unsubscribe", "Отписаться от расписания"}
};

map<string, vector<__int32>> Commands2{
    {"m", {0, 0}},

    {"sub_g", {1, 0}},
    {"sub_go", {1, 1}},
    {"get_g", {1, 2}},

    {"sub_p", {2, 0}},
    {"get_p", {2, 1}},

    {"sub_o", {3, 0}},
    {"get_o", {3, 1}},

    {"unsubscribe", {4, 0}},
    {"status", {4, 1}},
    {"t_dop", {4, 2}},
    {"unm", {4, 3}},
    {"un_button", {4, 4}},
    {"start", {4, 5}},

    {"qq", {5, 0}},
    {"q", {5, 1}},
    {"stats", {5, 2}},
    {"info", {5, 3}},
    {"tea", {5, 4}},
    {"get_us", {5, 5}},
    {"update", {5, 6}},

    {"mut", {6, 0}},
    {"unmut", {6, 1}},
    {"ban", {6, 2}},
    {"unban", {6, 3}},
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
string StartText = "";//приветственное сообщение
bool ModeSend = 0;//режим отправки 0 - v1, 1 - v2
//DWORD SleepTime = 60000;
void (*update)();//функция для отправки расписания, (указатель) на неё
//bool EnableAd = 1;
//bool EnableAutoUpdate = 1;

//bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("1.png", "image/png"));
//bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile("2.png", "image/png"));

string formatG(string str) {
    if (str == "")
        return "";

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
    if (str == "")
        return "";

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

__int32 findCommand(string message, int& param2) {
    if (message.find(' ') != string::npos)
        message = message.substr(1, message.find(' ') - 1);
    else
        message = message.substr(1, message.size() - 1);

    if (message.find('@') != string::npos)
        message = message.substr(0, message.find('@'));

    auto a = Commands2.find(message);

    if (a != Commands2.end()) {
        param2 = a->second[1];
        return a->second[0];
    }
    else 
        return -1;
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
            else if (parameter == "GroupId" && GroupsForSpam.size() < 31)
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

    for (int i = 0; i < rb::DisabledGroups.size(); i++)
        rb::DisabledGroups[i] = 1;

    std::ofstream outputFile("..\\users.txt");
    outputFile << "v2\n";

    for (const auto& us : subscribedUsers) {
        if (us.mode != 2 && us.mode != 3)
            outputFile << us.tgId << ' ' << us.group << ' ' << (char)(us.mode + '0') << endl;  // Записываем оставшиеся строки
        else
            outputFile << us.tgId << ' ' << us.Tea << ' ' << (char)(us.mode + '0') << endl;  // Записываем оставшиеся строки

        if (us.mode < 2 && us.group != -1 && rb::DisabledGroups[us.group] == 1)
            rb::DisabledGroups[us.group] = 0;
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

void updateUsersF(int version) {
    if (version == 0) {
        const vector<string> oldGroups = { "ДО-124", "ДО-223", "ДО-322", "ДО-421", "ИИС-124", "ИИС-223", "ИИС-322", "ИИС-421", "ИКС-124",
            "ИКС-223", "ИКС-322", "ИКС-421", "ИСП-124а", "ИСП-124ир", "ИСП-124ис", "ИСП-124п", "ИСП-124р", "ИСП-124т", "ИСП-223а",
            "ИСП-223ир", "ИСП-223ис", "ИСП-223п", "ИСП-223р", "ИСП-223т", "ИСП-322а", "ИСП-322ир", "ИСП-322ис", "ИСП-322п", "ИСП-322р",
            "ИСП-322т", "ИСП-421а", "ИСП-421ир", "ИСП-421ис", "ИСП-421п", "ИСП-421р", "ИСП-421т", "ОИБ-124", "ОИБ-223", "ОИБ-322",
            "ОИБ-421", "ОПС-124", "ОПС-223", "ОПС-322", "ОПС-421", "СИС-124", "СИС-223", "СИС-322", "СИС-421", "ТО-124", "ТО-223",
            "ТО-322", "ТО-421", "ЭСС-124", "ЭСС-223", "ЭСС-322", "ЭСС-421", "МТО-124", "МТО-223", "МТО-322", "МТО-421"
        };

        vector<int> convTable;
        for (auto& name : oldGroups) {
            auto pos = find(Groups.begin(), Groups.end(), name);
            convTable.push_back(pos == Groups.end() ? 0 : pos - Groups.begin());
        }

        for (auto& user : subscribedUsers) {
            if (user.group > -1) {
                user.group = convTable[user.group];
            }
        }

    }
}

bool IsNormalCfg = getConfig("..\\config.txt");
TgBot::Bot bot(BotKey);

void updateV2() {

    try
    {
        //ожидание сообщения об отправке от ядра
        {
            rb::mtx1.lock();
            if (rb::syncMode == 1) {

                try {
                    if (isUpdate && RootTgId != 0) {
                        bot.getApi().sendMessage(RootTgId, "Обнова (качается и устанавливается сама)\n" + CurrentVers + " -> " + newVersion +
                            "\n\nПодробнее об оновлении:\nhttps://t.me/backgroundbotvksit", false, 0, NULL);

                        bot.getApi().sendMessage(SecondRootTgId, "Обнова (качается и устанавливается сама)\n" + CurrentVers + " -> " + newVersion +
                            "\n\nПодробнее об оновлении:\nhttps://t.me/backgroundbotvksit", false, 0, NULL);
                    }
                }
                catch (const std::exception& e)
                {
                    string str = e.what();
                    logMessage(std::format("Error: Не удалось скинуть сообщение о обновлении - {}", e.what()), "system", 222);
                }
                
                bool wait = 1;
                rb::syncMode = 2;

                rb::mtx1.unlock();

                while (wait) {
                    this_thread::sleep_for(300ms);
                    rb::mtx1.lock();
                    wait = rb::syncMode != 3;
                    rb::mtx1.unlock();
                }
            }
            else {
                rb::mtx1.unlock();
                return;
            }
        }
            

        
        int triesToSend = 0;//попытки отправки одному человеку
        int edgeGroup = 0;// кол-во файлов на буферную группу
        corps& corp = rb::corpss[rb::currentCorps];

        if (!rb::ErrorOnCore) {//расписание успешно обработано

            logMessage("Отправка расписания в группу", "system", 3);

            // Отправка в группу
            {
                int countG = 0, localCount = 0;//кол-во групп 1, кол-во групп 2

                for (auto& mPage : corp.pages) {
                    if (!mPage.isEmpty) {
                        for (int i = 0; i < mPage.groups.size(); i++)
                            if (mPage.groups[i].isExists && !rb::DisabledGroups[i])
                                countG++;

                        countG++;
                    }
                }

                countG = countG / GroupsForSpam.size() + 1;


                //отправка в группу
                bool success = 0;
                
                for (auto& mPage : corp.pages) {
                    if (mPage.isEmpty)
                        continue;


                    for (int i = 0; i < mPage.groups.size(); i++) {
                        try 
                        {
                            auto& group = mPage.groups[i];

                            if (group.isExists && !rb::DisabledGroups[i]) {
                                localCount++;
                                group.idSpam = (localCount / countG) % GroupsForSpam.size();

                                auto message = bot.getApi().sendPhoto(GroupsForSpam[group.idSpam], 
                                    TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + "\\" + Groups1251[i] + ".png", "image/png"));
                                group.messageId = message->messageId;


                                if (mPage.IsNewPage || group.changed) {
                                    auto message = bot.getApi().sendPhoto(GroupsForSpam[group.idSpam],
                                        TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + "\\" + Groups1251[i] + "S.png", "image/png"));
                                    group.messageIdS = message->messageId;
                                }
                            }
                        }
                        catch (const std::exception& e)
                        {
                            logMessage(std::format("Error: {} | {}", e.what(), Groups[i]), "system", 4);
                            i--;
                            localCount--;
                        }
                    }
                    while (!success) {
                        try
                        {
                            auto message = bot.getApi().sendPhoto(GroupsForSpam[GroupsForSpam.size() - 1], 
                                TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));
                            mPage.mi = message->messageId;
                            success = 1;
                        }
                        catch (const std::exception& e)
                        {
                            logMessage(std::format("Error: {}", e.what()), "system", 5);
                        }
                    }
                    success = 0;
                }
            }


            logMessage("Отправка расписания людям", "system", 8);

            //рассылка
            for (int i = 0; i < subscribedUsers.size(); i++) {
                auto& us = subscribedUsers[i];
                triesToSend = 0;
                try {
                    for (auto& mPage : corp.pages) {
                        if (mPage.isEmpty)
                            continue;

                        if (us.group == -1) {// общее
                            bot.getApi().copyMessage(us.tgId, GroupsForSpam[GroupsForSpam.size() - 1], mPage.mi);
                        }
                        else if (us.mode == 0 && mPage.groups[us.group].isExists) {
                            bot.getApi().copyMessage(us.tgId, GroupsForSpam[mPage.groups[us.group].idSpam], mPage.groups[us.group].messageId);
                        }
                        else if (us.mode == 1 && mPage.groups[us.group].isExists && (mPage.IsNewPage || mPage.groups[us.group].changed)) {
                            bot.getApi().copyMessage(us.tgId, GroupsForSpam[mPage.groups[us.group].idSpam], mPage.groups[us.group].messageIdS);
                        }
                        else if (us.mode == 2 || us.mode == 3) {
                            if (mPage.Teachers.find(us.Tea) != mPage.Teachers.end())
                                bot.getApi().sendPhoto(us.tgId, TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + "\\" + Utf8_to_cp1251(us.Tea.c_str()) + ".png", "image/png"));
                            else if (us.mode == 2)
                                bot.getApi().copyMessage(us.tgId, GroupsForSpam[GroupsForSpam.size() - 1], mPage.mi);
                        }

                        triesToSend = 0;
                    }
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

            //удаление из группы
            {
                int tryingToDelete = 0;
                bool success = 0;

                for (auto& mPage : corp.pages) {
                    if (mPage.isEmpty)
                        continue;

                    for (int i = 0; i < mPage.groups.size(); i++) {
                        try
                        {
                            tryingToDelete = 0;

                            auto& group = mPage.groups[i];

                            if (group.isExists && !rb::DisabledGroups[i]) {
                                bot.getApi().deleteMessage(GroupsForSpam[group.idSpam], group.messageId);

                                if (mPage.IsNewPage || group.changed)
                                    bot.getApi().deleteMessage(GroupsForSpam[group.idSpam], group.messageIdS);

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
                            bot.getApi().deleteMessage(GroupsForSpam[GroupsForSpam.size() - 1], mPage.mi);
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
                    success = 0;
                }
            }

            //сохранение пользователей
            saveUsers();

            logMessage("Конец отправки", "system", 21);
        }
        else {//отработка ощибки
            //отправка в группу
            bool success = 0;

            //отправка в группу
            for (auto& mPage : corp.pages) {
                if (mPage.isEmpty)
                    continue;

                while (!success) {
                    try
                    {
                        auto message = bot.getApi().sendPhoto(GroupsForSpam[GroupsForSpam.size() - 1],
                            TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));
                        mPage.mi = message->messageId;
                        success = 1;
                    }
                    catch (const std::exception& e)
                    {
                        logMessage(std::format("Error: {}", e.what()), "system", 5);
                    }
                }
                success = 0;
            }
            
            //рассылка
            for (int i = 0; i < subscribedUsers.size(); i++) {
                auto& us = subscribedUsers[i];

                for (auto& mPage : corp.pages) {

                    try {
                        if (mPage.isEmpty)
                            continue;

                        bot.getApi().copyMessage(us.tgId, GroupsForSpam[GroupsForSpam.size() - 1], mPage.mi);
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

            
            //удаление из группы
            {
                int tryingToDelete = 0;
                bool success = 0;

                for (auto& mPage : corp.pages) {
                    if (mPage.isEmpty)
                        continue;

                    tryingToDelete = 0;
                    while (!success) {
                        try
                        {
                            bot.getApi().deleteMessage(GroupsForSpam[GroupsForSpam.size() - 1], mPage.mi);
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
                    success = 0;
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

    rb::mtx1.lock();
    rb::syncMode = 0;
    rb::mtx1.unlock();
}

void updateV1() {

    try
    {
        //ожидание сообщения об отправке от ядра
        {
            rb::mtx1.lock();
            if (rb::syncMode == 1) {

                try {
                    if (isUpdate && RootTgId != 0) {
                        bot.getApi().sendMessage(RootTgId, "Обнова (качается и устанавливается сама)\n" + CurrentVers + " -> " + newVersion +
                            "\n\nПодробнее об оновлении:\nhttps://t.me/backgroundbotvksit", false, 0, NULL);

                        bot.getApi().sendMessage(SecondRootTgId, "Обнова (качается и устанавливается сама)\n" + CurrentVers + " -> " + newVersion +
                            "\n\nПодробнее об оновлении:\nhttps://t.me/backgroundbotvksit", false, 0, NULL);
                    }
                }
                catch (const std::exception& e)
                {
                    string str = e.what();
                    logMessage(std::format("Error: Не удалось скинуть сообщение о обновлении - {}", e.what()), "system", 222);
                }

                bool wait = 1;
                rb::syncMode = 2;

                rb::mtx1.unlock();

                while (wait) {
                    this_thread::sleep_for(300ms);
                    rb::mtx1.lock();
                    wait = rb::syncMode != 3;
                    rb::mtx1.unlock();
                }
            }
            else {
                rb::mtx1.unlock();
                return;
            }
        }


        int triesToSend = 0;//попытки отправки одному человеку
        int edgeGroup = 0;// кол-во файлов на буферную группу
        corps& corp = rb::corpss[rb::currentCorps];

        if (!rb::ErrorOnCore) {//расписание успешно обработано

            logMessage("Отправка расписания людям", "system", 8);

            //рассылка
            for (int i = 0; i < subscribedUsers.size(); i++) {
                auto& us = subscribedUsers[i];
                try {
                    for (auto& mPage : corp.pages) {
                        if (mPage.isEmpty)
                            continue;


                        if (us.group == -1) {// общее
                            bot.getApi().sendPhoto(us.tgId, TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));
                        }
                        else if (us.mode == 0 && mPage.groups[us.group].isExists) {
                            bot.getApi().sendPhoto(us.tgId,
                                TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + "\\" + Groups1251[subscribedUsers[i].group] + ".png", "image/png"));
                        }
                        else if (us.mode == 1 && mPage.groups[us.group].isExists && (mPage.IsNewPage || mPage.groups[us.group].changed)) {
                            bot.getApi().sendPhoto(us.tgId,
                                TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + "\\" + Groups1251[subscribedUsers[i].group] + "S.png", "image/png"));
                        }
                        else if (us.mode == 2 || us.mode == 3) {
                            if (mPage.Teachers.find(us.Tea) != mPage.Teachers.end())
                                bot.getApi().sendPhoto(us.tgId, TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + "\\" + Utf8_to_cp1251(us.Tea.c_str()) + ".png", "image/png"));
                            else if (us.mode == 2)
                                bot.getApi().sendPhoto(us.tgId, TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));


                            triesToSend = 0;
                        }

                    }
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

            //сохранение пользователей
            saveUsers();

            logMessage("Конец отправки", "system", 21);
        }
        else {//отработка ошибки
            //отправка в группу
            bool success = 0;

            logMessage("Отправка расписания людям", "system", 8);

            //рассылка
            for (int i = 0; i < subscribedUsers.size(); i++) {
                auto& us = subscribedUsers[i];
                try {
                    for (auto& mPage : corp.pages) {
                        if (mPage.isEmpty)
                            continue;

                        bot.getApi().sendPhoto(us.tgId, TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));

                        triesToSend = 0;
                    }
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


            //сохранение пользователей
            saveUsers();
        }

    }
    catch (const std::exception& e)
    {
        logMessage("УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС УЖАССССС 11) EROR | " + (string)e.what(), "system", 31);
    }

    rb::mtx1.lock();
    rb::syncMode = 0;
    rb::mtx1.unlock();
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

    //инициализация переменных
    {
        genGroups();

        rb::corpss.push_back(corps("spo.pdf", 0));
        rb::corpss.push_back(corps("npo.pdf", 1));
        rb::DisabledGroups.assign(Groups.size(), 1);

        for (int i = 0; i < Groups.size(); i++) {
            Groups1251.push_back(Utf8_to_cp1251(Groups[i].c_str()));
        }

        ifstream ifs("..\\spamText.txt");
        string strForRead;
        while (getline(ifs, strForRead)) {
            spamText.insert(strForRead);
        }
        ifs.close();
    }

    //чтение папок
    {
        string currentFile;

        for (corps& corp : rb::corpss) {
            for (auto& page : corp.pages) {
                bool groupF = 0;//было ли расписание групп

                for (const auto& entry : fs::directory_iterator(rb::imgPath + page.folderName)) {
                    currentFile = cp1251_to_utf8(entry.path().filename().string().c_str());

                    if (currentFile.find(".png") != string::npos) {
                        currentFile = currentFile.erase(currentFile.size() - 4);
                        int groupId = findGroup(currentFile);

                        if (groupId != -1) {
                            page.groups[groupId] = 1;
                            groupF = 1;
                        }
                        else if (currentFile[currentFile.size() - 1] != 'S')
                            page.Teachers.insert(currentFile);
                    }
                }

                if (page.Teachers.size() != 0 || groupF) {
                    page.isEmpty = 0;
                    corp.pagesUse++;
                }
                else
                    page.isEmpty = 1;
            }
        }

        ifstream ifs(rb::imgPath + "4\\t.txt");
        while (getline(ifs, currentFile)) {
            rb::AllTeachers.insert(currentFile);
        }
    }

    //инициализация пользователей
    {
        string str;
        int version = 0;
        ifstream ifs("..\\users.txt");

        getline(ifs, str);
        if (str != "v2") {
            ifs.close();
            ifs.open("..\\users.txt");
        }
        else
            version = 2;
        
        while (getline(ifs, str)) {
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

                if (myU.mode < 2 && myU.group != -1 && rb::DisabledGroups[myU.group] == 1)
                    rb::DisabledGroups[myU.group] = 0;
            }
        }
        ifs.close();

        if (version == 0) {
            updateUsersF(0);
            saveUsers();
            logMessage("Переход на пользователей v2 выполнен", "system.txt");
        }
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
        TgBot::KeyboardButton::Ptr mainMenuButton3(new TgBot::KeyboardButton("Посмотреть расписание"));
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


        //Посмотреть расписание
        getRaspisKeyboard->resizeKeyboard = true;
        //
        TgBot::KeyboardButton::Ptr getRaspisButton1(new TgBot::KeyboardButton("Посмотреть общее расписание"));
        TgBot::KeyboardButton::Ptr getRaspisButton2(new TgBot::KeyboardButton("Посмотреть расписание преподавателя"));
        TgBot::KeyboardButton::Ptr getRaspisButton3(new TgBot::KeyboardButton("Посмотреть расписание группы"));
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
                int64_t& userId = message->chat->id;
                string& text = message->text;

                int UserNumber = subscribedUsers.size();// номер пользователя в массиве
                vector<int> UserNumbers;// номера доп подписок пользователя
                for (int i = 0; i < subscribedUsers.size(); i++) {

                    if (subscribedUsers[i].tgId == message->chat->id) {
                        if (UserNumber == subscribedUsers.size())
                            UserNumber = i;
                        else
                            UserNumbers.push_back(i);
                    }
                }
                bool isUserSubs = UserNumber != subscribedUsers.size();
                //bool isMMessage = 0;
                int8_t isStandartMessage = 1;// для логгирования


                if (isUserSubs) {
                    if (text[0] == '/') {
                        myUser* user = &subscribedUsers[UserNumber];// ссылка на пользователя
                        int commandId2 = -100, commandId = findCommand(text, commandId2);// классификация команд
                        string answerText;// текстовый ответ

                        string commandParam;
                        if (text.find(' ') != string::npos)
                            commandParam = text.substr(text.find(' ') + 1);
                        


                        if (commandId == 0) {
                            if (commandId2 == 0) {
                                if (commandParam == "") {
                                    answerText = "Пример:\n\"/m /sub_go ИСП-322ир\"\n\nПояснение:\n\
/m - даёт возможность подписаться сразу на несколько расписаний, список подписок в /status";
                                }
                                else {
                                    text = commandParam;
                                    commandId = findCommand(text, commandId2);

                                    if (text.find(' ') != string::npos)
                                        commandParam = text.substr(text.find(' ') + 1);


                                    if (UserNumbers.size() > 3) {
                                        commandId = -10;
                                        isStandartMessage = 2;
                                        answerText = "Нельзя подписаться более чем на 5 различных расписаний";
                                    }
                                    else {
                                        myUser MyUs;
                                        MyUs.tgId = userId;
                                        MyUs.group = -1;
                                        MyUs.mode = 101;

                                        subscribedUsers.insert(subscribedUsers.begin() + UserNumber + UserNumbers.size() + 1, MyUs);
                                        user = &subscribedUsers[UserNumber + UserNumbers.size() + 1];

                                        cout << 123123123;
                                    }
                                }

                            }
                        }

                        if (commandId == 1) {// группы
                            commandParam = formatG(commandParam);
                            int group = findGroup(commandParam);
                            
                            if (commandParam == "") {
                                answerText = "Добавьте группу в сообщение, пример:\n\"/команда ИСП-322р\"";
                            }
                            else if (group == -1) {
                                answerText = "Группа "+ commandParam +" не найдена";
                            }
                            else if (commandId2 == 0) { // sub_g
                                answerText = "Вы подписались на расписание группы " + commandParam;

                                user->group = group;
                                user->mode = 0;
                                saveUsers();

                                commandId2 = 100;
                            }
                            else if (commandId2 == 1) { // sub_go
                                answerText = "Вы подписались на отдельное расписание группы " + commandParam;

                                user->group = group;
                                user->mode = 1;
                                saveUsers();

                                commandId2 = 101;
                            }
                            else if (commandId2 == 2) { // get_g
                                commandId2 = 101;
                            }

                            if (commandId2 == 100) {// рассылка фото с группой
                                bool isSanded = 0;

                                for (corps& corp : rb::corpss) {
                                    for (auto& page : corp.pages) {
                                        if (page.groups[group].isExists) {
                                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile(rb::imgPath + page.folderName + "\\"
                                                + Utf8_to_cp1251(Groups[group].c_str()) + ".png", "image/png"));
                                            isSanded = 1;
                                        }
                                    }
                                }

                                if(!isSanded)
                                    bot.getApi().sendMessage(userId, "Расписание для группы не найдено(", false, 0, NULL);
                            }
                            else if (commandId2 == 101) {// рассылка маленького фото с группой
                                bool isSanded = 0;

                                for (corps& corp : rb::corpss) {
                                    for (auto& page : corp.pages) {
                                        if (page.groups[group].isExists) {
                                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile(rb::imgPath + page.folderName + "\\"
                                                + Utf8_to_cp1251(Groups[group].c_str()) + "S.png", "image/png"));
                                            isSanded = 1;
                                        }
                                    }
                                }

                                if (!isSanded)
                                    bot.getApi().sendMessage(userId, "Расписание для группы не найдено(", false, 0, NULL);
                            }
                        }
                        else if (commandId == 2) {// преподаватели
                            commandParam = formatT(commandParam);
                            string altTeacher = Utf8_to_cp1251(commandParam.c_str());
                            bool isActualTea = 1, isUnusualName = 0;
                            // проверка на правильность
                            {
                                isActualTea = isActualTea && altTeacher[0] < -32 && altTeacher[0] > -65;
                                for (int i = 1; i < altTeacher.size(); i++) {
                                    isActualTea = isActualTea && (altTeacher[i] < 0 && altTeacher[i] > -33) || altTeacher[i] == 46;//-1 я, -64 А
                                }

                                if (isActualTea) {
                                    isUnusualName = find(rb::AllTeachers.begin(), rb::AllTeachers.end(), commandParam) == rb::AllTeachers.end();
                                }
                            }


                            if (commandParam == "") {
                                answerText = "Добавьте ФИО преподавателя в сообщение\nпример:\n\"/команда Авдуевская Н.С.\"";
                            }
                            else if (!isActualTea) {
                                answerText = "Некорректное имя преподавателя";
                            }
                            else if (isUnusualName) {
                                bot.getApi().sendMessage(userId, "Предупреждение: \"" + commandParam + "\" - ФИО преподавателя выглядит не обычно,\
 оно не было замечено в пред идущих расписаниях, лучше перепроверьте его", false, 0, NULL);
                            }
                            else if (commandId2 == 0) {
                                answerText = "Вы подписались на расписание преподавателя " + commandParam;

                                user->Tea = commandParam;
                                user->mode = 3;
                                user->group = 0;
                                saveUsers();

                                commandId2 = 100;
                            }
                            else if (commandId2 == 1) {
                                commandId2 = 100;
                            }

                            if (commandId2 == 100) {
                                bool isSanded = 0;

                                for (corps& corp : rb::corpss) {
                                    for (auto& page : corp.pages) {
                                        if (page.Teachers.find(commandParam) != page.Teachers.end()) {
                                            bot.getApi().sendPhoto(userId, TgBot::InputFile::fromFile(rb::imgPath + page.folderName + "\\"
                                                + altTeacher + ".png", "image/png"));

                                            isSanded = 1;
                                        }
                                    }
                                }

                                if (!isSanded)
                                    bot.getApi().sendMessage(userId, "Расписание для преподавателя не найдено(", false, 0, NULL);
                            }
                        }
                        else if (commandId == 3) {

                            if (commandId2 == 0) {
                                answerText = "Вы подписались на общее расписание";
                                user->group = -1;
                                user->mode = 0;
                                saveUsers();
                            }
                            
                            for (auto& corp : rb::corpss) {
                                for (auto& mPage : corp.pages) {
                                    if (mPage.isEmpty)
                                        continue;

                                    bot.getApi().sendPhoto(userId,
                                        TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));
                                }
                            }
                        }
                        else if (commandId == 4) {
                            //{ "unsubscribe", { 4, 0 } },
                            //{ "status", {4, 1} },
                            //{ "t_dop", {4, 2} },
                            //{ "unm", {4, 3} },
                            //{ "un_button", {4, 4} },
                            //{ "start", {4, 5} },

                            if (commandId2 == 0) {// unsubscribe

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

                                answerText = "Вы отписались от расписания.";
                            }
                            else if (commandId2 == 1) {// status
                                text = "Состояние подписки";
                            }
                            else if (commandId2 == 2) {// t_dop

                                bool isValidUser = 0;
                                int* firstT = NULL;

                                // поиск первой подписки с преподавателем
                                {
                                    if (subscribedUsers[UserNumber].mode == 2 || subscribedUsers[UserNumber].mode == 3) {
                                        firstT = &UserNumber;
                                        isValidUser = 1;
                                    }
                                    else for (int& a : UserNumbers) {
                                        if (subscribedUsers[a].mode == 2 || subscribedUsers[a].mode == 3) {
                                            isValidUser = 1;
                                            if (firstT == NULL)
                                                firstT = &a;
                                        }
                                    }
                                }
                                    
                                if (!isValidUser) {
                                    answerText = "Вы должны быть подписаны хотя бы на одно расписание для преподавателей";
                                }
                                else if (commandParam == "нет") {
                                    subscribedUsers[*firstT].mode = 3;
                                    saveUsers();
                                    answerText = "Теперь вы не будете получать расписание, где нет вас";
                                }
                                else if (commandParam == "да") {
                                    subscribedUsers[*firstT].mode = 2;
                                    saveUsers();
                                    answerText = "Теперь вы снова будете получать расписание, даже если на нём нет вас";
                                }
                                else {
                                    answerText = "Пример:\n\"/t_dop нет\"\n\nПояснение:\nнет - вы не будете получать то расписание, на котором нету вас\
\nда - вы будете получать расписание, даже если на нём нет вас";
                                }
                            }
                            else if (commandId2 == 3) {// unm

                                if (commandParam != "" && -1 < stoi(commandParam) - 1 && stoi(commandParam) - 1 < UserNumbers.size()) {
                                    subscribedUsers.erase(subscribedUsers.begin() + UserNumbers[stoi(commandParam) - 1]);
                                    answerText = "Вы отписались от доп подписки под номером " + commandParam;
                                    saveUsers();
                                }
                                else if (commandParam == "") {
                                    answerText = "Пример:\n\"/unm 1\"\n1 - номер доп подписки в /status";
                                }
                                else {
                                    answerText = "Не верный номер доп. подписки";
                                }
                            }
                            else if (commandId2 == 4) {// unbutton
                                ReplyKeyboardRemove::Ptr removeKeyboard(new ReplyKeyboardRemove);
                                bot.getApi().sendMessage(userId, "Клавиатура удалена\n\nЧто бы вернуть используйте /start", false, 0, removeKeyboard);
                            }

                        }
                        else if (commandId == 5 && (userId == RootTgId || userId == SecondRootTgId)) {
                            //{ "qq", { 5, 0 } },
                            //{ "q", {5, 1} },
                            //{ "stats", {5, 2} },
                            //{ "info", {5, 3} },
                            //{ "tea", {5, 4} },
                            //{ "get_us", {5, 5} },
                            //{ "update", {5, 6} },

                            if (commandId2 == 0) {// qq
                                if (commandParam == "1")
                                    bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("..\\system.txt", "text/plain"));
                                else if (commandParam == "2")
                                    bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile(rb::imgPath + "4\\t.txt", "text/plain"));
                                else if (commandParam == "3")
                                    bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("..\\spamText.txt", "text/plain"));
                                else if (commandParam == "4")
                                    bot.getApi().sendDocument(userId, TgBot::InputFile::fromFile("..\\updater\\log.txt", "text/plain"));
                                else
                                    answerText = "Пример|\n\"/qq 1\"\nНомера расписаны в /info";
                            }
                            else if (commandId2 == 1) {// q
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
                            else if (commandId2 == 2) {// stats
                                int g = 0, go = 0, p = 0, o = 0;

                                // подсчёт
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

                                answerText = "Пользователи: " + to_string(subscribedUsers.size()) +
                                    "\nПреподаватели (всего): " + to_string(rb::AllTeachers.size()) +
                                    "\nРежим рассылки: " + to_string(ModeSend + 1) +
                                    "\nБуферных групп: " + to_string(GroupsForSpam.size()) +
                                    "\nЗадержка проверки: " + to_string(SleepTime / 1000) +
                                    "\nРеклама: " + (EnableAd ? "Вкл." : "Выкл.") +
                                    "\nАвто обнова: " + (EnableAutoUpdate ? "Вкл." : "Выкл.") +
                                    "\nP: " + to_string(p) +
                                    "\nG: " + to_string(g) +
                                    "\nGO: " + to_string(go) +
                                    "\nO: " + to_string(o);
                            }
                            else if (commandId2 == 3) {
                                answerText = "вопросы и предложения tg: @wyanarba\n\nпро доступ:\nу меня (создателя бота):\
\n1) команды /q и /stats\n2) так же возможность скачивать некоторые файлы (/qq число)\
\n1 - файл с системными логами ошибками и тд. system.txt\
\n2 - файл с преподавателями которые были встречены в расписании когда либо 4/t.txt\
\n3 - файл с списком слов для фильтрации spamText.txt\
\n4 - файл с логом авто обновления\
\nвсе эти файлы нужны мне для улучшения бота\
\n\nу тебя (владельца бота):\
\n1 - сообщение пользователю от лица бота: (telegram id человека)Текст сообщения\
\n2 - сообщение всем пользователям: )(Текст сообщения\
\nвсе остальные команды из /q, а так же те, что были перечислены выше\
\nпри подписке на расписание на первое место в очереди попадает владелец, на второе я, дальше кто как успеет";
                            }
                            else if (commandId2 == 4) {// tea
                                int i = 0;

                                for (auto& a : subscribedUsers)
                                    if (a.mode > 1 && a.mode < 4) {
                                        i++;
                                        Chat::Ptr chat = bot.getApi().getChat(a.tgId);
                                        answerText += std::format("{}) {}:\n    @{},\n    {}\n", i, a.Tea, (chat->username == "" ? "Тега нет(" : chat->username), a.tgId);
                                    }
                            }
                            else if (commandId2 == 5) {// get_us
                                if (commandParam == "") {
                                    answerText = "Пример:\n\"/get_us 1234567890\"\n1234567890 - tgId человека";
                                }
                                else {
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

                                    answerText = std::format("@{}\n\n{}",
                                        (chat->username == "" ? "Тега нет(" : chat->username),
                                        message);
                                }
                            }
                            else if (commandId2 == 6) {
                                answerText = "Проверка обновления пройдёт скорее";
                                tryesChek = 0;
                            }
                        }
                        else if (commandId == 6 && userId == RootTgId) {
                            //{ "mut", { 6, 0 } },
                            //{ "unmut", {6, 1} },
                            //{ "ban", {6, 2} },
                            //{ "unban", {6, 3} },

                            if (commandId2 == 0) {// mut
                                if (commandParam != "" && mutedUsers.find(stoll(commandParam)) == mutedUsers.end()) {
                                    if (stoll(commandParam) != RootTgId && stoll(commandParam) != SecondRootTgId) {
                                        mutedUsers.insert(stoll(commandParam));
                                        saveBadUsers(0);
                                        answerText = "Сообщения этого человека не будут записываться в логах";
                                    }
                                    else
                                        answerText = "Не надо мутить владельца или создателя";
                                }
                                else {
                                    if (commandParam == "") {
                                        answerText = "Пример:\n\"\/mut 1234567890\"\n1234567890 - tgId человека\n/mut\nбез параметров выводит это\n\
при муте, сообщения человека не логируются\n\nСписок замученных:\n";

                                        for (auto& a : mutedUsers) {
                                            answerText += to_string(a);
                                        }
                                    }
                                    else
                                        answerText = "Пользователь уже есть в списке";
                                }
                            }
                            else if (commandId2 == 1) {
                                if (commandParam != "" && mutedUsers.find(stoll(commandParam)) != mutedUsers.end()) {
                                    mutedUsers.erase(mutedUsers.find(stoll(commandParam)));
                                    saveBadUsers(0);
                                    answerText = "Сообщения этого человека теперь снова логируются";
                                }
                                else {
                                    if (commandParam == "")
                                        answerText = "Пример:\n\"\/unmut 1234567890\"\n1234567890 - tgId человека\n/unmut - отменяет действие /mut";
                                    else
                                        answerText = "Пользователь уже убран из списка";
                                }
                            }
                            else if (commandId2 == 2) {
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

                                        answerText = "Пользователь заблокирован";
                                    }
                                    else
                                        answerText = "Не надо банить владельца или создателя";
                                }
                                else {
                                    if (commandParam == "") {
                                        answerText = "Пример:\n\"\/ban 1234567890\"\n1234567890 - tgId человека\n/ban\nбез параметров выводит это\n\
при бане, человек больше не может использовать бота\n\nСписок забаненных:\n";

                                        for (auto& a : blockedUsers) {
                                            answerText += to_string(a);
                                        }

                                    }
                                    else
                                        answerText = "Пользователь уже есть в списке";
                                }
                            }
                            else if (commandId2 == 3) {
                                if (commandParam != "" && blockedUsers.find(stoll(commandParam)) != blockedUsers.end()) {
                                    blockedUsers.erase(blockedUsers.find(stoll(commandParam)));
                                    saveBadUsers(1);
                                    answerText = "Теперь этот человек снова имеет доступ к боту";
                                }
                                else {
                                    if (commandParam == "")
                                        answerText = "Пример:\n\"\/unban 1234567890\"\n1234567890 - tgId человека\n/unmut - отменяет действие /ban";
                                    else
                                        answerText = "Пользователь уже убран из списка";
                                }
                            }
                        }
                        else {
                            isStandartMessage = 0;
                            //answerText = "Команда не распознана(";
                        }


                        // ответ пользователю текстом
                        if (answerText != "") {// текстовый ответ
                            bot.getApi().sendMessage(userId, answerText, false, 0, NULL);
                        }

                        // неверная команда с /m
                        if (user->mode == 101) {
                            subscribedUsers.erase(subscribedUsers.begin() + UserNumber + UserNumbers.size() + 1);
                        }
                        
                        // для логов
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

                        bot.getApi().sendMessage(userId, "Пример:\n\"/sub_g ИСП-322р\"", false, 0, NULL);
                    }
                    else if (message->text == "Подписаться на отдельное расписание группы") {

                        bot.getApi().sendMessage(userId, "Пример:\n\"/sub_go ИСП-322р\"", false, 0, NULL);

                    }
                    else if (message->text == "Подписаться на расписание преподавателя") {

                        bot.getApi().sendMessage(userId, "Пример:\n\"/sub_p Ананьин Е.М.\"", false, 0, NULL);
                    }
                    else if (message->text == "Подписаться на общее расписание") {
                        bot.getApi().sendMessage(userId, "Используйте команду /sub_o", false, 0, NULL);
                    }
                    else if (text == "Состояние подписки") {
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
                    else if (message->text == "Посмотреть расписание") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, getRaspisKeyboard);
                    }
                    else if (message->text == "Назад") {

                        bot.getApi().sendMessage(userId, "Переход выполнен", false, 0, mainMenuKeyboard);
                    }
                    else if (message->text == "Посмотреть общее расписание") {

                        for (auto& corp : rb::corpss) {
                            for (auto& mPage : corp.pages) {
                                if (mPage.isEmpty)
                                    continue;

                                bot.getApi().sendPhoto(userId,
                                    TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));
                            }
                        }
                    }
                    else if (message->text == "Посмотреть расписание преподавателя") {
                        bot.getApi().sendMessage(userId, "Пример:\n\"/get_p Ананьин Е.М.\"", false, 0, NULL);
                    }
                    else if (message->text == "Посмотреть расписание группы") {

                        bot.getApi().sendMessage(userId, "Пример:\n\"/get_g ИСП-322р\"", false, 0, NULL);
                    }
                    else if (message->text == "Получать расписание и для другого корпуса") {

                        bot.getApi().sendMessage(userId, "Используйте команду /t_dop", false, 0, NULL);
                    }
                    else if (message->text == "Подписаться ещё на одно расписание") {

                        bot.getApi().sendMessage(userId, "Используйте команду /m", false, 0, NULL);
                    }
                    else if (message->text == "Отписаться от одного из расписаний") {

                        bot.getApi().sendMessage(userId, "Используйте команду /unm", false, 0, NULL);
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

                            for (auto& corp : rb::corpss) {
                                for (auto& mPage : corp.pages) {
                                    if (mPage.isEmpty)
                                        continue;

                                    bot.getApi().sendPhoto(userId,
                                        TgBot::InputFile::fromFile(rb::imgPath + mPage.folderName + ".png", "image/png"));
                                }
                            }
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
                    if(isStandartMessage != 0)
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