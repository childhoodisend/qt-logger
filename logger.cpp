#include "logger.h"

#include <QTime>
#include <QFileInfo>
#include <QSettings>
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
#include <QTextCodec>
#endif

//! Namespace for Graphical Workstation of the 6G_DIRA_3D project
namespace DIRA_3D_GW {

    Logger::Logger(): m_rootFolder("")
                    , m_fileName("")
                    , m_level(LoggerLevel::Warning)
                    , m_maxFilesSizeInBytes(-1)
                    , m_maxFilesCount(-1)
    {}

    Logger::~Logger() {
        m_awake_to_exit = true;
        m_is_writing = false;
        m_cv.notify_one();

        if (m_writerThread.joinable()) {
            m_writerThread.join();
        }
    }

    bool Logger::init(const QString &dir,
                      const QString &fileName,
                      LoggerLevel level,
                      std::int64_t maxFileSize,
                      std::int32_t maxFilesCount) {

        if (dir.isEmpty()) {
            return false;
        }

        m_rootFolder = dir;
        m_fileName = fileName;
        m_level = level;
        m_maxFilesSizeInBytes = maxFileSize;
        m_maxFilesCount = maxFilesCount;

        m_is_writing = !this->m_fileName.isEmpty();
        if (m_is_writing) {
            m_writerThread = std::thread(&Logger::write_action, this);
        }
        return true;
    }

    void Logger::write_action() {
        m_cur_dir = QDir(m_rootFolder);
        if (!m_cur_dir.exists()) {
            qWarning("Cannot find the %s. Directory will be created", qPrintable(m_rootFolder));
            m_cur_dir.mkpath(".");
        }

        m_cur_file.setFileName(m_cur_dir.filePath(m_fileName));
        if (!m_cur_file.open(QIODevice::ReadWrite | QIODevice::Append)) {
            qWarning("Cannot create the file %s", qPrintable(m_cur_file.fileName()));
        }

        while (m_is_writing) {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (!m_ready) {
                m_cv.wait(lock);
                if (m_awake_to_exit) {
                    return;
                }
            }

            QString cur;
            while (dequeueItem(cur))
            {
                if (isFileMaxSize()) {
                    backupActiveFile();
                }
                m_cur_file.write(cur.toStdString().c_str());
                m_cur_file.flush();
            }

            m_ready = false;
        }
    }

    void Logger::addQueueItem(const QString& item) {
        // Блокировка асинхронного доступа к очереди вынесена на уровень
        // выше, так как получение текущего времени потоко не безопасно
        this->m_queue.push_back(item);
        m_ready = true;
        m_cv.notify_one();
    }

