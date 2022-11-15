/********************************************************************
* Copyright (C) 2022 by Elektron Research and Production Company
* ALL RIGHTS RESERVED.
*********************************************************************
*
* Project: Graphic Workstation (6G_DIRA_3D)
*
* File Name: modules/Logger/logger.h
*
* Description: Файл с описание экспортируемого интерфейса модуля
*           Logger.
*               Модуль предназначен для ведения журнала работы.
*               Для работы модуля необходимо его инициализировать.
*           Если имя файла журнала не задано - журнал НЕ ВЕДЁТСЯ.
*           Если каталог для файла журнала не задан - журнал ведётся
*           в текущем каталоге.
*
* Notes: Any remarks or cautions
*
* History:
*	20 Sep 2022	Igor Popovich: Created
*
********************************************************************/
#ifndef LOGGER_H
#define LOGGER_H

#include <QtCore/qglobal.h>

#if defined(LOGGER_LIB_LIBRARY)
#  define LOGGER_EXPORT Q_DECL_EXPORT
#else
#  define LOGGER_EXPORT Q_DECL_IMPORT
#endif

#include <QString>
#include <QQueue>
#include <QDir>
#include <QFile>

#include <thread>
#include <memory>
#include <condition_variable>

#include "loggertypes.h"

//! Namespace for Graphical Workstation of the 6G_DIRA_3D project
namespace DIRA_3D_GW {

/*! \class Экспортируемый класс объекта ведения журнала Logger.
 *  \brief Экспортирует интерфейс для работы с объектом ведения журнала работы
 * модуля или приложения.
 *     Объект Logger позволяет настроить место, имя файла журнала и уровень ведения
 * журнала, а так же максимальный размеру файла фурнала и количество хранимых файлов
 * журналов.
 *     Объект можно инициализировать только оджин раз (либо параметрами, либо из файла
 * конфигурации). После установки параметров объект начинает процесс ведения журнала.
 *     Запись данных в файл журнала выполняется в фоновом потоке, а все методы по
 * добавлению записей в жернал - потокобезопасны.
 */
    class LOGGER_EXPORT Logger {
    public:
        /**
          * @brief Конструктор по умолчанию.
          */
        Logger();

        /**
          * @brief Деструктор
          * @remark Ожидает завершение потока записи и освобождает все ресурсы
          * используемые объектом.
          */
        virtual ~Logger();

        /**
         * @brief Инициализация объекта ведения журнала
         * @remarks Инициализирует все свойства объекта указанными значениями. Те
         * свойства значения которых не указаны - инициализируются значениями по
         * умолчанию.
         *     При необходимости создаётся каталог хранения файлов журнала и сам
         * файл журнала. Запускается поток записи файла.
         *
         * @param dir Каталог хранения файлов журнала
         * @param fileName Имя файла журнала
         * @param level Уровень сообщений попадающих в журнал
         * @param maxFileSize Максимальный размер файла журнала
         * @param maxFilesCount Количество хранимых (предыдущих) файлов журнала
         * @return true - если все параметры установлены корректно и объект готов к
         * записи журнала или false - в случае ошибок
         * @see LoggerLevel
         */
        bool init(const QString &dir,
                  const QString &fileName,
                  LoggerLevel level = LoggerLevel::Warning,
                  std::int64_t maxFileSize = -1,
                  std::int32_t maxFilesCount = -1);

        /**
         * @brief Инициализация свойств модуля из файла конфигурации
         * @remarks Читает все свойства относящиеся к ведению журнала из указанной
         * секции файла конфигурации.
         *     Если все необходимые параметры прочитаны из файла, выполняется
         * инициализация модуля и его запуск.
         *
         * @param file Имя (полный путь) файла конфигурации
         * @param section Название секции в файле
         * @return true - если инициализация прошла успешно или false - в случае
         * ошибок
         */
        bool initFromConfig(const QString &file, const QString &section);

