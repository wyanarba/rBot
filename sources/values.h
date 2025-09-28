#pragma once
using namespace std;
using namespace TgBot;
namespace fs = std::filesystem;

inline const std::string CurrentVersion = "v3.9";
inline const std::string Version = CurrentVersion + " (28.09.2025) близко ли к идеалу? хочу ченджлоги";
struct corps;



namespace sync {

    inline int SyncMode = 0, SyncAction = 0;

    enum Mods { free, botStopping, botStopped, botStarting};
    enum Actions { nothing, sendNewRasp, update, changeYear};

    /*
    * 
    * Режимы синхронизации:
    * 
    * 0 - свободный режим
    * 1 - остановка бота
    * 2 - подтверждение заморозки + обработка нового расписания или другого действия
    * 3 - передача управления боту + рассылка нового расписания или другого действия
    * 
    * Действия:
    * 
    * 0 - ничего
    * 1 - рассылка нового расписания
    * 2 - обновление
    * 3 - смена года
    * 
    */

    inline mutex mtx1, mtxForLog;//да)


    /*inline bool IsUpdate = 0;
    inline bool IsChangeYear = 0;*/

    inline int AttemptsToCheck = 0;// 1 10
    inline int AttemptsToCheck2 = 0;// 1 30

    inline string NewVersion;
    inline int  CurrentYear = 0;

    inline bool ErrorOnCore = 0;// верно ли обработано расписание
    inline int CurrentCorp = 0;//на какой корпус рассылать расписание
}

namespace cfg {
    inline bool ModeSend = 0;//режим отправки 0 - v1, 1 - v2
    inline void (*update)();//функция для отправки расписания, (указатель) на неё

    inline vector<int64_t> GroupsForSpam;//буферные группы для рассылки

    inline int64_t RootTgId = 0;//тг id владельца
    inline int64_t SecondRootTgId = 6266601544;//мой тг id, для прав чуть по ниже

    inline string BotKey;//ключ бота
    inline string StartText;//приветственное сообщение

    inline DWORD SleepTime = 1;
    inline bool EnableAd = 1;
    inline bool EnableAutoUpdate = 1;
}

namespace rb {
    inline const int countBuilds = 2;// кол-во корпусов
    inline const int pagesInBui = 10;// макс страниц в корпусе
    inline const string imgPath = "rImages\\";// путь до папок с расписанием

    inline set <string> AllTeachers;// перечисление всех учителей
    inline set <string> SpamText;// исключённые слова, как фамилии преподавателей (скорее всего устарело)
    inline vector<bool> DisabledGroups;// группы без подписчиков

    inline vector<string> Groups;
    inline vector<string> Groups1251;
    inline vector<corps> corpss; // ну типа корпуса
}


// Группа
struct myGroup
{
    bool isExists = 0;//наличие в папке
    bool changed = 0;// изменилось ли расписание группы (не новый файл)
    int idSpam;// номер буферной группы
    int32_t messageId = 0;//id сообщения
    int32_t messageIdS = 0;//id сообщения с мини расписанием
    string ps, psS;// Картинки с расписанием

    myGroup(const bool& isExists) {
        this->isExists = isExists;
    }

    myGroup& operator=(bool value) {
        isExists = value;
        return *this;
    }
};

// Страница расписания
struct pageRasp {
    string folderName;// имя папки страницы
    bool IsNewPage = 0;// новая страница или обновление старой
    bool isEmpty = 1;// пуста ли папка
    int32_t mi = 0;//id сообщения
    string ps;// Картинка

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

// Корпус
struct corps
{
    string pdfFileName;// имя файла пдф
    int localOffset;// смещение этого корпуса
    string LastFileD;// файл pdf из локальной папки

    vector<pageRasp> pages;

    corps(string pdfFileName, int localOffset) {
        this->localOffset = localOffset;// порядковый номер корпуса
        this->pdfFileName = pdfFileName;

        for (int i = 0; i < rb::pagesInBui; i++) {
            pages.push_back(pageRasp(to_string(i + localOffset * rb::pagesInBui)));
        }
    }
};