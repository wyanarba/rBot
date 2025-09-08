#include <freetype/freetype.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <opencv2/opencv.hpp>

#include "pch.h"

#include "raspisCore.h"
#include "functions.h"
#include "values.h"



using namespace cv;
using namespace poppler;


struct myCoord
{
    float x, y;
    string name;

    myCoord(const float& x, const float& y, const string& name) : x(x), y(y), name(name) {}
};


const string CurrentVersion = "v3.81";
const string Version = CurrentVersion + " (04.09.2024) скоро уже год?";

string MainUrl = "https://rasp.vksit.ru/";
const char UpdateCommand[17] = "start update.bat";
string FileDownloaded;//файлы .pdf с расписанием
int CutsOffX = 0, CutsOffY = 0, LeftEdge = 0;


// Скачивание файла с сайта
static bool DownloadFileToMemory(const std::string& url, std::string& fileContent) {
    HINTERNET hInternet = InternetOpen("File Downloader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) {
        logMessage("Failed to initialize InternetOpen.", "system", 201);
        return false;
    }

    // Генерация уникального параметра для предотвращения кеширования
    std::stringstream ss;
    ss << url << "?t=" << std::time(nullptr);  // Добавляем текущее время к URL
    std::string fullUrl = ss.str();

    // Открываем URL с уникальным параметром
    HINTERNET hFile = InternetOpenUrl(hInternet, fullUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hFile) {
        logMessage("Failed to open URL.", "system", 202);
        InternetCloseHandle(hInternet);
        return false;
    }

    std::ostringstream contentStream;
    char buffer[4096];
    DWORD bytesRead;
    while (InternetReadFile(hFile, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        contentStream.write(buffer, bytesRead);
    }

    fileContent = contentStream.str();

    InternetCloseHandle(hFile);
    InternetCloseHandle(hInternet);

    return true;
}

// Запись строк (файлов .pdf) в файлы
static bool WriteStringToFile(const std::string& content, const std::string& filePath) {
    std::ofstream outFile(filePath, std::ios::binary);
    if (!outFile.is_open()) {
        logMessage("Failed to open file for writing.", "system", 203);
        return false;
    }
    outFile.write(content.data(), content.size());
    outFile.close();
    return true;
}

// Чтение строк (файлов .pdf) из файлов
static bool ReadStringFromFile(const std::string& filePath, std::string& content) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) {
        logMessage("Failed to open file for reading.", "system", 204);
        return false;
    }
    std::ostringstream contentStream;
    contentStream << inFile.rdbuf();
    content = contentStream.str();
    inFile.close();
    return true;
}

// Получение количества страниц в документе
static int getPDFPageCount(const std::string& filePath) {
    // Загружаем документ
    poppler::document* doc = poppler::document::load_from_file(filePath);

    if (!doc) {
        //errorExcept("Не удалось открыть PDF файл: " + filePath, 0);
        return -1;
    }

    // Получаем количество страниц
    int numPages = doc->pages();

    // Освобождаем память
    delete doc;

    return numPages;
}

// Рисование текста
static void drawTextFT(cv::Mat& img, const std::string& aa, const std::string& fontPath, int fontSize, int x_center, int y_center) {
    // Конвертация из Windows-1251 в wstring
    int size_needed = MultiByteToWideChar(1251, 0, aa.c_str(), -1, nullptr, 0);
    std::wstring text(size_needed, 0);
    MultiByteToWideChar(1251, 0, aa.c_str(), -1, &text[0], size_needed);
    text.pop_back(); // удаляем null-terminator

    // Инициализация FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "FT_Init_FreeType failed\n";
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "FT_New_Face failed\n";
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Предварительный расчёт общей ширины и максимальной высоты текста
    int text_width = 0;
    int text_height = 0;
    int max_top = 0;
    int max_bottom = 0;

    for (wchar_t wc : text) {
        if (FT_Load_Char(face, wc, FT_LOAD_RENDER)) continue;

        text_width += face->glyph->advance.x >> 6;

        int top = face->glyph->bitmap_top;
        int bottom = face->glyph->bitmap.rows - face->glyph->bitmap_top;

        if (top > max_top) max_top = top;
        if (bottom > max_bottom) max_bottom = bottom;
    }

    text_height = max_top + max_bottom;

    // Начальные координаты для центровки
    int pen_x = x_center - text_width / 2;
    int pen_y = y_center + max_top / 2;

    // Отрисовка
    for (wchar_t wc : text) {
        if (FT_Load_Char(face, wc, FT_LOAD_RENDER)) {
            std::wcerr << L"Failed to load char: " << wc << L"\n";
            continue;
        }

        FT_GlyphSlot g = face->glyph;
        int w = g->bitmap.width;
        int h = g->bitmap.rows;
        int top = g->bitmap_top;
        int left = g->bitmap_left;

        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                int x_img = pen_x + left + col;
                int y_img = pen_y - top + row;
                if (x_img >= 0 && y_img >= 0 && x_img < img.cols && y_img < img.rows) {
                    uchar alpha = g->bitmap.buffer[row * g->bitmap.pitch + col];
                    if (alpha > 0) {
                        cv::Vec3b& pixel = img.at<cv::Vec3b>(y_img, x_img);
                        for (int c = 0; c < 3; ++c) {
                            pixel[c] = static_cast<uchar>(
                                pixel[c] * (255 - alpha) / 255
                                );
                        }
                    }
                }
            }
        }

        pen_x += g->advance.x >> 6;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}