    bool Logger::dequeueItem(QString& dst) {
        if (!m_queue.isEmpty()) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            dst = m_queue.takeFirst();
            return true;
        }
        return false;
    }

    QString Logger::format_msg(const QString &strLevel,
                               const QString &message,
                               const QString &sourceFile,
                               std::int32_t sourceLine) {
        if (!sourceFile.isEmpty()) {
            if (sourceLine != -1)
                return QString("%1 [%2]: %3 [%4 (%5)]\n")
                        .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"))
                        .arg(strLevel).arg(message).arg(sourceFile).arg(sourceLine);

            return QString("%1 [%2]: %3 [%4]\n")
                        .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"))
                        .arg(strLevel).arg(message).arg(sourceFile);
        }

        if (sourceLine != -1)
            return QString("%1 [%2]: %3 (%4)\n")
                        .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"))
                        .arg(strLevel).arg(message).arg(sourceLine);

        return QString("%1 [%2]: %3\n")
                        .arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss"))
                        .arg(strLevel).arg(message);
    }

    LoggerLevel Logger::LoggerLevel_form_str(const QString& level) {
        std::map<QString, LoggerLevel> logMap = {{"SYSTEM", System}, {"CRITICAL", Critical},
                                                 {"ERROR", Error}, {"WARNING", Warning},
                                                 {"INFO", Info}, {"DEBUG", Debug},
                                                 {"DEVELOPER", Developer}};
        QString upperLevel = level.toUpper();
        if (logMap.find(upperLevel) != logMap.end()) {
            return logMap[upperLevel];
        }
        return Warning;
    }

    int64_t Logger::MaxLogFileSize_to_int(const QString& size){
        if (size.isEmpty()) {
            return -1;
        }

        std::map<QString, int64_t> m_size = {{"Kb", 1024ULL},
                                             {"Mb", 1024ULL*1024ULL},
                                             {"Gb", 1024ULL*1024ULL*1024ULL},
                                             {"Tb", 1024ULL*1024ULL*1024ULL*1024ULL},
                                             {"kb", 1024ULL},
                                             {"mb", 1024ULL*1024ULL},
                                             {"gb", 1024ULL*1024ULL*1024ULL},
                                             {"tb", 1024ULL*1024ULL*1024ULL*1024ULL},
                                             {"KB", 1024ULL},
                                             {"MB", 1024ULL*1024ULL},
                                             {"GB", 1024ULL*1024ULL*1024ULL},
                                             {"TB", 1024ULL*1024ULL*1024ULL*1024ULL}};

        for (const auto& s : m_size) {
            const auto& pos = size.indexOf(s.first);
            if (pos != -1) {
                const int64_t num = size.left(pos).toInt() * s.second;
                return num;
            }
        }

        for (const auto& c : size) {
            if (c.isLetter()) {
                return -1;
            }
        }

        return size.toInt();
    }

    bool Logger::initFromConfig(const QString &file, const QString &section) {
        if (!QFile::exists(file)) {
            return false;
        }

        QSettings sett(file, QSettings::IniFormat);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        QTextCodec *codec = QTextCodec::codecForName("UTF-8");
        sett.setIniCodec(codec);
#endif
        sett.beginGroup(section);
        this->m_rootFolder = sett.value("LogFolder", "").toString();
        this->m_fileName = sett.value("LogFileName", "").toString();

        if (this->m_rootFolder.isEmpty()) {
            return false;
        }

        const QString log_l = sett.value("LogLevel", "System").toString();
        this->m_level = LoggerLevel_form_str(log_l);

        const QString size = sett.value("MaxLogFileSize", "").toString();
        this->m_maxFilesSizeInBytes = MaxLogFileSize_to_int(size);

        this->m_maxFilesCount =  sett.value("MaxFilesCount", -1).toInt();

        m_is_writing = !this->m_fileName.isEmpty();
        if (m_is_writing) {
            m_writerThread = std::thread(&Logger::write_action, this);
        }

        return true;
    }

    void Logger::system(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::System && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("System", message, sourceFile, sourceLine));
        }
    }

    void Logger::critical(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::Critical && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("Critical", message, sourceFile, sourceLine));
        }
    }

    void Logger::error(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::Error && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("Error", message, sourceFile, sourceLine));
        }
    }

    void Logger::warning(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::Warning && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("Warning", message, sourceFile, sourceLine));
        }
    }

    void Logger::info(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::Info && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("Info", message, sourceFile, sourceLine));
        }
    }

    void Logger::debug(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::Debug && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("Debug", message, sourceFile, sourceLine));
        }
    }

    void Logger::dev(const QString &message, const QString &sourceFile, std::int32_t sourceLine) {
        if (this->m_level >= LoggerLevel::Developer && m_is_writing) {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            this->addQueueItem(format_msg("Developer", message, sourceFile, sourceLine));
        }
    }

    bool Logger::isDeveloper() const    {   return m_level == LoggerLevel::Developer;   }
    bool Logger::isDebug() const        {   return m_level >= LoggerLevel::Debug;       }
    bool Logger::isInfo() const         {   return m_level >= LoggerLevel::Info;        }
    bool Logger::isWarning() const      {   return m_level >= LoggerLevel::Warning;     }

    bool Logger::isFileMaxSize() const {
        static const qint64 diff = 80;
        return m_maxFilesSizeInBytes != -1 && m_cur_file.size() - diff >= m_maxFilesSizeInBytes;
    }

    void Logger::backupActiveFile() {
        if (m_maxFilesCount == -1) {
            QFile backup(m_cur_file.fileName() + "_backup");

            if (backup.exists())
                backup.remove();

            // copy from backup to m_cur_file
            if(m_cur_file.copy(backup.fileName())) {
                backup.setPermissions(backup.permissions() |
                                    QFileDevice::WriteOwner |
                                    QFileDevice::WriteUser |
                                    QFileDevice::WriteGroup |
                                    QFileDevice::WriteOther);

                if (!backup.open(QIODevice::ReadOnly|QIODevice::Unbuffered|QIODevice::Text)) {
                    qWarning("Cannot create the file %s", (const char *) backup.fileName().data());
                }
                const auto size = (qint64)backup.size() / 4;
                backup.seek(size);

                const auto str = backup.readLine();
                const int pos = (int)str.indexOf("\n");
                backup.seek(size + pos + 1);

                if (!m_cur_file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
                    qWarning("Cannot create the file %s", (const char *) m_cur_file.fileName().data());
                }
                // write current size to m_cur_file
                const auto size_to_write = backup.size() - backup.pos();
                m_cur_file.write((const char*) backup.map(backup.pos(), size_to_write), size_to_write);
                m_cur_file.flush();
                backup.close();
                std::thread remover = std::thread([&](QString fn)
                                        {
                                           QFile f(fn);
                                           while (f.exists())
                                                f.remove();
                                        }, m_cur_file.fileName() + "_backup");

                if (remover.joinable())
                    remover.detach();
            }
            else {
                qWarning("Cannot create the file %s", (const char *) backup.fileName().data());
            }
        } else {
            // check m_maxFilesCount
            QStringList nameFilter;
            nameFilter << QString("%1_*.log").arg(QFileInfo(m_cur_file).baseName());
            QFileInfoList files = m_cur_dir.entryInfoList(nameFilter,QDir::Files | QDir::NoDotAndDotDot,QDir::Time | QDir::Reversed);
            while (m_maxFilesCount - 1 < files.size()) {
                // clean last files
                const QFileInfo &info = files.takeFirst();
                QFile::remove(info.absoluteFilePath());
                files.pop_front();
            }

            // rename m_cur_file
            QString new_file_name = QFileInfo(m_cur_file).baseName()
                                     + QDateTime::currentDateTime().toString("_ddMMyyyy_hhmmss_zzz.log");
            while (m_cur_dir.exists(m_cur_dir.filePath(new_file_name))) {
                new_file_name = QFileInfo(m_cur_file).baseName()
                                 + QDateTime::currentDateTime().toString("_ddMMyyyy_hhmmss_zzz.log");
            }

            m_cur_file.close();
            m_cur_file.rename(m_cur_dir.filePath(new_file_name));

            // create new file m_fileName
            m_cur_file.setFileName(m_cur_dir.filePath(m_fileName));
            if (!m_cur_file.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
                qWarning("Cannot create the file %s", (const char *) m_cur_file.fileName().data());
            }
        }
    }

}   // End namespace DIRA_3D_GW
