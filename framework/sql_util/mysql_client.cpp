#include "mysql_client.h"
#include <glog/logging.h>

namespace sql_util{

bool MySqlClient::init(const std::string &host, const std::string &user, const std::string &password, const std::string &database, int connect_timeout_s){
    host_ = host;
    user_ = user;
    password_ = password;
    database_ = database;
    connect_timeout_s_ = connect_timeout_s;
    return connect();
}

std::unique_ptr<sql::ResultSet> MySqlClient::execute(const std::string &sql_str, int &error_code, uint32_t timeout_ms){
    //尝试重连
    auto ret = execute_interal(sql_str, error_code, timeout_ms);
    if(error_code == 2013){
        if(!connect()){
            return ret;
        }
    }
    return execute_interal(sql_str, error_code, timeout_ms);
}

bool MySqlClient::connect(){
    try {
        driver_ = get_driver_instance();
        sql::ConnectOptionsMap connection_options;
        connection_options["hostName"] = host_;
        connection_options["userName"] = user_;
        connection_options["password"] = password_;
        connection_options["OPT_CONNECT_TIMEOUT"] = connect_timeout_s_;
        con_.reset(driver_->connect(connection_options));
        con_->setSchema(database_);
        stmt_.reset(con_->createStatement());
        stmt_->execute("SET SESSION wait_timeout=600");

    } catch (sql::SQLException &e) {
        LOG(ERROR) << "exception: " << e.what() << " ,code = " << e.getErrorCode() << " ,state = " << e.getSQLState();
        return false;
    }
    return true;
}

std::unique_ptr<sql::ResultSet>  MySqlClient::execute_interal(const std::string &sql_str, int &error_code, uint32_t timeout_ms){
    try {
        stmt_->execute("SET SESSION MAX_EXECUTION_TIME=" + std::to_string(timeout_ms));
        std::unique_ptr<sql::ResultSet> ret;
        ret.reset(stmt_->executeQuery(sql_str));
        error_code = 0;
        return ret;
    } catch (sql::SQLException &e) {
        error_code = e.getErrorCode();
        LOG(ERROR) << "exception: " << e.what() << " ,code = " << e.getErrorCode() << " ,state = " << e.getSQLState();
    }
    return nullptr;
}

void MySqlClient::print_result(sql::ResultSet* res, 
    std::ostream& outputStream, 
    const std::string& delimiter, 
    bool includeHeader) {
    try {
        // 获取元数据
        sql::ResultSetMetaData* meta = res->getMetaData();
        int columnCount = meta->getColumnCount();

        // 打印表头 (列名)
        if (includeHeader) {
            for (int i = 1; i <= columnCount; i++) {
                outputStream << meta->getColumnName(i);
                if (i < columnCount) {
                    outputStream << delimiter;
                }
            }
            outputStream << std::endl;
        }

        // 打印数据行
        while (res->next()) {
            for (int i = 1; i <= columnCount; i++) {
                if (res->isNull(i)) {
                    // 空值处理
                    outputStream << "";
                } else {
                    // 获取字符串值并处理特殊字符
                    std::string value = res->getString(i);
                    // CSV 格式处理: 如果字符串包含分隔符、引号或换行符，需要用引号包围
                    bool needQuotes = (value.find(delimiter) != std::string::npos) || 
                                        (value.find('"') != std::string::npos) || 
                                        (value.find('\n') != std::string::npos);
                
                    if (needQuotes) {
                        outputStream << '"';
                        // 将字符串中的双引号替换为两个双引号 (CSV 转义规则)
                        size_t pos = 0;
                        while ((pos = value.find('"', pos)) != std::string::npos) {
                            value.replace(pos, 1, "\"\"");
                            pos += 2;
                        }
                        outputStream << value;
                        outputStream << '"';
                    } else {
                        outputStream << value;
                    }
                }
                if (i < columnCount) {
                outputStream << delimiter;
                }
            }
            outputStream << std::endl;
        }

    } catch (sql::SQLException &e) {
        LOG(ERROR) << "sql exception: " << e.what() << " ,code = " << e.getErrorCode() << " ,state = " << e.getSQLState();
    } catch (std::exception &e) {
        LOG(ERROR) << "std exception: " << e.what();
    }
}


}; //sql_util
