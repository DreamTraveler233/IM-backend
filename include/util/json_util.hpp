#ifndef __CIM_UTIL_JSON_UTIL_HPP__
#define __CIM_UTIL_JSON_UTIL_HPP__

#include <jsoncpp/json/json.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace CIM {
class JsonUtil {
   public:
    static bool NeedEscape(const std::string& v);
    static std::string Escape(const std::string& v);
    static std::string GetString(const Json::Value& json, const std::string& name,
                                 const std::string& default_value = "");
    static double GetDouble(const Json::Value& json, const std::string& name,
                            double default_value = 0);
    static int32_t GetInt32(const Json::Value& json, const std::string& name,
                            int32_t default_value = 0);
    static uint32_t GetUint32(const Json::Value& json, const std::string& name,
                              uint32_t default_value = 0);
    static int64_t GetInt64(const Json::Value& json, const std::string& name,
                            int64_t default_value = 0);
    static uint64_t GetUint64(const Json::Value& json, const std::string& name,
                              uint64_t default_value = 0);
    static int16_t GetInt16(const Json::Value& json, const std::string& name,
                            int16_t default_value = 0);
    static uint16_t GetUint16(const Json::Value& json, const std::string& name,
                              uint16_t default_value = 0);
    static int8_t GetInt8(const Json::Value& json, const std::string& name,
                          int8_t default_value = 0);
    static uint8_t GetUint8(const Json::Value& json, const std::string& name,
                            uint8_t default_value = 0);
    static bool FromString(Json::Value& json, const std::string& v);
    static std::string ToString(const Json::Value& json);
};

}  // namespace CIM

#endif