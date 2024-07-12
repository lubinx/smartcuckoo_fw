
-----------------------------------------------------------------------------
# vol 音量控制
    vol 100                         设置 100% 音量
    vol 90                          设置 90% 音量

-----------------------------------------------------------------------------
# dim 显示亮度设置
    dim                             返回当前亮度 (20 / 40 / 60 / 80 / 100) %
    dim [20 / 40 / 60 / 80 / 100]   设置亮度

-----------------------------------------------------------------------------
# dt 设置/返回 日期时间

## 命令:
    dt                              返回日期时钟
    dt yyyy/mm/dd hh:nn:ss          返回日期时钟

## 日期格式:
    yyyy/mm/dd hh:nn:ss

-----------------------------------------------------------------------------
# loc 设置语言

## 命令:
    loc                             返回语言列表
    loc [id/lcid]                   设置语言, id 或 lcid

## 语言列表格式:
    {"voice_id": 0,
        "locales": [
        {"id":0, "lcid":"en-AU", "voice":"Chloe"},
        {"id":4, "lcid":"en-US", "voice":"Ava"},
        {"id":5, "lcid":"en-US", "voice":"Ethan"}
    ]}

## 示例:
    loc 4                           设置英文-US
    loc en                          设置英文, 第一个
    loc en-US                       设置英文 en-US

-----------------------------------------------------------------------------
# hfmt 设置时间格式
    hfmt 0                          语言默认
    hfmt 12                         12小时制
    hfmt 24                         24t小时制

-----------------------------------------------------------------------------
# alm 设置返回闹钟

# 命令
    alm
    alm <id> <enable/disable>
    alm <id> <enable/disable> <mtime> <ringtone_id> [mdate=yyyymmdd] [wdays=%x]

## 闹钟格式
    {"alarms": [
            {"id":1, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":2, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":3, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":4, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":5, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":6, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":7, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0},
            {"id":8, "enabled":false, "mtime":0, "ringtone_id":0, "mdate":0, "wdays":0}
    ]}

# 示例
    alm                             返回闹钟设置
    alm 1 disable                   禁止闹钟
    alm 1 enable 1700 0 wdays=0x7F  设置每天下午5点
    alm 1 enable 1700 0 wdays=0x3E  设置工作日(周一至周五) 下午5点

-----------------------------------------------------------------------------
# lamp 夜灯控制
    {"colors": [
        {"id": 0, "R": 50, "G": 50, "B": 50}
    ]}

## 命令
    lamp                            返回色表
    lamp on / off                   开关夜灯
    lamp [id] [20~100]              开关夜灯

-----------------------------------------------------------------------------
# env 读取温湿度
    env                             读取温湿度
    env fahrenheit / celsius        设置温度单位

## 返回结果
    tmpr=10.0 °F / °C
    humidity=50

-----------------------------------------------------------------------------
# nois 播放白噪音
    nois                            列表白噪音主题
    nois [on/off]                   开关白噪音
    nois [next/prev]                上一首/下一首白噪音
    nois id                         播放指定主题

## 白噪音主题格式

    {"colors": [
        {"id": 0, “theme": 主题}
    ]}
