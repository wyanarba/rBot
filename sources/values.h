#pragma once
using namespace std;


extern const std::string CurrentVersion;
extern const std::string Version;
struct corps;



namespace sync {
    extern int8_t SyncMode;
    //переменная для синхронизации, 0 - свободный режим, 1 - ожидание остановки потока бота,
    // 2 - подтверждение остановки потока бота, 3 - отправка расписания

    extern mutex mtx1, mtxForLog;//да)


    extern bool IsUpdate;
    extern bool IsChangeYear;

    extern int AttemptsToCheck;// 1 10
    extern int AttemptsToCheck2;// 1 30

    extern string NewVersion;
    extern int  CurrentYear;
}

namespace cfg {
    extern bool ModeSend;//режим отправки 0 - v1, 1 - v2
    extern void (*update)();//функция для отправки расписания, (указатель) на неё

    extern vector<int64_t> GroupsForSpam;//буферные группы для рассылки

    extern int64_t RootTgId;//тг id владельца
    extern int64_t SecondRootTgId;//мой тг id, для прав чуть по ниже

    extern string BotKey;//ключ бота
    extern string StartText;//приветственное сообщение

    extern const DWORD SleepTime;
    extern bool EnableAd;
    extern bool EnableAutoUpdate;
}

namespace rb {
    extern const int countBuilds;// кол-во корпусов
    extern const int pagesInBui;// макс страниц в корпусе
    extern const string imgPath;// путь до папок с расписанием

    extern bool ErrorOnCore;// верно ли обработано расписание
    extern int currentCorps;//на какой корпус рассылать расписание
    

    extern set <string> AllTeachers;// перечисление всех учителей
    extern set <string> SpamText;// исключённые слова, как фамилии преподавателей (скорее всего устарело)
    extern vector<bool> DisabledGroups;// группы без подписчиков

    extern vector<string> Groups;
    extern vector<string> Groups1251;
    extern vector<corps> corpss; // ну типа корпуса

    extern bool EnableMLog;// Доп. логирование
}



struct myGroup
{
    bool isExists = 0;//наличие в папке
    bool changed = 0;// изменилось ли расписание группы (не новый файл)
    int idSpam;// номер буферной группы
    int32_t messageId = 0;//id сообщения
    int32_t messageIdS = 0;//id сообщения с мини расписанием
    string ps, psS;// Картинки с расписанием

    myGroup(const bool& isExists) : isExists(isExists) {}

    myGroup& operator=(bool value) {
        isExists = value;
        return *this;
    }
};

// страница расписания
struct pageRasp {
    string folderName;// имя папки страницы
    bool IsNewPage = 0;// новая страница или обновление старой
    bool isEmpty = 1;// пуста ли папка
    int32_t mi = 0;//id сообщения
    string ps;// Картинка с расписанием

    vector<myGroup>groups;// группы
    set <string> Teachers;// преподаватели

    pageRasp(string folderName) {
        this->folderName = folderName;
        groups.assign(rb::Groups.size(), 0);
    }

    void clear() {
        Teachers.clear();
        groups.clear();
        groups.assign(rb::Groups.size(), 0);
        isEmpty = 1;
        IsNewPage = 0;
    }
};

//корпус
struct corps
{
    string pdfFileName;// имя файла пдф
    int localOffset;// смещение этого корпуса
    string LastFileD;// файл pdf из локальной папки

    vector<pageRasp> pages;
    int pagesUse = 0;// сколько сейчас используется страниц

    corps(string pdfFileName, int localOffset) {
        this->localOffset = localOffset;// порядковый номер корпуса
        this->pdfFileName = pdfFileName;

        for (int i = 0; i < rb::pagesInBui; i++) {
            pages.push_back(pageRasp(to_string(i + localOffset * rb::pagesInBui)));
        }
    }
};