static void postRaspis() {
    sync::mtx1.lock();//отправляем сообщение боту и ожидаем завершения отправки расписания
    sync::SyncMode = 3;
    sync::mtx1.unlock();

    bool wait = 1;

    while (wait) {
        this_thread::sleep_for(300ms);
        sync::mtx1.lock();
        wait = sync::SyncMode != 0;
        sync::mtx1.unlock();
    }
}

static int findMajorityElement(const vector<int>& numbers) {
    map<int, int> frequency;
    for (int i = 0; i < numbers.size() - 1; i++) {
        frequency[numbers[i + 1] - numbers[i]]++;
    }

    int majorityElement = numbers[0];
    int maxFrequency = 0;
    for (const auto& entry : frequency) {
        if (entry.second > maxFrequency) {
            maxFrequency = entry.second;
            majorityElement = entry.first;
        }
    }
    return majorityElement;
}

static std::vector<int> filterVector(const std::vector<int>& input, double thresholdFactor = 1) {

    // Подсчёт частоты каждого числа
    std::map<int, int> frequency;
    for (int num : input) {
        frequency[num]++;
    }

    // Вычисление среднего количества повторений
    double averageFrequency = std::accumulate(frequency.begin(), frequency.end(), 0.0,
        [](double sum, const std::pair<int, int>& pair) {
            return sum + pair.second;
        }) / frequency.size();

    // Рассчитаем порог для фильтрации
    double threshold = averageFrequency * thresholdFactor;

    // Формируем новый вектор: удаляем редкие значения и оставляем уникальные
    std::vector<int> result;
    for (const auto& [value, count] : frequency) {
        if (count >= threshold) {
            result.push_back(value);
        }
    }

    return result;
}



static void editRaspis(string filePath) {
    Mat image = imread(filePath);;
    int& imageHeight = image.rows, imageWidth = image.cols;
    int startX = imageWidth + 1, startY = imageHeight + 1, endX = 0, endY = 0;
    Vec3b pixel;
    int margin, marginTop;//отступ по краям
    int wLine = 50;//ширина левой линии

    //поиск startX
    for (int i = 0; i < imageHeight; i++) {// x - j, y - i
        for (int j = 0; j < imageWidth; j++) {

            if (startX > j) {
                pixel = image.at<Vec3b>(i, j); // получение цвета пикселя

                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    startX = j;
                    wLine = 50;
                    break;
                }
            }
            else
                break;
        }

        if (startX < imageWidth) {
            pixel = image.at<Vec3b>(i, startX);

            if (pixel[0] < 50) {
                for (int k = 0; k < wLine; k++) {

                    pixel = image.at<Vec3b>(i, startX + k); // получение цвета пикселя

                    if (pixel[0] > 50) {
                        wLine = k;
                    }
                }
            }
        }
    }

    //поиск startY
    for (int i = 0; i < imageWidth; i++) {// x - i, y - j
        for (int j = 0; j < imageHeight; j++) {
            if (startY > j) {
                pixel = image.at<Vec3b>(j, i); // получение цвета пикселя
                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    startY = j;
                    break;
                }
            }
            else
                break;
        }
    }

    //поиск endX
    for (int y = imageHeight - 1; y > 0; y--) {// x - j, y - i
        for (int x = imageWidth - 1; x > 0; x--) {
            if (x > endX) {
                pixel = image.at<Vec3b>(y, x); // получение цвета пикселя
                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    endX = x;
                    break;
                }
            }
            else
                break;
        }
    }

    //поиск endY
    for (int i = imageWidth - 1; i > 0; i--) {// x - i, y - j
        for (int j = imageHeight - 1; j > 0; j--) {
            if (endY < j) {
                pixel = image.at<Vec3b>(j, i); // получение цвета пикселя
                if (pixel[0] < 50 && pixel[1] < 50 && pixel[2] < 50) {
                    endY = j;
                    break;
                }
            }
            else
                break;
        }
    }

    if (wLine < 3 || wLine > 40)
        wLine = 7;

    margin = wLine * 1.7;
    marginTop = margin * 2;


    cv::Rect roi(startX - margin, startY - marginTop, endX - startX + 2 * margin, endY - startY + margin + marginTop); // x, y, width, height
    cv::imwrite(filePath, image(roi)); // Сохранение изображения

    CutsOffX = startX - margin;
    CutsOffY = startY - marginTop;
    LeftEdge = margin;
}