        /**
         * @brief Регистрация системного сообщения для записи в журнал
         * @remarks Системные сообщения пишутся в журнал всегда (вне зависимости от
         * уровня ведения журнала).
         *     Имя файла исходного кода и номер строки в файле исходного кода - опциональные
         * параметры для всех уровней кроме ERROR и CRITICAL. Эти данные пишутся в файл
         * журнала для возможности идентифицировать в дальнейшем место ошибки.
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void system(const QString &message,
                    const QString &sourceFile = QString(""),
                    std::int32_t sourceLine = -1);

        /**
         * @brief Регистрация критического сообщения для записи в журнал
         * @remarks К уровню критических сообщений относятся все ситуации связанные
         * с потерей данных или способные привести к остановке работы системы или модуля.
         *     При добавлении критической ошибки в журнале обязательно должны быть
         * зафиксированы название файла исходного кода и номер строки в нём для участка
         * на котором возникла ошибка. Для всех ошибо такого типа в файле журнала после
         * текста сообщения будет добавлено, например: (main.cpp [10])
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void critical(const QString &message,
                      const QString &sourceFile,
                      std::int32_t sourceLine);

        /**
         * @brief Регистрация сообщения об ошибке для записи в журнал
         * @remarks К этому уровню сообщений относятся все сообщения которые возникают
         * из-за системных или логических ошибок в системе. Все исключения - это
         * ошибки.
         *     При добавлении ошибки в журнале обязательно должны быть зафиксированы
         * название файла исходного кода и номер строки в нём для участка на котором
         * возникла ошибка.  Для всех ошибо такого типа в файле журнала после
         * текста сообщения будет добавлено, например: (main.cpp [10])
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void error(const QString &message,
                   const QString &sourceFile,
                   std::int32_t sourceLine);

        /**
         * @brief Регистрация предупреждения для записи в журнал
         * @remarks К предупреждениям относятся все ситуации, которые влияют на работу модуля
         * или системы в целом. Если какая-то операция не может быть выполнена из-за отсутствия
         * данных или части данны, то это необходимо регистрировать как предупреждение.
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void warning(const QString &message,
                     const QString &sourceFile = QString(""),
                     std::int32_t sourceLine = -1);

        /**
         * @brief Регистрация информационного сообшения для записи в журнал
         * @remarks Информационные сообщения регистрируют все ситуации которые не относятся
         * к предупреждениям, но имеют более высокую ценность чем отладочная информация.
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void info(const QString &message,
                  const QString &sourceFile = QString(""),
                  std::int32_t sourceLine = -1);

        /**
         * @brief Регистрация отладочного сообшения для записи в журнал
         * @remarks Отладочными сообщениями считается вся информация которая представляет
         * интерес для службы поддержки или внедрения и необходимая для передачи проблемы
         * в разработку. Так же на этом уровне могут регистрироваться dump`ы отправляемых
         * или получаемых сообщений или DICOM dataset.
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void debug(const QString &message,
                   const QString &sourceFile = QString(""),
                   std::int32_t sourceLine = -1);

        /**
         * @brief Регистрация служебных сообшения на уровне разработки для записи в журнал
         * @remarks Служебные сообщения должны писаться только для отладочных версии модулей
         * (DEBUG). На этом уровне в журнал может писаться любая информация необходимая для
         * разработки и/или отладки модуля.
         *     Эти сообщения не должны выводится в Release сборках, даже если они включены.
         *
         * @param message Текст сообщения
         * @param sourceFile Имя файла исходного кода откуда иницирована запись сообщения
         * @param sourceLine Номер строки кода.
         */
        void dev(const QString &message,
                 const QString &sourceFile = QString(""),
                 std::int32_t sourceLine = -1);

    private:
        /**
         *
         */
        void write_action();
        /**
         *
         * @param str
         * @param message
         * @param sourceFile
         * @param sourceLine
         * @return
         */
        QString
        format_msg(const QString &str, const QString &message, const QString &sourceFile = QString(""), std::int32_t sourceLine = -1);
        /**
         *
         * @param item
         */
        void addQueueItem(const QString& item);
        /**
         *
         * @param dst
         * @return
         */
        bool dequeueItem(QString& dst);

        /**
         *
         * @param level
         * @return
         */
        LoggerLevel LoggerLevel_form_str(const QString& level);

        /**
         *
         * @param size
         * @return
         */
        int64_t MaxLogFileSize_to_int(const QString& size);

        /**
         * @brief Проверка размера файла на предмет достижения максимального размера
         * @remarks Проверяет размер файла журнала и если он близок к максимальному
         * ((m_maxFilesSizeInBytes +/- 80) байт), тогда выполняется сохранение копии
         * файла журнала, а модуль начинает вести журнал в новый файл.
         *
         * @return true если файл журнала достиг максимального размера или false - в
         * противном случае.
         * @private
         */
        bool isFileMaxSize() const;

        /**
         * @brief Сохрание текущего журнала и подготовка к продолжению ведения жернала.
         * @remarks Если количество хранимых файлов журналов установлено и не равно 0,
         * текущий файл сохраняется с новым именем (при этом контролируется количество
         * хранимых файлов), а ведение журнала начинается в новом файле с именем файла
         * журнала.
         *     Если же количество хранимых файлов журнала установлено и равно 0, то
         * необходимо удалить ~30-35% записей в начале журнала и продолжить ведение журнала
         *
         * @private
         */
        void backupActiveFile();

    private:
        QString m_rootFolder;       ///< Каталог в котором хранится файл журнала
        QString m_fileName;         ///< Имя файла журнала
        LoggerLevel m_level;        ///< Текущий уровень логгирования

        std::int64_t m_maxFilesSizeInBytes;   ///< Максимальный размер файла журнала
        std::int32_t m_maxFilesCount;         ///< Количество хранящихся файлов журнала

        std::mutex m_mutex;
        std::mutex m_queue_mutex;
        QQueue<QString> m_queue;        ///< Очередь сообщений для записи в файл журнала

        bool m_ready = false;
        bool m_is_writing = false;
        std::thread m_writerThread;     ///< Поток осуществляющий запись сообщений из очереди в файл
        std::condition_variable m_cv;   ///< Объект синхронизации для запуска потока записи из режима ожидания


        QDir m_cur_dir;
        QFile m_cur_file;

        bool m_awake_to_exit = false;
    };

    typedef std::shared_ptr<Logger> LoggerPtr;

}   // End namespace DIRA_3D_GW

#endif // LOGGER_H