static void getLocalRaspis(pageRasp& mPage, string pdf_path, int pageNum) {

    string imageName = rb::imgPath + mPage.folderName + ".png", folderToSave = rb::imgPath + mPage.folderName + "\\";

    set <string> lastGroups;//преподаватели и изменённые группы
    map <string, Mat>teachers;//картинки преподов
    vector <myCoord> t2;//имена и отчества для преподавателей
    Vec3b pixel;//пиксель изображения
    Mat image, imageCoper, imageCoper2;//картинки
    vector <int> xDots[4], yDots[4], extremDotsI;// 0 - верхние левые, 1 - нижние правые, 2 - не сортированные точки, 3 - временные
    const float coeff = 5.5563, frCoeff = 0.8;//коэффициент для перевода координат pdf в пиксели изображения5.5563
    bool isNewFile = 1;//полностью ли изменилось расписание
    unique_ptr<document> doc(document::load_from_file(pdf_path));// Загружаем PDF-документ
    poppler::page* page = NULL;
    page = doc->create_page(pageNum);
    Mat timeImage, corpsNumImage;
    string corpsNum = "err";

    int padding = 0;//отступы в маленькой версии картинки
    int padding1 = 0;//отступы между датой в маленькой версии картинки

    if (!doc) {
        logMessage("Ошибка загрузки PDF.", "system");
        return;
    }
    if (!page) {
        logMessage("Ошибка открытия страницы ", "system");
    }

    //извлечение текста с координатами
    auto text_boxes = page->text_list();


    image = imread(imageName); // загрузка изображения
    image.copyTo(imageCoper);
    image.copyTo(imageCoper2);



    //поиск перекрёстных точек
    {
        int radCheck = 0, radMerge = 5;//радиусы для поиска перекрёстных точек
        int xOff = 0, y0 = 0, y1 = 0;//смещения и кол-во точек(для ключевого y)

        //поиск шапки таблицы (она обычно не меняется)
        for (int y = 0; y < image.rows; y++) {
            if (image.at<Vec3b>(y, LeftEdge)[0] < 100) {//чёрный пиксель

                //проверка линии по x
                bool b1 = 1;

                while (b1 && xOff < LeftEdge * 3) {
                    xOff++;
                    b1 = image.at<Vec3b>(y, LeftEdge + xOff)[0] < 100;
                }

                if (xOff == LeftEdge * 3) {
                    if (yDots[2].size() > 0 && y - yDots[2][yDots[2].size() - 1] > radMerge)
                        y1++;

                    if (y1 == 2) {
                        y1 = y;
                        y = image.rows;
                    }

                    yDots[2].push_back(y);
                }

                xOff = 0;
            }
        }

        y0 = yDots[2][0];
        radCheck = y1 - y0;
        yDots[2].clear();


        //сам поиск
        for (int x = LeftEdge; x < image.cols; x++) {
            bool isNormalLine = 1;

            for (int y = y0; y < y1; y++) {//проверка на нормальную линию
                isNormalLine = image.at<Vec3b>(y, x)[0] < 100;
                if (!isNormalLine) {
                    y = y1;
                }
            }

            if (!isNormalLine)
                continue;

            for (int y = y1; y < image.rows; y++) {

                if (image.at<Vec3b>(y, x)[0] < 100) {//чёрный пиксель

                    bool b1 = 1, b2 = 1;

                    while ((b1 || b2) && xOff < radCheck) {//проверка по x
                        xOff++;

                        if (b1) {
                            b1 = image.at<Vec3b>(y, x + xOff)[0] < 100;
                        }

                        if (b2) {
                            b2 = image.at<Vec3b>(y, x - xOff)[0] < 100;
                        }
                    }

                    if (xOff == radCheck) {
                        xDots[2].push_back(x);
                        yDots[2].push_back(y);
                        //imageCoper.at<Vec3b>(y, x) = cv::Vec3b(0, 255, 0);
                    }

                    xOff = 0;
                }
            }
        }

        //фильтрация результата
        sort(xDots[2].begin(), xDots[2].end());
        sort(yDots[2].begin(), yDots[2].end());


        for (int i = 0; i < xDots[2].size() - 1; i++) {
            xDots[3].push_back(xDots[2][i]);

            if (xDots[0].size() == 0)
                xDots[0].push_back(xDots[2][i]);

            else if (xDots[2][i] - xDots[2][i - 1] > radMerge) {//первая
                xDots[0].push_back(xDots[2][i]);
            }

            if (xDots[2][i + 1] - xDots[2][i] > radMerge) {//последняя
                xDots[3] = filterVector(xDots[3], 0.7);//70% отличие от среднего

                if (xDots[3][0] != xDots[0][xDots[0].size() - 1]) {
                    xDots[0].pop_back();
                    xDots[0].push_back(xDots[3][0]);
                }

                xDots[1].push_back(xDots[3][xDots[3].size() - 1]);
                xDots[3].clear();
            }

        }

        for (int i = 0; i < yDots[2].size() - 1; i++) {
            yDots[3].push_back(yDots[2][i]);

            if (yDots[0].size() == 0)
                yDots[0].push_back(yDots[2][i]);

            else if (yDots[2][i] - yDots[2][i - 1] > radMerge) {
                yDots[0].push_back(yDots[2][i]);
            }

            if (yDots[2][i + 1] - yDots[2][i] > radMerge) {
                yDots[3] = filterVector(yDots[3], 0.7);//70% отличие от среднего

                if (yDots[3][0] != yDots[0][yDots[0].size() - 1]) {
                    yDots[0].pop_back();
                    yDots[0].push_back(yDots[3][0]);
                }

                yDots[1].push_back(yDots[3][yDots[3].size() - 1]);
                yDots[3].clear();
            }

        }

        xDots[1].push_back(xDots[2][xDots[2].size() - 1]);
        yDots[1].push_back(yDots[2][yDots[2].size() - 1]);

        /*for (int i = 0; i < xDots[0].size(); i++) {
            for (int x = xDots[0][i]; x <= xDots[1][i]; x++) {
                for (int j = 0; j < yDots[0].size(); j++) {
                    for (int y = yDots[0][j]; y <= yDots[1][j]; y++) {
                        imageCoper.at<Vec3b>(y, x) = cv::Vec3b(0, 0, 255);
                    }
                }
            }
        }*/

    }

    //костыльчик
    {}

    //поиск крайних точек
    {
        int majority = findMajorityElement(yDots[0]);
        double threshold = majority * 0.8;  // Порог, на 20% меньше большинство

        for (int i = 0; i < yDots[0].size() - 1; i++) {
            if (yDots[0][i + 1] - yDots[0][i] < threshold) {
                extremDotsI.push_back(i);
            }
        }

        extremDotsI.push_back(yDots[0].size() - 1);
    }


    //проверка на изменение файла, полностью новое расписание или не большое изменение расписания
    {
        Mat mat1 = imread(folderToSave + "coper.png");
        Mat mat2 = image(cv::Rect(0, 0, image.cols, yDots[0][0]));

        if (!mat1.empty()) {
            mat1 = mat1(cv::Rect(0, 0, mat1.cols, yDots[0][0]));

            if (mat1.size() == mat2.size()) {
                // Разделяем оба изображения на каналы
                std::vector<cv::Mat> channels1, channels2;
                cv::split(mat1, channels1);
                cv::split(mat2, channels2);

                // Сравниваем каналы
                if (cv::countNonZero(channels1[0] != channels2[0]) != 0) {
                    isNewFile = false;
                }
            }
            else
                isNewFile = 0;
        }
        else
            isNewFile = 0;

        isNewFile = !isNewFile;

        if (isNewFile)
            logMessage("Полностью новое", "system");
        else
            logMessage("Не большое изменение", "system");

        mPage.IsNewPage = isNewFile;


        if (isNewFile) {
            folderToSave.substr(folderToSave.size() - 2, 2);
            try {
                for (const auto& entry : std::filesystem::directory_iterator(folderToSave)) {
                    if (entry.is_regular_file()) {
                        std::filesystem::remove(entry.path());
                    }
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                logMessage(e.what(), "system");
            }

        }
        else {
            string currentFilee;
            for (const auto& entry : fs::directory_iterator(folderToSave)) {
                currentFilee = cp1251_to_utf8(entry.path().filename().string().c_str());
                currentFilee = currentFilee.erase(currentFilee.size() - 4);

                if (currentFilee[currentFilee.size() - 1] == 'S') {
                    lastGroups.insert(currentFilee);
                }
            }
        }
    }


    //подготовка к разделению
    {
        Mat overlay;
        imageCoper2.copyTo(overlay);


        for (int dwaoijawp = 0; dwaoijawp < text_boxes.size(); dwaoijawp++) {
            const auto& box = text_boxes[dwaoijawp];

            byte_array byte_arr = text_boxes[dwaoijawp].text().to_utf8();

            string text(byte_arr.data(), byte_arr.size()), text1251 = Utf8_to_cp1251(text.c_str());

            auto dotCord = text.find_last_of(".");//время
            auto dotDopCord = text.find(".");//время
            bool timeFinded = 0, corpFinded = 0;

            //для номера корпуса
            if (!corpFinded && text == "КОРПУС") {
                byte_array byte_arr2 = text_boxes[dwaoijawp - 1].text().to_utf8();
                string text2(byte_arr2.data(), byte_arr2.size());

                corpsNum = text2;
                corpFinded = 1;
            }

            //для преподавателей
            if (text.size() > 5 && text1251[0] > -65 && text1251[0] < -32 && dotCord == 5 && dotDopCord == 2) {

                float x1 = box.bbox().x();
                float y1 = box.bbox().y();
                float x2 = box.bbox().right();
                float y2 = box.bbox().bottom();
                const int padding = 3;//отступы в маленькой версии картинки

                x1 = floor(x1 * coeff) - CutsOffX;
                y1 = floor(y1 * coeff) - CutsOffY;
                x2 = floor(x2 * coeff) - CutsOffX;
                y2 = floor(y2 * coeff) - CutsOffY;

                Rect roi2(x1 - padding, y1 - padding, (x2 - x1 + 2 * padding) / text1251.size() * 4, y2 - y1 + 2 * padding);  // x, y, ширина, высота
                cv::rectangle(overlay, roi2, { 255, 0, 0 }, cv::FILLED);

                text.erase(6);
                text1251.erase(4);

                t2.push_back({ static_cast<float>(box.bbox().x()),
                    static_cast<float>(box.bbox().y()),
                    text });
            }

            //для времени
            if (!timeFinded && dwaoijawp + 2 < text_boxes.size() && dotCord != string::npos && dotDopCord != dotCord && text1251.size() == 10) {
                const auto& nbox = text_boxes[dwaoijawp + 2];


                bool isDate = 1;
                for (char& ch : text1251) {
                    if ((ch < 48 || ch > 57) && ch != 46) {
                        isDate = 0;
                        timeFinded = 1;
                    }
                }

                if (isDate) {

                    float x1 = box.bbox().x();
                    float y1 = box.bbox().y();
                    float x2 = nbox.bbox().right();
                    float y2 = nbox.bbox().bottom();
                    const int padding = 3;//отступы в маленькой версии картинки

                    x1 = floor(x1 * coeff) - CutsOffX;
                    y1 = floor(y1 * coeff) - CutsOffY;
                    x2 = floor(x2 * coeff) - CutsOffX;
                    y2 = floor(y2 * coeff) - CutsOffY;


                    Rect roi2(x1 - padding, y1 - padding, x2 - x1 + 2 * padding, y2 - y1 + 2 * padding);  // x, y, ширина, высота
                    cv::rectangle(overlay, roi2, { 0, 255, 0 }, cv::FILLED);


                    timeImage = image(roi2);
                    timeFinded = 1;
                }
            }
        }

        cv::addWeighted(overlay, 0.60, imageCoper2, 1 - 0.60, 0, imageCoper2);

        //динамические отступы
        if (xDots[1][0] - xDots[0][0] < 3 || xDots[1][0] - xDots[0][0] > 40) {
            padding = 7;
            padding1 = 7;
        }
        else {
            padding = (xDots[1][0] - xDots[0][0]) * 0.8;
            padding1 = (xDots[1][0] - xDots[0][0]) * 2;
        }

        //номер корпуса
        int fsize = timeImage.rows * frCoeff;
        corpsNumImage = cv::Mat(timeImage.rows, timeImage.cols, CV_8UC3, cv::Scalar(255, 255, 255));
        drawTextFT(corpsNumImage, Utf8_to_cp1251((corpsNum + " корпус").c_str()), "times.ttf", fsize, timeImage.cols / 2, timeImage.rows / 2);
    }

    //костыльчик #2
    {}

    //разделение
    {
        Mat overlay2, overlay3;
        imageCoper2.copyTo(overlay2), imageCoper.copyTo(overlay3);



        for (int dwaoijawp = 0; dwaoijawp < text_boxes.size(); dwaoijawp++) {
            const auto& box = text_boxes[dwaoijawp];
            byte_array byte_arr = text_boxes[dwaoijawp].text().to_utf8();
            string text(byte_arr.data(), byte_arr.size()), text1251 = Utf8_to_cp1251(text.c_str());

            //cout << "Текст: '" << text << "'\t с координатами: x1=" << 1 << ", y1=" << 2 << endl;

            auto dashCord = text.find("-");//поиск всех групп
            auto dotCord = text.find_last_of(".");//поиск всех групп
            auto dotDopCord = text.find(".");//поиск всех групп

            //для групп
            if ((dashCord != 0 && dashCord + 2 < text.size()) && dashCord != string::npos && (text.at(dashCord - 1) < '0' || text.at(dashCord - 1) > '9') && (text.at(dashCord + 1) >= '0' || text.at(dashCord + 1) <= '9')) {
                // Печать всего текста для проверки
                float x1 = box.bbox().x();
                float y1 = box.bbox().y();
                float y2 = box.bbox().bottom();
                int extremDot = 0, startDotXI = 0, startDotYI = 0;

                //cout << "Текст: '" << text << "'\t с координатами: x1=" << x1 << ", y1=" << y1 << endl;

                //image.at<Vec3b>(, ) = cv::Vec3b(0, 0, 255);

                x1 = floor(x1 * coeff) - CutsOffX;
                y1 = floor(y1 * coeff) - CutsOffY;
                y2 = floor(y2 * coeff) - CutsOffY;

                y1 += (y2 - y1) / 2;

                //выбор подходящей крайней точки (точка у группы)
                for (int i = 0; i < extremDotsI.size(); i++) {
                    if (y1 < yDots[0][extremDotsI[i]]) {
                        extremDot = extremDotsI[i];
                        i = extremDotsI.size();
                    }
                }

                //выбор подходящей x точки
                for (int i = 0; i < xDots[0].size(); i++) {
                    if (x1 <= xDots[0][i]) {
                        startDotXI = i;
                        i = xDots[0].size();
                    }
                }

                //выбор подходящей y точки
                for (int i = 0; i < yDots[0].size(); i++) {
                    if (y1 <= yDots[0][i]) {
                        startDotYI = i;
                        i = yDots[0].size();
                    }
                }



                if (extremDot == 0 || startDotXI == 0 || startDotYI == 0) {
                    logMessage(std::format("ошибка 1 {}, {}, {}, {}", extremDot, startDotXI, startDotYI, text), "system");
                    continue;
                }

                //для зеленого выделения
                Rect roi2(xDots[1][startDotXI - 1] + 1, yDots[1][startDotYI - 1] + 1, xDots[0][startDotXI] - xDots[1][startDotXI - 1] - 1, yDots[0][extremDot] - yDots[1][startDotYI - 1] - 1);  // x, y, ширина, высота

                Rect roi(xDots[1][startDotXI - 1], yDots[0][startDotYI - 1], xDots[1][startDotXI] - xDots[1][startDotXI - 1], yDots[1][extremDot] - yDots[0][startDotYI - 1]);//пары
                Rect roi1(xDots[0][0], yDots[0][startDotYI - 1], xDots[1][3] - xDots[0][0], yDots[1][extremDot] - yDots[0][startDotYI - 1]);//время


                Mat cropped = image(roi1), cropped1 = image(roi);//группа, звонки
                Size newSize(max(cropped.cols + cropped1.cols, timeImage.cols) + 2 * padding, cropped.rows + timeImage.rows * 2 + 2 * (padding + padding1));

                Mat result(newSize, image.type(), Scalar(255, 255, 255)); // Для цветного изображения

                cropped.copyTo(result(Rect(padding, timeImage.rows * 2 + padding1 * 2 + padding, cropped.cols, cropped.rows)));
                cropped1.copyTo(result(Rect(cropped.cols + padding, timeImage.rows * 2 + padding1 * 2 + padding, cropped1.cols, cropped1.rows)));
                corpsNumImage.copyTo(result(Rect((result.cols - timeImage.cols) / 2 + padding, 0, timeImage.cols, timeImage.rows)));
                timeImage.copyTo(result(Rect((result.cols - timeImage.cols) / 2 + padding, corpsNumImage.rows, timeImage.cols, timeImage.rows)));



                Mat tempImage;//картинка с группой для сохранения
                image.copyTo(tempImage);
                Mat overlay;

                tempImage.copyTo(overlay);
                cv::rectangle(overlay, roi2, { 0, 255, 0 }, cv::FILLED);
                cv::addWeighted(overlay, 0.25, tempImage, 1 - 0.25, 0, tempImage);


                cv::rectangle(overlay3, roi2, { 0, 255, 0 }, cv::FILLED);


                if (!isNewFile) {

                    if (lastGroups.find(text + 'S') != lastGroups.end()) {
                        Mat sImageLast = imread(folderToSave + Utf8_to_cp1251(text.c_str()) + "S.png");

                        // Разделяем оба изображения на каналы
                        std::vector<cv::Mat> channels1, channels2;
                        cv::split(result, channels1);
                        cv::split(sImageLast, channels2);

                        // Сравниваем каждый канал
                        if (result.size() != sImageLast.size() || cv::countNonZero(channels1[0] != channels2[0]) != 0) {
                            int groupId = findGroup(text);
                            if (groupId != -1) {
                                mPage.groups[groupId].changed = 1;
                            }
                            else {
                                logMessage("Неожиданная группа " + text, "system");
                            }
                        }
                    }
                    else {
                        int groupId = findGroup(text);
                        if (groupId != -1) {
                            mPage.groups[groupId].changed = 1;
                        }
                        else {
                            logMessage("Неожиданная группа " + text, "system");
                        }
                    }
                }

                if (!cv::imwrite(folderToSave + Utf8_to_cp1251(text.c_str()) + "S.png", result))
                    logMessage("Не удалось записать файл " + text, "system");

                if (cv::imwrite(folderToSave + Utf8_to_cp1251(text.c_str()) + ".png", tempImage)) {
                    int groupId = findGroup(text);
                    if (groupId != -1) {
                        mPage.groups[groupId] = 1;
                    }
                    else {
                        logMessage("Неожиданная группа " + text, "system");
                    }
                }
                else
                    logMessage("Не удалось записать файл " + text, "system");
            }

            //для преподавателей
            if (text.size() > 6 && text1251[0] > -65 && text1251[0] < -32 && (text1251[1] < -64 || text1251[1] > -33) && rb::SpamText.find(text) == rb::SpamText.end()) {
                // Печать всего текста для проверки
                float x1 = box.bbox().x();
                float x2 = box.bbox().right();
                float y1 = box.bbox().y();
                float y2 = box.bbox().bottom();
                int extremDot = 0, startDotXI = 0, startDotYI = 0;
                bool isNormalText = 1;
                float min_distantion = 500000;
                int numT2 = 0;

                if (dotCord != string::npos) {
                    if (text.size() - (dotCord + 1) < 4)
                        continue;

                    int charWidth = (box.bbox().right() - box.bbox().x()) / Utf8_to_cp1251(text.c_str()).size();
                    x1 += 4 * charWidth;

                    text.erase(0, dotCord + 1);
                    text1251 = Utf8_to_cp1251(text.c_str());
                }

                isNormalText = text1251[0] < -32 && text1251[0] > -65;
                for (int i = 1; i < text1251.size(); i++) {
                    isNormalText = isNormalText && text1251[i] < 0 && text1251[i] > -33;//-1 я, -64 А
                }

                if (!isNormalText) {
                    continue;
                }

                for (int i = 0; i < t2.size(); i++) {
                    auto& a = t2[i];
                    float distanation = a.x - x2;

                    if (a.y == y1 && min_distantion > distanation && distanation > 0) {
                        numT2 = i;
                        min_distantion = distanation;
                    }

                }

                if (min_distantion == 500000)
                    continue;

                text += t2[numT2].name;



                if (teachers.find(text) == teachers.end()) {
                    image.copyTo(teachers[text]);
                }


                x1 = floor(x1 * coeff) - CutsOffX;
                y1 = floor(y1 * coeff) - CutsOffY;
                x2 = floor(x2 * coeff) - CutsOffX;
                y2 = floor(y2 * coeff) - CutsOffY;

                //выбор ближайшей x точки
                for (int i = 0; i < xDots[0].size(); i++) {
                    if (x1 <= xDots[0][i]) {
                        startDotXI = i;
                        i = xDots[0].size();
                    }
                }

                //выбор ближайшей y точки
                for (int i = 0; i < yDots[0].size(); i++) {
                    if (y1 <= yDots[0][i]) {
                        startDotYI = i;
                        i = yDots[0].size();
                    }
                }



                if (startDotXI == 0 || startDotYI == 0) {
                    logMessage(std::format("ошибка 2: {}, {}, {}", startDotXI, startDotYI, text), "");
                    continue;
                }


                Rect roi1(xDots[1][0] + 1, yDots[1][startDotYI - 1] + 1, xDots[0][3] - xDots[1][0] - 1, yDots[0][startDotYI] - yDots[1][startDotYI - 1] - 1);
                Rect roi2(xDots[1][startDotXI - 1] + 1, yDots[1][startDotYI - 1] + 1, xDots[0][startDotXI] - xDots[1][startDotXI - 1] - 1, yDots[0][startDotYI] - yDots[1][startDotYI - 1] - 1);  // x, y, ширина, высота
                Rect roi3(x1 - padding, y1 - padding, (x2 - x1 + 2 * padding), y2 - y1 + 2 * padding);  // x, y, ширина, высота

                Mat overlay;
                teachers[text].copyTo(overlay);

                cv::rectangle(overlay, roi2, { 0, 255, 0 }, cv::FILLED);
                cv::rectangle(overlay, roi1, { 0, 255, 0 }, cv::FILLED);
                cv::addWeighted(overlay, 0.25, teachers[text], 1 - 0.25, 0, teachers[text]);
                //teachers[text].at<Vec3b>(y1, x1) = cv::Vec3b(0, 0, 254); // Установка цвета пикселя

                cv::rectangle(overlay2, roi3, { 0, 255, 0 }, cv::FILLED);
            }
        }

        cv::addWeighted(overlay3, 0.20, imageCoper, 1 - 0.20, 0, imageCoper);
        cv::addWeighted(overlay2, 0.50, imageCoper2, 1 - 0.50, 0, imageCoper2);
    }

    //сохранение картинок с преподавателями
    for (auto& teacher : teachers) {
        if (cv::imwrite(folderToSave + Utf8_to_cp1251(teacher.first.c_str()) + ".png", teacher.second))
            mPage.Teachers.insert(teacher.first);
        else {
            logMessage("Не удалось записать файл " + teacher.first, "system");
        }

        if (rb::AllTeachers.find(teacher.first) == rb::AllTeachers.end())
            rb::AllTeachers.insert(teacher.first);
    }

    //запись учителей
    std::ofstream outputFile(rb::imgPath + "4\\t.txt");
    for (const string& tea : rb::AllTeachers) {
        outputFile << tea << '\n';
    }
    outputFile.close();  // Закрываем файл

    ////проверка на наличие изменений
    //if (!isNewFile){
    //    Mat sImageLast = imread(folderToSave + "coper.png");

    //    // Разделяем оба изображения на каналы
    //    std::vector<cv::Mat> channels1, channels2;
    //    cv::split(imageCoper, channels1);
    //    cv::split(sImageLast, channels2);

    //    // Сравниваем каждый канал
    //    if (imageCoper.size() != sImageLast.size() || cv::countNonZero(channels1[0] != channels2[0]) != 0) {
    //        mPage.isEmpty = 0;
    //    }
    //    else
    //        mPage.isEmpty = 1;
    //}

    //добавление рекламы
    if (cfg::EnableAd) {
        Mat adImg = imread("..\\imgs\\ad.png");//   ..\\imgs\\ad.png
        if (adImg.data) {
            Mat overlay;
            int y = yDots[0][0] + (yDots[1][1] - yDots[0][0]) / 2 > adImg.rows + 10 ? yDots[0][0] + (yDots[1][1] - yDots[0][0]) / 2 - adImg.rows : 5;

            image.copyTo(overlay);
            adImg.copyTo(overlay(Rect(xDots[0][xDots[0].size() - 1] - adImg.cols, y, adImg.cols, adImg.rows)));
            cv::addWeighted(overlay, 0.75, image, 1 - 0.75, 0, image);

            cv::imwrite(imageName, image);
        }
    }


    delete page;

    //сохранение мусора ;)
    cv::imwrite(folderToSave + "coper.png", imageCoper);
    cv::imwrite(folderToSave + "coper2.png", imageCoper2);
}

void main2() {
    try
    {
        for (corps& corp : rb::corpss) {
            ReadStringFromFile(corp.pdfFileName, corp.LastFileD);
        }

        while (true) {

            try {
                // Обновление расписания
                for (corps& corp : rb::corpss) {
                    if (!DownloadFileToMemory(MainUrl + corp.pdfFileName, FileDownloaded)) {
                        logMessage("Не удалось скачать файл с расписанием", "system", 120);
                        continue;
                    }

                    if (FileDownloaded != corp.LastFileD) {
                        logMessage("Начало обработки нового расписания, " + to_string(corp.localOffset + 1) + " корпус", "system", 112);

                        WriteStringToFile(FileDownloaded, corp.pdfFileName);
                        corp.LastFileD = FileDownloaded;

                        int pageCount = getPDFPageCount(corp.pdfFileName);

                        if (pageCount > rb::pagesInBui)
                            logMessage("Больше максимума страниц, impossible", "system", 112);

                        for (int i = pageCount + rb::pagesInBui * (corp.localOffset); i < rb::pagesInBui * (corp.localOffset + 1); i++) {

                            for (const auto& entry : std::filesystem::directory_iterator(rb::imgPath + to_string(i))) {
                                if (entry.is_regular_file()) {
                                    std::filesystem::remove(entry.path());
                                }

                            }
                        }



                        //приостанавливаем бота
                        {
                            sync::mtx1.lock();

                            sync::SyncMode = 1;
                            bool wait = 1;

                            sync::mtx1.unlock();

                            while (wait) {
                                this_thread::sleep_for(100ms);
                                sync::mtx1.lock();
                                wait = sync::SyncMode != 2;
                                sync::mtx1.unlock();
                            }
                        }

                        // зачистка переменных корпуса
                        {
                            corp.pagesUse = 0;

                            for (auto& page : corp.pages)
                                page.clear();

                            rb::ErrorOnCore = 0;
                        }

                        for (int i = 0; i < pageCount && i < rb::pagesInBui; i++) {
                            auto& page = corp.pages[i];
                            page.isEmpty = 0;
                            corp.pagesUse++;

                            system(std::format("magick -density 400 {}[{}] -background white -flatten -quality 100 {}.png",
                                corp.pdfFileName, i, rb::imgPath + page.folderName).c_str());

                            //обрезка фото
                            try
                            {
                                editRaspis(rb::imgPath + page.folderName + ".png");
                                getLocalRaspis(page, corp.pdfFileName, i);
                            }
                            catch (const std::exception& e)
                            {
                                logMessage("Какой ужас! Скинь мне это tg: @wyanarba EROR: гет локал распис | " + (string)e.what(), "system", 113);
                                rb::ErrorOnCore = 1;
                            }
                        }

                        //рассылка
                        rb::currentCorps = corp.localOffset;
                        postRaspis();
                        logMessage("Конец обработки нового расписания", "system", 115);
                    }

                }

                // Поиск обновы
                {
                    if (cfg::EnableAutoUpdate && sync::AttemptsToCheck == 0) {//чек обновы

                        sync::AttemptsToCheck++;

                        if (DownloadFileToMemory("https://wyanarba.github.io/rBot/", sync::NewVersion) && sync::NewVersion.size() < 8) {
                            if (sync::NewVersion != CurrentVersion) {
                                logMessage("Обнова!!! " + CurrentVersion + " -> " + sync::NewVersion, "system");

                                sync::IsUpdate = 1;
                                sync::mtx1.lock();//приостанавливаем бота

                                sync::SyncMode = 1;
                                bool wait = 1;

                                sync::mtx1.unlock();

                                while (wait) {
                                    this_thread::sleep_for(100ms);
                                    sync::mtx1.lock();
                                    wait = sync::SyncMode != 2;
                                    sync::mtx1.unlock();
                                }

                                system(UpdateCommand);
                                exit(0);
                            }
                        }
                        else {
                            logMessage("Не удалось скачать версию", "system", 121);
                        }
                    }

                    else if (sync::AttemptsToCheck < 10)
                        sync::AttemptsToCheck++;
                    else if (sync::AttemptsToCheck == 10)
                        sync::AttemptsToCheck = 0;
                }

                // Смена года
                {
                    if (sync::AttemptsToCheck2 == 0) {

                        sync::AttemptsToCheck2++;

                        time_t t = time(nullptr);
                        tm now = {};
                        localtime_s(&now, &t);

                        now.tm_year = (now.tm_year + 1900) % 100;

                        if (now.tm_year > sync::CurrentYear && (now.tm_mon > 6 || (now.tm_mon == 6 && now.tm_mday > 4))) {
                            logMessage("Смена года!!! " + to_string(sync::CurrentYear) + " -> " + to_string(now.tm_year), "system");
                            sync::IsChangeYear = 1;


                            sync::mtx1.lock();// Приостанавливаем бота
                            sync::SyncMode = 1;
                            bool wait = 1;
                            sync::mtx1.unlock();

                            while (wait) {
                                this_thread::sleep_for(100ms);
                                sync::mtx1.lock();
                                wait = sync::SyncMode != 2;
                                sync::mtx1.unlock();
                            }


                            // Сама смена
                            rb::Groups.clear();
                            rb::Groups1251.clear();

                            genGroups();
                            for (int i = 0; i < rb::Groups.size(); i++) {
                                rb::Groups1251.push_back(Utf8_to_cp1251(rb::Groups[i].c_str()));
                            }


                            // Возращение
                            sync::mtx1.lock();
                            sync::SyncMode = 0;
                            sync::mtx1.unlock();
                        }
                    }

                    else if (sync::AttemptsToCheck2 < 30)
                        sync::AttemptsToCheck2++;
                    else if (sync::AttemptsToCheck == 30)
                        sync::AttemptsToCheck2 = 0;
                }
            }
            catch (const std::exception& e)
            {
                logMessage("css) EROR | " + (string)e.what(), "system", 121);
            }


            Sleep(cfg::SleepTime);
        }
    }
    catch (const std::exception& e)
    {
        logMessage("c) EROR | " + (string)e.what(), "system", 121);
    }
}
